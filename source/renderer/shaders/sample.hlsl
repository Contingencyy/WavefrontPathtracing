#pragma once
#include "common.hlsl"

float2 direction_to_equirect_uv(float3 dir)
{
    float2 uv = float2(atan2(dir.z, dir.x), asin(-dir.y));
    uv *= INV_ATAN;
    uv += 0.5f;
    
    return uv;
}

void create_orthonormal_basis(float3 normal, inout float3 tangent, inout float3 bitangent)
{
    if (abs(normal.x) > abs(normal.z))
    {
        tangent = normalize(float3(-normal.y, normal.x, 0.0f));
    }
    else
    {
        tangent = normalize(float3(0.0f, -normal.z, normal.y));
    }
    
    bitangent = cross(normal, tangent);
}

float3 direction_to_normal_space(float3 normal, float3 dir)
{
    float3 tangent = (float3) 0;
    float3 bitangent = (float3) 0;
    create_orthonormal_basis(normal, tangent, bitangent);
    
    float3x3 basis = float3x3(tangent, normal, bitangent);
    return mul(dir, basis);
}

void uniform_hemisphere_sample(float3 normal, float2 r, out float3 out_dir, out float out_pdf)
{
    float sin_theta = sqrt(1.0f - r.x * r.x);
    float phi = 2.0f * PI * r.y;
    
    out_dir = normalize(direction_to_normal_space(normal, float3(sin_theta * cos(phi), r.x, sin_theta * sin(phi))));
    out_pdf = INV_TWO_PI;
}

void cosine_weighted_hemisphere_sample(float3 normal, float2 r, out float3 out_dir, out float out_pdf)
{
    float sin_theta = sqrt(1.0f - r.x);
    float phi = 2.0f * PI * r.y;
    
    out_dir = normalize(direction_to_normal_space(normal, float3(sin_theta * cos(phi), sqrt(r.x), sin_theta * sin(phi))));
    out_pdf = max(dot(normal, out_dir), 0.0f) * INV_PI;
}

float3 refract(float3 dir, float3 normal, float eta, float cosi, float k)
{
    return normalize(dir * eta + ((eta * cosi - sqrt(k)) * normal));
}

float fresnel(float cos_in, float cos_out, float ior_outside, float ior_inside)
{
    float s_polarized = (ior_outside * cos_in - ior_inside + cos_out) / (ior_outside * cos_in + ior_inside * cos_out);
    float p_polarized = (ior_outside * cos_out - ior_inside + cos_in) / (ior_outside * cos_out + ior_inside * cos_in);
    return 0.5f * ((s_polarized * s_polarized) + (p_polarized * p_polarized));
}
