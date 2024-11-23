#pragma once
#include "shared.hlsl.h"

/*
    Global defines
*/

#define RAY_MAX_T 3.402823466e+38F
#define RAY_NUDGE_MULTIPLIER 0.001f

#define TRI_BUFFER_IDX_INVALID 0xFFFFFFFF
#define INSTANCE_IDX_INVALID 0xFFFFFFFF
#define PRIMITIVE_IDX_INVALID 0xFFFFFFFF

#define PI 3.14159265f
#define TWO_PI 2.0f * PI
#define INV_PI 1.0f / PI
#define INV_TWO_PI 1.0f / TWO_PI
#define INV_ATAN float2(0.1591f, 0.3183f)

/*
    Global constant buffers
*/

ConstantBuffer<view_shader_data_t> cb_view : register(b0);

/*
    Common data structures
*/

triangle_t load_triangle(ByteAddressBuffer buffer, uint primitive_idx)
{
    uint byte_offset = TRIANGLE_SIZE * primitive_idx;
    triangle_t tri = buffer.Load<triangle_t>(byte_offset);
    return tri;
}

template<typename T>
T triangle_interpolate(T v0, T v1, T v2, float3 bary)
{
    return v0 * bary.x + v1 * bary.y + v2 * bary.z;
}

struct hit_result_t
{
    float3 bary;
    uint tri_buffer_idx;
    uint primitive_idx;
    uint instance_idx;
};

hit_result_t make_hit_result()
{
    hit_result_t hit = (hit_result_t)0;
    hit.tri_buffer_idx = TRI_BUFFER_IDX_INVALID;
    hit.primitive_idx = PRIMITIVE_IDX_INVALID;
    hit.instance_idx = INSTANCE_IDX_INVALID;
    
    return hit;
}

bool hit_result_is_valid(hit_result_t hit)
{
    return (
        hit.tri_buffer_idx != TRI_BUFFER_IDX_INVALID &&
        hit.instance_idx != INSTANCE_IDX_INVALID &&
        hit.primitive_idx != PRIMITIVE_IDX_INVALID
    );
}

struct ray_t
{
    float3 origin;
    float3 dir;
    float3 inv_dir;
    float t;
};

ray_t make_primary_ray(uint2 pixel_pos, float2 render_dim)
{
    float2 uv = (pixel_pos + 0.5f) / render_dim;
    uv.y = 1.0f - uv.y;
    
    float2 pixel_pos_clip = float2(2.0f * uv - 1.0f);
    
    float3 camera_to_pixel_view = normalize(mul(float4(pixel_pos_clip, 1.0f, 1.0f), cb_view.clip_to_view).xyz);
    float3 camera_to_pixel_world = mul(float4(camera_to_pixel_view, 0.0f), cb_view.view_to_world).xyz;
    float3 camera_origin_world = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), cb_view.view_to_world).xyz;
    
    ray_t ray = (ray_t)0;
    ray.origin = camera_origin_world;
    ray.dir = camera_to_pixel_world;
    ray.inv_dir = 1.0f / ray.dir;
    ray.t = RAY_MAX_T;
    
    return ray;
}

//ray_t make_primary_ray(uint2 pixel_pos, float tan_fov, float aspect_ratio, float2 render_dim)
//{
//    float2 uv = (pixel_pos + 0.5f) / render_dim.x;
//    //uv.y = 1.0f - uv.y;
    
//    float2 pixel_pos_view = float2(2.0f * uv.x - 1.0f, 1.0f - 2.0f * uv.y);
    
//    pixel_pos_view.x *= aspect_ratio;
//    pixel_pos_view *= tan_fov;
    
//    float3 camera_origin_world = mul(cb_view.world_to_view, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
//    float3 camera_to_pixel_world = mul(cb_view.world_to_view, float4(pixel_pos_view, 1.0f, 0.0f)).xzy;
//    camera_to_pixel_world = normalize(camera_to_pixel_world);
    
//    ray_t ray = (ray_t) 0;
//    ray.origin = camera_origin_world;
//    ray.dir = camera_to_pixel_world;
//    ray.inv_dir = 1.0f / ray.dir;
//    ray.t = RAY_MAX_T;
    
//    return ray;
//}
