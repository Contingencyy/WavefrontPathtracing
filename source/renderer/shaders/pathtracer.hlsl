#include "common.hlsl"
#include "intersect.hlsl"
#include "accelstruct.hlsl"
#include "sample.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

struct shader_input_t
{
    uint scene_tlas_index;
    uint hdr_env_index;
    uint2 hdr_env_dimensions;
    uint instance_buffer_index;
    uint rt_index;
    uint sample_count;
    uint random_seed;
};

ConstantBuffer<shader_input_t> g_input : register(b2, space0);

float4 sample_hdr_env(float3 dir, float2 texture_resolution)
{
    Texture2D<float4> env_texture = get_resource<Texture2D>(g_input.hdr_env_index);
    uint2 sample_pos = direction_to_equirect_uv(dir) * texture_resolution;
    
    return env_texture[sample_pos];
}

#if RAYTRACING_MODE_SOFTWARE
float4 trace_path(ByteAddressBuffer scene_tlas, float2 pixel_pos, float2 render_dim, uint seed)
#else
float4 trace_path(RaytracingAccelerationStructure scene_tlas, float2 pixel_pos, float2 render_dim, uint seed)
#endif
{
#if RAYTRACING_MODE_SOFTWARE
    ray_t ray = make_primary_ray(pixel_pos, render_dim);
#else
    RayDesc ray = make_primary_ray(pixel_pos, render_dim);
#endif
    
    float3 throughput = (float3) 1;
    float3 energy = (float3) 0;
    
    uint ray_depth = 0;
    bool specular_ray = false;
    
    while (ray_depth <= cb_render_settings.ray_max_recursion)
    {
        // Prepare hit result and trace TLAS
        hit_result_t hit = make_hit_result();
        trace_ray_tlas(scene_tlas, ray, hit);
        
        // We have missed the scene entirely, so we treat the HDR environment texture as a light source and stop tracing
        if (!has_hit_geometry(hit))
        {
            energy += sample_hdr_env(ray.Direction, float2(g_input.hdr_env_dimensions)).xyz * throughput;
            break;
        }
        
        ByteAddressBuffer instance_buffer = get_resource<ByteAddressBuffer>(g_input.instance_buffer_index);
        instance_data_t instance = load_instance(instance_buffer, hit.instance_idx);
        
        ByteAddressBuffer triangle_buffer = get_resource<ByteAddressBuffer>(instance.triangle_buffer_idx);
        triangle_t hit_tri = load_triangle(triangle_buffer, hit.primitive_idx);
        
        float3 hit_pos = ray.Origin + ray.Direction * hit.t;
        float3 hit_normal = triangle_interpolate(hit_tri.v0.normal, hit_tri.v1.normal, hit_tri.v2.normal, hit.bary);
        hit_normal = normalize(mul(float4(hit_normal, 0.0f), instance.local_to_world).xyz);
        
        // If the surface is an emissive material, treat it as a light source and stop tracing
        if (instance.material.emissive)
        {
            energy += instance.material.emissive_color * instance.material.emissive_intensity * throughput;
            break;
        }

        float r = rand_float(seed);
        
        // Specular path
        if (r < instance.material.specular)
        {
            float3 reflect_dir = reflect(ray.Direction, hit_normal);
            ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
            
            throughput *= instance.material.albedo;
            specular_ray = true;
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
                            exp(-instance.material.absorption.x * hit.t),
                            exp(-instance.material.absorption.y * hit.t),
                            exp(-instance.material.absorption.z * hit.t)
                        );
                        throughput *= absorption;
                    }
                    
                    ray = make_ray(hit_pos + refract_dir * RAY_NUDGE_MULTIPLIER, refract_dir);
                    specular_ray = true;
                }
                else
                {
                    float3 reflect_dir = reflect(ray.Direction, hit_normal);
                    ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
                    
                    throughput *= instance.material.albedo;
                    specular_ray = true;
                }
            }
        }
        // Diffuse path
        else
        {
            float2 random_sample = float2(rand_float(seed), rand_float(seed));
            float3 diffuse_dir = (float3) 0;
            float hemisphere_pdf = 0.0f;
            
            if (cb_render_settings.cosine_weighted_diffuse)
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
            
            specular_ray = false;
        }
        
        ray_depth++;
    }
    
    return float4(energy, 1.0f);
}

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
#if RAYTRACING_MODE_SOFTWARE
    ByteAddressBuffer scene_tlas = get_resource<ByteAddressBuffer>(g_input.scene_tlas_index);
#else
    RaytracingAccelerationStructure scene_tlas = get_resource<RaytracingAccelerationStructure>(g_input.scene_tlas_index);
#endif
    RWTexture2D<float4> color_out = get_resource<RWTexture2D<float4> >(g_input.rt_index);
    
    // Make the primary ray and start tracing
    uint2 pixel_pos = uint2(dispatch_id.xy);
    float4 final_color = trace_path(scene_tlas, pixel_pos, cb_view.render_dim, g_input.random_seed + dispatch_id.y * dispatch_id.x + dispatch_id.x);
    
    float4 color_accum = color_out[pixel_pos];
    float sample_weight = 1.0f / g_input.sample_count;
    
    if (cb_render_settings.accumulate)
    {
        color_out[pixel_pos] = color_accum * (1.0f - sample_weight) + final_color * sample_weight;
    }
    else
    {
        color_out[pixel_pos] = final_color;
    }
}
