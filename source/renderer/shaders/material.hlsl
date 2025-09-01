#pragma once
#include "common.hlsl"

struct sampled_material_t
{
    float3 base_color;
    float opacity;
    float metallic;
    float3 normal;
    float roughness;
    float3 emissive_color;
};

// TODO: Pass in triangle interpolated normal and tangent, calculate world normal in here
sampled_material_t sample_material(material_t material, float2 tex_coord)
{
    sampled_material_t sampled_material = (sampled_material_t)0;
    
    Texture2D texture_base_color = get_resource<Texture2D>(material.base_color_index);
    float4 base_color = texture_base_color.Sample(sampler_linear_wrap, tex_coord);
    sampled_material.base_color = material.base_color_factor * base_color.xyz;
    sampled_material.opacity = base_color.w;
    
    Texture2D texture_metallic_roughness = get_resource<Texture2D>(material.metallic_roughness_index);
    float4 metallic_roughness = texture_metallic_roughness.Sample(sampler_linear_wrap, tex_coord);
    sampled_material.metallic = metallic_roughness.b;
    sampled_material.roughness = metallic_roughness.g;
    
    // TODO: Proper normal mapping, get hit surface in here with normal/tangent/bitangent
    Texture2D texture_normal = get_resource<Texture2D>(material.normal_index);
    float4 normal = texture_normal.Sample(sampler_linear_wrap, tex_coord);
    sampled_material.normal = normal.xyz;
    
    Texture2D texture_emissive = get_resource<Texture2D>(material.emissive_index);
    float4 emissive = texture_emissive.Sample(sampler_linear_wrap, tex_coord);
    sampled_material.emissive_color = material.emissive_strength * material.emissive_factor * emissive.xyz;
    
    return sampled_material;
}
