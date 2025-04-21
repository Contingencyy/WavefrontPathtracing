#include "../common.hlsl"
#include "../sample.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

struct shader_input_t
{
    uint buffer_ray_index;
    uint buffer_intersection_index;
    uint buffer_energy_index;
    uint buffer_throughput_index;
    uint buffer_pixelpos_index;
    uint buffer_instance_index;
    uint texture_hdr_env_index;
    uint texture_hdr_env_width;
    uint texture_hdr_env_height;
    uint random_seed;
    uint recursion_depth;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

bool intersection_result_valid(intersection_result_t intersection_result)
{
    return (
        intersection_result.instance_idx != INSTANCE_IDX_INVALID &&
        intersection_result.primitive_idx != PRIMITIVE_IDX_INVALID
    );
}

float4 sample_hdr_env(float3 dir, float2 texture_resolution)
{
    Texture2D<float4> env_texture = get_resource<Texture2D>(cb_in.texture_hdr_env_index);
    uint2 sample_pos = direction_to_equirect_uv(dir) * texture_resolution;
    
    return env_texture[sample_pos];
}

// TODO: Shade binning to allow for evaluation of different material types?
[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint data_offset = dispatch_id.y * cb_view.render_dim.x + dispatch_id.x;
    
    RWByteAddressBuffer buffer_ray = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_index);
    RayDesc2 ray = buffer_ray.Load<RayDesc2>(data_offset * sizeof(RayDesc2));

    // Temporary
    if (!(length(ray.Direction) > 0.0))
    {
        return;
    }
    
    ByteAddressBuffer buffer_intersection = get_resource<ByteAddressBuffer>(cb_in.buffer_intersection_index);
    intersection_result_t intersection_result = buffer_intersection.Load<intersection_result_t>(data_offset * sizeof(intersection_result_t));
    
    RWByteAddressBuffer buffer_pixelpos = get_resource<RWByteAddressBuffer>(cb_in.buffer_pixelpos_index);
    uint2 pixel_pos = buffer_pixelpos.Load<uint2>(data_offset * sizeof(uint2));
    uint pixel_offset = pixel_pos.y * cb_view.render_dim.x + pixel_pos.x;

    RWByteAddressBuffer buffer_energy = get_resource<RWByteAddressBuffer>(cb_in.buffer_energy_index);
    float3 energy = buffer_energy.Load<float3>(pixel_offset * sizeof(float3));
    
    RWByteAddressBuffer buffer_throughput = get_resource<RWByteAddressBuffer>(cb_in.buffer_throughput_index);
    float3 throughput = buffer_throughput.Load<float3>(pixel_offset * sizeof(float3));

    bool terminate_path = false;
    if (!intersection_result_valid(intersection_result))
    {
        energy += sample_hdr_env(ray.Direction, float2(cb_in.texture_hdr_env_width, cb_in.texture_hdr_env_height)).xyz * throughput;
        terminate_path = true;
    }

    ByteAddressBuffer instance_buffer = get_resource<ByteAddressBuffer>(cb_in.buffer_instance_index);
    instance_data_t instance = load_instance(instance_buffer, intersection_result.instance_idx);
    
    if (instance.material.emissive)
    {
        energy += instance.material.emissive_color * instance.material.emissive_intensity * throughput;
        terminate_path = true;
    }
    
    if (!terminate_path)
    {
        ByteAddressBuffer triangle_buffer = get_resource<ByteAddressBuffer>(instance.triangle_buffer_idx);
        triangle_t hit_tri = load_triangle(triangle_buffer, intersection_result.primitive_idx);
    
        float3 hit_pos = ray.Origin + ray.Direction * intersection_result.t;
        float3 hit_normal = triangle_interpolate(hit_tri.v0.normal, hit_tri.v1.normal, hit_tri.v2.normal, intersection_result.bary);
        hit_normal = normalize(mul(float4(hit_normal, 0.0f), instance.local_to_world).xyz);
        
        uint seed = cb_in.random_seed + dispatch_id.y * dispatch_id.x + dispatch_id.x;
        float r = rand_float(seed);

        // Specular path
        if (r < instance.material.specular)
        {
            float3 reflect_dir = reflect(ray.Direction, hit_normal);
            ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
            
            throughput *= instance.material.albedo;
        }
        // Refract path
        else if (r < instance.material.specular + instance.material.refractivity)
        {
            float3 N = hit_normal;
            float cosi = clamp(dot(N, ray.Direction), -1.0f, 1.0f);
            float etai = 1.0f, etat = instance.material.ior;
            
            float Fr = 1.0f;
            bool inside = true;
            
            // Figure out if we are currently inside or outside the object we have hit
            if (cosi < 0.0f)
            {
                cosi = -cosi;
                inside = false;
            }
            else
            {
                float temp = etai;
                etai = etat;
                etat = temp;
                N = -N;
            }
            
            float eta = etai / etat;
            float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
            
            // Follow either the reflected/specular or refracted path
            if (k >= 0.0f)
            {
                float3 refract_dir = refract(ray.Direction, N, eta, cosi, k);
                float cos_in = dot(ray.Direction, hit_normal);
                float cos_out = dot(refract_dir, hit_normal);
                
                Fr = fresnel(cos_in, cos_out, etai, etat);
                
                if (rand_float(seed) > Fr)
                {
                    throughput *= instance.material.albedo;
                    
                    if (inside)
                    {
                        float3 absorption = float3(
                            exp(-instance.material.absorption.x * intersection_result.t),
                            exp(-instance.material.absorption.y * intersection_result.t),
                            exp(-instance.material.absorption.z * intersection_result.t)
                        );
                        throughput *= absorption;
                    }
                    
                    ray = make_ray(hit_pos + refract_dir * RAY_NUDGE_MULTIPLIER, refract_dir);
                }
                else
                {
                    float3 reflect_dir = reflect(ray.Direction, hit_normal);
                    ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
                    throughput *= instance.material.albedo;
                }
            }
        }
        // Diffuse path
        else
        {
            float2 random_sample = float2(rand_float(seed), rand_float(seed));
            float3 diffuse_dir = (float3) 0;
            float hemisphere_pdf = 0.0f;
            
            if (cb_settings.cosine_weighted_diffuse)
            {
                cosine_weighted_hemisphere_sample(hit_normal, random_sample, diffuse_dir, hemisphere_pdf);
            }
            else
            {
                uniform_hemisphere_sample(hit_normal, random_sample, diffuse_dir, hemisphere_pdf);
            }
            
            float3 diffuse_brdf = instance.material.albedo;
            float NdotR = max(dot(diffuse_dir, hit_normal), 0.0f);
            
            ray = make_ray(hit_pos + diffuse_dir * RAY_NUDGE_MULTIPLIER, diffuse_dir);
            throughput *= (NdotR * diffuse_brdf) / hemisphere_pdf;
        }
    }
    
    // TODO: Change all output buffer writes to an atomic increment except for those that need to keep mapping to pixels 1 to 1
    // TODO: Write ExecuteIndirect arguments here to feed the next recursion iteration from the GPU instead
    // TODO: Simply do not increment atomic counter for next recursion iteration instead of writing a "null ray"
    // Write new ray to output buffer
    if (!terminate_path && cb_in.recursion_depth < cb_settings.ray_max_recursion)
    {
        buffer_ray.Store<RayDesc2>(data_offset * sizeof(RayDesc2), ray);
        buffer_pixelpos.Store<uint2>(data_offset * sizeof(uint2), pixel_pos);
    }
    else if (terminate_path && cb_in.recursion_depth < cb_settings.ray_max_recursion)
    {
        ray = make_ray(float3(0.0, 0.0, 0.0), float3(0.0, 0.0, 0.0));
        buffer_ray.Store<RayDesc2>(data_offset * sizeof(RayDesc2), ray);
    }

    // Write new energy and throughput to output buffers at correct pixel position
    buffer_energy.Store<float3>(pixel_offset * sizeof(float3), energy);
    buffer_throughput.Store<float3>(pixel_offset * sizeof(float3), throughput);
}
