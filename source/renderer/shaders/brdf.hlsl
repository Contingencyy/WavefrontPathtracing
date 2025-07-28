#pragma once
#include "common.hlsl"

// Normal distribution function
float d_ggx(float NoH, float roughness)
{
    float a = roughness * roughness;
    float f = (NoH * a - NoH) * NoH + 1.0;
    return a / (PI * f * f);
}



// Fresnel
float3 f_schlick(float u, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - u, 5.0);
}

// Fresnel90
float f_schlick90(float u, float F0, float F90)
{
    return F0 + (F90 - F0) * pow(1.0 - u, 5.0);
}
