#pragma once
#include "common.hlsl"

#define INTERSECT_EPSILON 1e-8
#define TRIANGLE_BACKFACE_CULLING 1

// Möller-Trumbore ray-triangle intersection
// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
void intersect_ray_triangle(float3 v0, float3 v1, float3 v2, inout ray_t ray, inout float3 barycentrics)
{
    float3 v0v1 = v1 - v0;
    float3 v0v2 = v2 - v0;
    
    float3 pvec = cross(ray.dir, v0v2);
    float det = dot(v0v1, pvec);
    
#if TRIANGLE_BACKFACE_CULLING
    if (abs(det) < INTERSECT_EPSILON)
#else
    if (det < INTERSECT_EPSILON)
#endif
    {
        return;
    }
    
    float inv_det = 1.0f / det;
    float3 tvec = ray.origin - v0;
    float v = dot(tvec, pvec) * inv_det;
    
    if (v < 0.0f || v > 1.0f)
    {
        return;
    }
    
    float3 qvec = cross(tvec, v0v1);
    float w = dot(ray.dir, qvec) * inv_det;
    
    if (w < 0.0f || v + w > 1.0f)
    {
        return;
    }
    
    float t = dot(v0v2, qvec) * inv_det;
    
    if (t < 0.0f || t >= ray.t)
    {
        return;
    }
    
    ray.t = t;
    barycentrics = float3(1.0f - v - w, v, w);
}

void intersect_ray_aabb(float3 aabb_min, float3 aabb_max, inout ray_t ray)
{
    float tx1 = (aabb_min.x - ray.origin.x) * ray.inv_dir.x;
    float tx2 = (aabb_max.x - ray.origin.x) * ray.inv_dir.x;
    float tmin = min(tx1, tx2);
    float tmax = max(tx1, tx2);
    
    float ty1 = (aabb_min.y - ray.origin.y) * ray.inv_dir.y;
    float ty2 = (aabb_max.y - ray.origin.y) * ray.inv_dir.y;
    tmin = max(tmin, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));
    
    float tz1 = (aabb_min.z - ray.origin.z) * ray.inv_dir.z;
    float tz2 = (aabb_max.z - ray.origin.z) * ray.inv_dir.z;
    tmin = max(tmin, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));
    
    if (tmax < tmin || tmax <= 0.0f || tmin >= ray.t)
    {
        return;
    }
    
    ray.t = tmin;
}
