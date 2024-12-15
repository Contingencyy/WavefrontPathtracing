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
    
    return dir.x * tangent + dir.y * normal + dir.z * bitangent;
}


float3 uniform_hemisphere_sample(float3 normal, float2 r)
{
    float sin_theta = sqrt(1.0f - r.x * r.x);
    float phi = 2.0f * PI * r.y;
    
    return direction_to_normal_space(normal, float3(sin_theta * cos(phi), r.x, sin_theta * sin(phi)));
}

float3 cosine_weighted_hemisphere_sample(float3 normal, float2 r)
{
    float sin_theta = sqrt(r.x);
    float phi = 2.0f * PI * r.y;
    
    return direction_to_normal_space(normal, float3(sin_theta * cos(phi), sqrt(1.0f - r.x), sin_theta * sin(phi)));
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
