#include "common.hlsl"
#include "intersect.hlsl"
#include "accelstruct.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

//struct cb_shader_params_t
//{
//  uint scene_tlas;
//};

//ConstantBuffer<cb_shader_params_t> cb_params : register(b1);

struct constants_t
{
    uint tlas_index;
    uint rt_index;
};
ConstantBuffer<constants_t> g_consts : register(b1);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    ByteAddressBuffer scene_tlas = ResourceDescriptorHeap[NonUniformResourceIndex(g_consts.tlas_index)];
    RWTexture2D<float4> render_target = ResourceDescriptorHeap[NonUniformResourceIndex(g_consts.rt_index)];
    
    uint2 pixel_pos = uint2(dispatch_id.xy);
    
    // Prepare primary ray and hit result
    ray_t ray = make_primary_ray(pixel_pos, cb_view.render_dim);
    hit_result_t hit = make_hit_result();
    
    // Trace the ray against the scene TLAS
    trace_ray_tlas(scene_tlas, ray, hit);
    
    // Write result to render target
    render_target[pixel_pos] = float4(hit.bary, 1.0f);
}
