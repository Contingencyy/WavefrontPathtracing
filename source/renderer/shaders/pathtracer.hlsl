#include "common.hlsl"
#include "intersect.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

//struct cb_pathtracer_data
//{
    
//};

//ConstantBuffer<cb_pathtracer_data> cb_pathtracer : register(b1);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel_pos = uint2(dispatch_id.xy);
    ray_t ray = make_primary_ray(pixel_pos, cb_view.render_dim);
    
    //float3 v0 = float3(0.0f, 15.0f, 15.0f);
    //float3 v1 = float3(5.0f, 5.0f, 15.0f);
    //float3 v2 = float3(-5.0f, 5.0f, 15.0f);
    
    //float3 barycentrics = (float)0;
    //intersect_ray_triangle(v0, v1, v2, ray, barycentrics);
    
    //RWTexture2D<float4> render_target = ResourceDescriptorHeap[NonUniformResourceIndex(0)];
    //render_target[pixel_pos] = float4(barycentrics, 1.0f);
    
    float3 aabb_min = float3(-5.0f, -5.0f, 10.0f);
    float3 aabb_max = float3(5.0f, 5.0f, 20.0f);
    
    intersect_ray_aabb(aabb_min, aabb_max, ray);
    
    if (ray.t < 10000.0f)
    {
        RWTexture2D<float4> render_target = ResourceDescriptorHeap[NonUniformResourceIndex(0)];
        render_target[pixel_pos] = float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
    else
    {
        RWTexture2D<float4> render_target = ResourceDescriptorHeap[NonUniformResourceIndex(0)];
        render_target[pixel_pos] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}
