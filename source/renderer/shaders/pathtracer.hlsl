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
    uint texture_energy_index;
    uint random_seed;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

float4 sample_hdr_env(float3 dir, float2 texture_resolution)
{
    Texture2D<float4> env_texture = get_resource<Texture2D>(cb_in.hdr_env_index);
    uint2 sample_pos = direction_to_equirect_uv(dir) * texture_resolution;
    
    return env_texture[sample_pos];
}

#if RAYTRACING_MODE_SOFTWARE
float3 trace_path(ByteAddressBuffer scene_tlas, float2 pixel_pos, float2 render_dim, uint seed)
#else
float3 trace_path(RaytracingAccelerationStructure scene_tlas, float2 pixel_pos, float2 render_dim, uint seed)
#endif
{
#if RAYTRACING_MODE_SOFTWARE
    ray_t ray = make_primary_ray(pixel_pos, render_dim);
#else
    RayDesc2 ray = make_primary_ray(pixel_pos, render_dim);
#endif
    
    float3 throughput = (float3) 1;
    float3 energy = (float3) 0;
    uint ray_depth = 0;
    
    while (ray_depth <= cb_settings.max_bounces)
    {
        // Prepare hit result and trace TLAS
        hit_result_t hit = make_hit_result();
        trace_ray_tlas(scene_tlas, ray, hit);
        
        // We have missed the scene entirely, so we treat the HDR environment texture as a light source and stop tracing
        if (!has_hit_geometry(hit))
        {
            energy += sample_hdr_env(ray.Direction, float2(cb_in.hdr_env_dimensions)).xyz * throughput;
            break;
        }
        
        ByteAddressBuffer instance_buffer = get_resource<ByteAddressBuffer>(cb_in.instance_buffer_index);
        instance_data_t instance = load_instance(instance_buffer, hit.instance_idx);
        
        ByteAddressBuffer triangle_buffer = get_resource<ByteAddressBuffer>(instance.triangle_buffer_idx);
        triangle_t hit_tri = load_triangle(triangle_buffer, hit.primitive_idx);
        
        float3 hit_pos = ray.Origin + ray.Direction * hit.t;
        float3 hit_normal = triangle_interpolate(hit_tri.v0.normal, hit_tri.v1.normal, hit_tri.v2.normal, hit.bary);
        hit_normal = normalize(mul(float4(hit_normal, 0.0f), instance.local_to_world).xyz);
        
        Texture2D<float4> base_color_texture = get_resource<Texture2D>(instance.material.base_color_index);
        float2 tex_coord = triangle_interpolate(hit_tri.v0.tex_coord, hit_tri.v1.tex_coord, hit_tri.v2.tex_coord, hit.bary);
        energy += base_color_texture.Sample(sampler_linear_wrap, tex_coord).xyz * throughput;
        
        // If the surface is an emissive material, treat it as a light source and stop tracing
        //if (instance.material.emissive)
        //{
        //    energy += instance.material.emissive_color * instance.material.emissive_intensity * throughput;
        //    break;
        //}

        //float r = rand_float(seed);
        
        //// Specular path
        //if (r < instance.material.specular)
        //{
        //    float3 reflect_dir = reflect(ray.Direction, hit_normal);
        //    ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
            
        //    throughput *= instance.material.base_color_factor;
        //}
        //// Refract path
        //else if (r < instance.material.specular + instance.material.refractivity)
        //{
        //    float3 N = hit_normal;
        //    float cosi = clamp(dot(N, ray.Direction), -1.0f, 1.0f);
        //    float etai = 1.0f, etat = instance.material.ior;
            
        //    float Fr = 1.0f;
        //    bool inside = true;
            
        //    // Figure out if we are currently inside or outside the object we have hit
        //    if (cosi < 0.0f)
        //    {
        //        cosi = -cosi;
        //        inside = false;
        //    }
        //    else
        //    {
        //        float temp = etai;
        //        etai = etat;
        //        etat = temp;
        //        N = -N;
        //    }
            
        //    float eta = etai / etat;
        //    float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
            
        //    // Follow either the reflected/specular or refracted path
        //    if (k >= 0.0f)
        //    {
        //        float3 refract_dir = refract(ray.Direction, N, eta, cosi, k);
        //        float cos_in = dot(ray.Direction, hit_normal);
        //        float cos_out = dot(refract_dir, hit_normal);
                
        //        Fr = fresnel(cos_in, cos_out, etai, etat);
                
        //        if (rand_float(seed) > Fr)
        //        {
        //            throughput *= instance.material.albedo;
                    
        //            if (inside)
        //            {
        //                float3 absorption = float3(
        //                    exp(-instance.material.absorption.x * hit.t),
        //                    exp(-instance.material.absorption.y * hit.t),
        //                    exp(-instance.material.absorption.z * hit.t)
        //                );
        //                throughput *= absorption;
        //            }
        //            ray = make_ray(hit_pos + refract_dir * RAY_NUDGE_MULTIPLIER, refract_dir);
        //        }
        //        else
        //        {
        //            float3 reflect_dir = reflect(ray.Direction, hit_normal);
        //            ray = make_ray(hit_pos + reflect_dir * RAY_NUDGE_MULTIPLIER, reflect_dir);
        //            throughput *= instance.material.albedo;
        //        }
        //    }
        //}
        //// Diffuse path
        //else
        //{
        //    float2 random_sample = float2(rand_float(seed), rand_float(seed));
        //    float3 diffuse_dir = (float3) 0;
        //    float hemisphere_pdf = 0.0f;
            
        //    if (cb_settings.cosine_weighted_diffuse)
        //    {
        //        cosine_weighted_hemisphere_sample(hit_normal, random_sample, diffuse_dir, hemisphere_pdf);
        //    }
        //    else
        //    {
        //        uniform_hemisphere_sample(hit_normal, random_sample, diffuse_dir, hemisphere_pdf);
        //    }
            
        //    float3 diffuse_brdf = instance.material.albedo * INV_PI;
        //    float NdotR = max(dot(diffuse_dir, hit_normal), 0.0f);
            
        //    ray = make_ray(hit_pos + diffuse_dir * RAY_NUDGE_MULTIPLIER, diffuse_dir);
        //    throughput *= (NdotR * diffuse_brdf) * (1.0 / hemisphere_pdf);
        //}
        
        ray_depth++;
    }
    
    return energy;
}

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
#if RAYTRACING_MODE_SOFTWARE
    ByteAddressBuffer scene_tlas = get_resource<ByteAddressBuffer>(cb_in.scene_tlas_index);
#else
    RaytracingAccelerationStructure scene_tlas = get_resource<RaytracingAccelerationStructure>(cb_in.scene_tlas_index);
#endif
    
    // Make the primary ray and start tracing
    uint2 pixel_pos = uint2(dispatch_id.xy);
    float3 energy = trace_path(scene_tlas, pixel_pos, cb_view.render_dim, cb_in.random_seed + dispatch_id.y * dispatch_id.x + dispatch_id.x);

    RWTexture2D<float4> buffer_energy = get_resource<RWTexture2D<float4> >(cb_in.texture_energy_index);
    buffer_energy[pixel_pos].xyz = energy;
}
