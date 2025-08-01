#include "../common.hlsl"
#include "../sample.hlsl"
#include "../material.hlsl"
#include "../brdf.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint buffer_ray_index;
    uint buffer_intersection_index;
    uint texture_energy_index;
    uint texture_throughput_index;
    uint buffer_pixelpos_index;
    uint buffer_pixelpos_two_index;
    uint buffer_instance_index;
    uint texture_hdr_env_index;
    uint texture_hdr_env_width;
    uint texture_hdr_env_height;
    uint random_seed;
    uint recursion_depth;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

float4 sample_hdr_env(float3 dir, float2 tex_dim)
{
    Texture2D<float4> env_texture = get_resource<Texture2D>(cb_in.texture_hdr_env_index);
    uint2 sample_pos = direction_to_equirect_uv(dir) * tex_dim;
    
    return env_texture[sample_pos];
}

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
        float3 hdr_env_color = sample_hdr_env(ray.Direction, float2(cb_in.texture_hdr_env_width, cb_in.texture_hdr_env_height)).xyz;
        energy += throughput * cb_settings.hdr_env_strength * hdr_env_color;
        terminate_path = true;
    }

    hit_surface_t hit_surface = get_hit_surface(intersection_result, cb_in.buffer_instance_index);
    sampled_material_t sampled_material = sample_material(hit_surface.instance.material, hit_surface.tex_coord);
    
    [branch]
    if (any(sampled_material.emissive_color) > 0.0)
    {
        energy += throughput * sampled_material.emissive_color;
        terminate_path = true;
    }

    [branch]
    if (!terminate_path)
    {
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
            float r_diffuse = rand_float(seed);
            
            float3 N = hit_surface.normal;
            float3 L;
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
    }

    switch (cb_settings.render_view_mode)
    {
    case RENDER_VIEW_MODE_GEOMETRY_INSTANCE:            energy = int_to_color(intersection_result.instance_idx); break;
    case RENDER_VIEW_MODE_GEOMETRY_PRIMITIVE:           energy = int_to_color(intersection_result.primitive_idx); break;
    case RENDER_VIEW_MODE_GEOMETRY_BARYCENTRICS:        energy = float3(intersection_result.bary, 0.0); break;
    case RENDER_VIEW_MODE_MATERIAL_BASE_COLOR:          energy = sampled_material.base_color; break;
    case RENDER_VIEW_MODE_MATERIAL_METALLIC_ROUGHNESS:  energy = float3(0.0, sampled_material.roughness, sampled_material.metallic); break;
    case RENDER_VIEW_MODE_MATERIAL_EMISSIVE:            energy = sampled_material.emissive_color; break;
    case RENDER_VIEW_MODE_WORLD_NORMAL:                 energy = abs(hit_surface.normal); break;
    case RENDER_VIEW_MODE_RENDER_TARGET_DEPTH:          energy = float3(intersection_result.t, intersection_result.t, intersection_result.t) / cb_view.far_plane;
    }
    
    // Write new ray to output buffer if we need to do another recursion
    [branch]
    if (!terminate_path &&
        cb_in.recursion_depth < cb_settings.max_bounces &&
        cb_settings.render_view_mode == RENDER_VIEW_MODE_NONE)
    {
        // Update indirect args for next recursion
        uint write_offset;
        uint ray_count_offset = cb_in.recursion_depth + 1;
        buffer_ray_counts.InterlockedAdd(ray_count_offset * sizeof(uint), 1u, write_offset);

        // Get next ray offset and write new ray
        buffer_ray.Store<RayDesc2>(write_offset * sizeof(RayDesc2), ray);
        RWByteAddressBuffer buffer_pixelpos_two = get_resource<RWByteAddressBuffer>(cb_in.buffer_pixelpos_two_index);
        buffer_pixelpos_two.Store<uint2>(write_offset * sizeof(uint2), pixel_pos);
    }

    // Write new energy and throughput to output buffers at correct pixel position
    texture_energy[pixel_pos].xyz = energy;
    texture_throughput[pixel_pos].xyz = throughput;
}
