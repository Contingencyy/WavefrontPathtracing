#pragma once
#include "shared.hlsl.h"

/*
    Global defines
*/

#define SMALL_FLOAT                 (1e-8)
#define FLOAT_MAX                   (asfloat(0x7F7FFFFF))
#define UINT_MAX                    (asuint(0xFFFFFFFF))

#define INTERSECT_EPSILON           SMALL_FLOAT
#define RAY_MIN_T                   SMALL_FLOAT
#define RAY_MAX_T                   FLOAT_MAX
#define RAY_NUDGE_MULTIPLIER        SMALL_FLOAT
#define TRIANGLE_BACKFACE_CULLING   1

#define INSTANCE_IDX_INVALID        UINT_MAX
#define PRIMITIVE_IDX_INVALID       UINT_MAX

#define PI                          3.14159265
#define TWO_PI                      6.28318530
#define INV_PI                      0.31830988
#define INV_TWO_PI                  0.15915494
#define INV_ATAN                    float2(0.1591, 0.3183)

// Define for SW/HW raytracing is set in shader compiler, if its not default to hardware
#ifndef RAYTRACING_MODE_SOFTWARE
#define RAYTRACING_MODE_SOFTWARE    0
#define RAYTRACING_MODE_HARDWARE    !RAYTRACING_MODE_SOFTWARE
#endif

#define RANDOM_USE_WANG_HASH        1
#define RANDOM_USE_XOR_SHIFT        !RANDOM_USE_WANG_HASH

#define IS_BIT_FLAG_SET(flags, flag) ((flags & flag) == 1)

/*
    Global constant buffers
*/

ConstantBuffer<render_settings_t> cb_settings : register(b0, space0);
ConstantBuffer<view_t> cb_view : register(b1, space0);

SamplerState sampler_linear_wrap : register(s0, space0);

/*
    Resource access
*/

// Returns a non-uniform bindless resource, meaning that it will can be different across threads within a wave
template<typename T>
T get_resource(uint index)
{
    return ResourceDescriptorHeap[NonUniformResourceIndex(index)];
}

// Returns a uniform bindless resource, meaning that it will be the same resource within a wave
template<typename T>
T get_resource_uniform(uint index)
{
    return ResourceDescriptorHeap[index];
}

/*
    Random
*/

uint xor_shift(inout uint seed)
{
    //seed = 1664525 * seed + 1013904223;
    seed ^= (seed << 13);
    seed ^= (seed >> 17);
    seed ^= (seed << 5);
    
    return seed;
}

uint wang_hash(inout uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    
    return seed;
}

float rand_float(inout uint seed)
{
#if RANDOM_USE_WANG_HASH
    return float(wang_hash(seed)) * 2.3283064365387e-10f;
#elif RANDOM_USE_XOR_SHIFT
    return float(xor_shift(seed)) * 2.3283064365387e-10f;
#endif
}

/*
    Common data structures
*/

triangle_t load_triangle(ByteAddressBuffer buffer, uint primitive_idx)
{
    uint byte_offset = sizeof(triangle_t) * primitive_idx;
    triangle_t tri = buffer.Load<triangle_t>(byte_offset);
    return tri;
}

instance_data_t load_instance(ByteAddressBuffer buffer, uint instance_idx)
{
    uint byte_offset = sizeof(instance_data_t) * instance_idx;
    instance_data_t instance_data = buffer.Load<instance_data_t>(byte_offset);
    return instance_data;
}

hit_result_t make_hit_result()
{
    hit_result_t hit = (hit_result_t)0;
    hit.instance_idx = INSTANCE_IDX_INVALID;
    hit.primitive_idx = PRIMITIVE_IDX_INVALID;
    hit.t = RAY_MAX_T;
    hit.bary = (float2) 0;
    
    return hit;
}

bool has_hit_geometry(hit_result_t hit)
{
    return (
        hit.instance_idx != INSTANCE_IDX_INVALID &&
        hit.primitive_idx != PRIMITIVE_IDX_INVALID
    );
}

struct ray_t
{
    float3 Origin;
    float3 Direction;
    float3 inv_dir;
    float t;
};

#if RAYTRACING_MODE_SOFTWARE
ray_t make_ray(float3 origin, float3 dir)
{
    ray_t ray = (ray_t) 0;
    ray.Origin = origin + dir * RAY_MIN_T;
    ray.Direction = dir;
    ray.inv_dir = 1.0f / dir;
    ray.t = RAY_MAX_T;
    
    return ray;
}

ray_t make_primary_ray(uint2 pixel_pos, float2 render_dim)
{
    float2 uv = (pixel_pos + 0.5f) / render_dim;
    uv.y = 1.0f - uv.y;
    
    float2 pixel_pos_clip = float2(2.0f * uv - 1.0f);
    
    float3 camera_to_pixel_view = normalize(mul(float4(pixel_pos_clip, 1.0f, 1.0f), cb_view.clip_to_view).xyz);
    float3 camera_to_pixel_world = mul(float4(camera_to_pixel_view, 0.0f), cb_view.view_to_world).xyz;
    float3 camera_origin_world = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), cb_view.view_to_world).xyz;
    
    ray_t ray = make_ray(camera_origin_world, camera_to_pixel_world);
    return ray;
}
#else
RayDesc2 make_ray(float3 origin, float3 dir)
{
    RayDesc2 ray = (RayDesc2)0;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = RAY_MIN_T;
    ray.TMax = RAY_MAX_T;

    return ray;
}

RayDesc2 make_primary_ray(uint2 pixel_pos, float2 render_dim)
{
    float2 uv = (pixel_pos + 0.5f) / render_dim;
    uv.y = 1.0f - uv.y;
    
    float2 pixel_pos_clip = float2(2.0f * uv - 1.0f);
    
    float3 camera_to_pixel_view = normalize(mul(float4(pixel_pos_clip, 1.0f, 1.0f), cb_view.clip_to_view).xyz);
    float3 camera_to_pixel_world = mul(float4(camera_to_pixel_view, 0.0f), cb_view.view_to_world).xyz;
    float3 camera_origin_world = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), cb_view.view_to_world).xyz;

    RayDesc2 ray = make_ray(camera_origin_world, camera_to_pixel_world);
    return ray;
}

// We need a RayDesc2 struct that is an exact copy of the DXR RayDesc struct because doing a
// (RW)ByteAddressBuffer.Load<RayDesc> will result in a deadlock!
// See: https://github.com/microsoft/DirectXShaderCompiler/issues/5261
RayDesc raydesc2_to_raydesc(RayDesc2 ray_desc2)
{
    RayDesc ray_desc = (RayDesc)0;
    ray_desc.Origin = ray_desc2.Origin;
    ray_desc.Direction = ray_desc2.Direction;
    ray_desc.TMin = ray_desc2.TMin;
    ray_desc.TMax = ray_desc2.TMax;
    
    return ray_desc;
}
#endif

template<typename T>
T interpolate(T v0, T v1, T v2, float2 bary)
{
    return v0 + bary.x * (v1 - v0) + bary.y * (v2 - v0);
}

struct hit_surface_t
{
    float3 position;
    float3 normal;
    float2 tex_coord;

    instance_data_t instance;
    triangle_t tri;
};

hit_surface_t get_hit_surface(ByteAddressBuffer buffer_instances, hit_result_t hit)
{
    hit_surface_t hit_surface = (hit_surface_t) 0;
    
    hit_surface.instance = load_instance(buffer_instances, hit.instance_idx);
    ByteAddressBuffer triangle_buffer = get_resource<ByteAddressBuffer>(hit_surface.instance.triangle_buffer_idx);
    hit_surface.tri = load_triangle(triangle_buffer, hit.primitive_idx);
    
    hit_surface.position = interpolate(hit_surface.tri.v0.position, hit_surface.tri.v1.position, hit_surface.tri.v2.position, hit.bary);
    hit_surface.position = mul(float4(hit_surface.position, 1.0), hit_surface.instance.local_to_world).xyz;
    // TODO: Calculate bitangent, do normal mapping
    hit_surface.normal = interpolate(hit_surface.tri.v0.normal, hit_surface.tri.v1.normal, hit_surface.tri.v2.normal, hit.bary);
    hit_surface.normal = normalize(mul(float4(hit_surface.normal, 0.0), hit_surface.instance.local_to_world).xyz);
    //hit_surface.tangent = interpolate(hit_tri.v0.tangent, hit_tri.v1.tangent, hit_tri.v2.tangent, hit.bary);
    hit_surface.tex_coord = interpolate(hit_surface.tri.v0.uv, hit_surface.tri.v1.uv, hit_surface.tri.v2.uv, hit.bary);
    
    return hit_surface;
}

/*
    Misc
*/

float3 linear_to_srgb(float3 color)
{
    float3 clamped = clamp(color, 0.0f, 1.0f);
    float3 cutoff = step(clamped, float3(0.0031308f, 0.0031308f, 0.0031308f));
    float3 higher = 1.055f * pow(clamped, 1.0f / 2.4f) - 0.055f;
    float3 lower = clamped * 12.92f;
    
    return lerp(higher, lower, cutoff);
}

float3 srgb_to_linear(float3 color)
{
    float3 clamped = clamp(color, 0.0f, 1.0f);
    float3 cutoff = step(clamped, 0.04045f);
    float3 higher = pow((clamped + 0.055f) / 1.055f, 2.4f);
    float3 lower = clamped / 12.92f;
    
    return lerp(higher, lower, cutoff);
}

bool is_nan(float x)
{
    return (asuint(x) & 0x7fffffff) > 0x7f800000;
}

uint murmur_mix(uint hash)
{
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    return hash;
}

float3 int_to_color(uint value)
{
    uint hash = murmur_mix(value);
    float3 color = float3(
        (hash >> 0) & 255,
        (hash >> 8) & 255,
        (hash >> 16) & 255
    );
    return color * (1.0 / 255.0);
}
