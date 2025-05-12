#include "../common.hlsl"
#include "../sample.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint buffer_ray_index;
    uint buffer_intersection_index;
    uint texture_energy_index;
    uint texture_throughput_index;
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
[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    RWByteAddressBuffer buffer_ray_counts = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_counts_index);
    uint ray_count = buffer_ray_counts.Load<uint>(cb_in.recursion_depth * 4);
    
    // Dispatches might have work that is not divisible by the dispatch thread dimensions, so we skip those
    if (dispatch_id.x >= ray_count)
        return;
    
    RWByteAddressBuffer buffer_ray = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_index);
    RayDesc2 ray = buffer_ray.Load<RayDesc2>(dispatch_id.x * sizeof(RayDesc2));
    
    ByteAddressBuffer buffer_intersection = get_resource<ByteAddressBuffer>(cb_in.buffer_intersection_index);
    intersection_result_t intersection_result = buffer_intersection.Load<intersection_result_t>(dispatch_id.x * sizeof(intersection_result_t));
    
    RWByteAddressBuffer buffer_pixelpos = get_resource<RWByteAddressBuffer>(cb_in.buffer_pixelpos_index);
    uint2 pixel_pos = buffer_pixelpos.Load<uint2>(dispatch_id.x * sizeof(uint2));

    RWTexture2D<float4> texture_energy = get_resource<RWTexture2D<float4> >(cb_in.texture_energy_index);
    float3 energy = texture_energy[pixel_pos].xyz;
    
    RWTexture2D<float4> texture_throughput = get_resource<RWTexture2D<float4> >(cb_in.texture_throughput_index);
    float3 throughput = texture_throughput[pixel_pos].xyz;

    bool terminate_path = false;
    [branch]
    if (!intersection_result_valid(intersection_result))
    {
        energy += sample_hdr_env(ray.Direction, float2(cb_in.texture_hdr_env_width, cb_in.texture_hdr_env_height)).xyz * throughput;
        terminate_path = true;
    }

    ByteAddressBuffer instance_buffer = get_resource<ByteAddressBuffer>(cb_in.buffer_instance_index);
    instance_data_t instance = load_instance(instance_buffer, intersection_result.instance_idx);
    
    [branch]
    if (instance.material.emissive)
    {
        energy += instance.material.emissive_color * instance.material.emissive_intensity * throughput;
        terminate_path = true;
    }

    [branch]
    if (!terminate_path)
    {
        ByteAddressBuffer triangle_buffer = get_resource<ByteAddressBuffer>(instance.triangle_buffer_idx);
        triangle_t hit_tri = load_triangle(triangle_buffer, intersection_result.primitive_idx);
    
        float3 hit_pos = ray.Origin + ray.Direction * intersection_result.t;
        float3 hit_normal = triangle_interpolate(hit_tri.v0.normal, hit_tri.v1.normal, hit_tri.v2.normal, intersection_result.bary);
        hit_normal = normalize(mul(float4(hit_normal, 0.0f), instance.local_to_world).xyz);
        
        uint seed = cb_in.random_seed + dispatch_id.x;
        float r = rand_float(seed);

        // Specular path
        [branch]
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
            
            float3 diffuse_brdf = instance.material.albedo * INV_PI;
            float NdotR = max(dot(diffuse_dir, hit_normal), 0.0f);
            
            ray = make_ray(hit_pos + diffuse_dir * RAY_NUDGE_MULTIPLIER, diffuse_dir);
            throughput *= (NdotR * diffuse_brdf) * (1.0 / hemisphere_pdf);
        }
    }
    
    // Write new ray to output buffer if we need to do another recursion
    [branch]
    if (cb_in.recursion_depth < cb_settings.max_bounces && !terminate_path)
    {
        // Update indirect args for next recursion
        uint write_offset;
        uint ray_count_offset = cb_in.recursion_depth + 1;
        buffer_ray_counts.InterlockedAdd(ray_count_offset * sizeof(uint), 1u, write_offset);

        // Get next ray offset and write new ray
        buffer_ray.Store<RayDesc2>(write_offset * sizeof(RayDesc2), ray);
        buffer_pixelpos.Store<uint2>(write_offset * sizeof(uint2), pixel_pos);
    }

    // Write new energy and throughput to output buffers at correct pixel position
    texture_energy[pixel_pos].xyz = energy;
    texture_throughput[pixel_pos].xyz = throughput;
}
