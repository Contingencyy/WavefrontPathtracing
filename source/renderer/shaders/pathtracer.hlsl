#include "common.hlsl"
#include "intersect.hlsl"
#include "accelstruct.hlsl"
#include "sample.hlsl"

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
    uint hdr_env_index;
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
    
    float3 final_color = (float3)0;
    
    if (hit_result_is_valid(hit))
    {
        //ByteAddressBuffer instance_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(hit.instance_idx)];
        // TODO: Get the triangle buffer index from the instance buffer data, remove from hit result
        ByteAddressBuffer triangle_buffer = ResourceDescriptorHeap[NonUniformResourceIndex(hit.tri_buffer_idx)];
        triangle_t tri = load_triangle(triangle_buffer, hit.primitive_idx);
        
        float3 normal_local = triangle_interpolate<float3>(tri.n0, tri.n1, tri.n2, hit.bary);
        final_color = abs(normal_local);
    }
    else
    {
        Texture2D<float4> env_texture = ResourceDescriptorHeap[NonUniformResourceIndex(g_consts.hdr_env_index)];
        uint2 sample_pos = direction_to_equirect_uv(ray.dir) * float2(4096.0f, 2048.0f);
        final_color = env_texture[sample_pos].xyz;
        // Reinhard HDR to SDR, env texture is HDR and we render to RGBA8_UNORM
        final_color = final_color / (1.0f + final_color);
    }
    
    // Write result to render target
    render_target[pixel_pos] = float4(final_color, 1.0f);
}
