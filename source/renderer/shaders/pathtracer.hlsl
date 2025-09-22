#include "common.hlsl"
#include "intersect.hlsl"
#include "accelstruct.hlsl"
#include "sample.hlsl"
#include "material.hlsl"

struct shader_input_t
{
    uint scene_tlas_index;
    uint hdr_env_index;
    uint2 hdr_env_dimensions;
    uint buffer_instance_index;
    uint texture_energy_index;
    uint random_seed;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

float4 sample_hdr_env(float3 dir, float2 tex_dim)
{
    Texture2D<float4> env_texture = get_resource<Texture2D>(cb_in.hdr_env_index);
    uint2 sample_pos = direction_to_equirect_uv(dir) * tex_dim;
    
    return env_texture[sample_pos];
}

#if RAYTRACING_MODE_SOFTWARE
float3 trace_path(ByteAddressBuffer scene_tlas, uint2 dispatch_id, uint2 pixel_pos, float2 render_dim)
#else
float3 trace_path(RaytracingAccelerationStructure scene_tlas, uint2 dispatch_id, uint2 pixel_pos, float2 render_dim)
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
            float3 hdr_env_color = sample_hdr_env(ray.Direction, float2(cb_in.hdr_env_dimensions)).xyz;
            energy += throughput * cb_settings.hdr_env_strength * hdr_env_color;
            break;
        }
        
        hit_surface_t hit_surface = get_hit_surface(hit, cb_in.buffer_instance_index);
        sampled_material_t sampled_material = sample_material(hit_surface.instance.material, hit_surface.tex_coord);

        [branch]
        if (any(sampled_material.emissive_color) > 0.0)
        {
            energy += throughput * sampled_material.emissive_color;
            break;
        }

        uint seed = cb_in.random_seed + dispatch_id.x;
        float r_path = rand_float(seed);

        float3 V = -ray.Direction;
        float3 F0 = float3(0.04, 0.04, 0.04);
        F0 = lerp(F0, sampled_material.base_color, sampled_material.metallic);
        float specular_probability = max(F0.x, max(F0.y, F0.z));
        bool specular_bounce = r_path <= specular_probability;

        /*[branch]
        if (specular_bounce)
        {
            float2 r_spec = float2(rand_float(seed), rand_float(seed));
            
            float3 N = hit_surface.normal;
            float3 L = sample_ggxv_ndf(V, sampled_material.roughness, r_spec.x, r_spec.y);
            float3 H = normalize(V + L);
            
            float NoL = max(0.0, dot(N, L));
            float HoV = max(0.0, dot(H, V));
            
            float G1_NoL = G1_Smith(NoL, sampled_material.roughness);
            float3 F = F_Schlick(HoV, F0);
            throughput *= (G1_NoL * F) / specular_probability;
            ray = make_ray(hit_surface.position, L);
        }
        else*/
        {
            float2 r_diffuse = float2(rand_float(seed), rand_float(seed));
            
            float3 N = hit_surface.normal;
            float3 L = 0;
            float pdf;
            
            if (cb_settings.cosine_weighted_diffuse)
            {
                L = cosine_weighted_hemisphere_sample(N, r_diffuse);
            }
            else
            {
                L = uniform_hemisphere_sample(N, r_diffuse);
            }
            
            float NoL = max(0.0, dot(N, L));

            if (cb_settings.cosine_weighted_diffuse)
            {
                pdf = cosine_weighted_hemisphere_pdf(NoL);
            }
            else
            {
                pdf = uniform_hemisphere_pdf();
            }

            float3 diffuse_brdf = sampled_material.base_color * INV_PI;
            throughput *= (NoL * diffuse_brdf) * (1.0 / pdf);// * (1.0 - specular_probability);
            ray = make_ray(hit_surface.position, L);
        }

        switch (cb_settings.render_view_mode)
        {
        case RENDER_VIEW_MODE_GEOMETRY_INSTANCE:            energy = int_to_color(hit.instance_idx); break;
        case RENDER_VIEW_MODE_GEOMETRY_PRIMITIVE:           energy = int_to_color(hit.primitive_idx); break;
        case RENDER_VIEW_MODE_GEOMETRY_BARYCENTRICS:        energy = float3(hit.bary, 0.0); break;
        case RENDER_VIEW_MODE_GEOMETRY_NORMAL:              energy = abs(interpolate(hit_surface.tri.v0.normal, hit_surface.tri.v1.normal, hit_surface.tri.v2.normal, hit.bary)); break;
        case RENDER_VIEW_MODE_GEOMETRY_UV:                  energy = float3(interpolate(hit_surface.tri.v0.uv, hit_surface.tri.v1.uv, hit_surface.tri.v2.uv, hit.bary), 0.0); break;
        case RENDER_VIEW_MODE_MATERIAL_BASE_COLOR:          energy = sampled_material.base_color; break;
        case RENDER_VIEW_MODE_MATERIAL_NORMAL:              energy = abs(sampled_material.normal); break;
        case RENDER_VIEW_MODE_MATERIAL_METALLIC_ROUGHNESS:  energy = float3(0.0, sampled_material.roughness, sampled_material.metallic); break;
        case RENDER_VIEW_MODE_MATERIAL_EMISSIVE:            energy = sampled_material.emissive_color; break;
        case RENDER_VIEW_MODE_WORLD_NORMAL:                 energy = abs(hit_surface.normal); break;
        case RENDER_VIEW_MODE_RENDER_TARGET_DEPTH:          energy = float3(hit.t, hit.t, hit.t) / cb_view.far_plane; break;
        }

        [BRANCH]
        if (cb_settings.render_view_mode != RENDER_VIEW_MODE_NONE)
        {
            break;
        }
        
        ray_depth++;
    }
    
    return energy;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
#if RAYTRACING_MODE_SOFTWARE
    ByteAddressBuffer scene_tlas = get_resource<ByteAddressBuffer>(cb_in.scene_tlas_index);
#else
    RaytracingAccelerationStructure scene_tlas = get_resource<RaytracingAccelerationStructure>(cb_in.scene_tlas_index);
#endif
    
    // Make the primary ray and start tracing
    uint2 pixel_pos = uint2(dispatch_id.x % cb_view.render_dim.x, dispatch_id.x / cb_view.render_dim.x);
    float3 energy = trace_path(scene_tlas, dispatch_id.xy, pixel_pos, cb_view.render_dim);

    RWTexture2D<float4> buffer_energy = get_resource<RWTexture2D<float4> >(cb_in.texture_energy_index);
    buffer_energy[pixel_pos].xyz = energy;
}
