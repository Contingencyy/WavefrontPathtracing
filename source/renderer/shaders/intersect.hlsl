#pragma once
#if RAYTRACING_MODE_SOFTWARE
#include "common.hlsl"

// Moeller-Trumbore ray-triangle intersection
// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
bool intersect_ray_triangle(float3 v0, float3 v1, float3 v2, inout ray_t ray, inout float2 barycentrics)
{
    float3 v0v1 = v1 - v0;
    float3 v0v2 = v2 - v0;
    
    float3 pvec = cross(ray.Direction, v0v2);
    float det = dot(v0v1, pvec);
    
#if TRIANGLE_BACKFACE_CULLING
    if (det < INTERSECT_EPSILON)
#else
    if (abs(det) < INTERSECT_EPSILON)
#endif
    {
        return false;
    }
    
    float inv_det = 1.0f / det;
    float3 tvec = ray.Origin - v0;
    float v = dot(tvec, pvec) * inv_det;
    
    if (v < 0.0f || v > 1.0f)
    {
        return false;
    }
    
    float3 qvec = cross(tvec, v0v1);
    float w = dot(ray.Direction, qvec) * inv_det;
    
    if (w < 0.0f || v + w > 1.0f)
    {
        return false;
    }
    
    float t = dot(v0v2, qvec) * inv_det;
    
    if (t < 0.0f || t >= ray.t)
    {
        return false;
    }
    
    ray.t = t;
    barycentrics = float2(v, w);
    return true;
}

float intersect_ray_aabb(float3 aabb_min, float3 aabb_max, ray_t ray)
{
    float tx1 = (aabb_min.x - ray.Origin.x) * ray.inv_dir.x;
    float tx2 = (aabb_max.x - ray.Origin.x) * ray.inv_dir.x;
    float tmin = min(tx1, tx2);
    float tmax = max(tx1, tx2);
    
    float ty1 = (aabb_min.y - ray.Origin.y) * ray.inv_dir.y;
    float ty2 = (aabb_max.y - ray.Origin.y) * ray.inv_dir.y;
    tmin = max(tmin, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));
    
    float tz1 = (aabb_min.z - ray.Origin.z) * ray.inv_dir.z;
    float tz2 = (aabb_max.z - ray.Origin.z) * ray.inv_dir.z;
    tmin = max(tmin, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));
    
    if (tmax >= tmin && tmin < ray.t && tmax > 0.0f)
    {
        return tmin;
    }
    
    return RAY_MAX_T;
}
#endif
