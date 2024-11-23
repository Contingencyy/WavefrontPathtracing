#pragma once
#include "common.hlsl"

float2 direction_to_equirect_uv(float3 dir)
{
    float2 uv = float2(atan2(dir.z, dir.x), asin(-dir.y));
    uv *= INV_ATAN;
    uv += 0.5f;
    
    return uv;
}
