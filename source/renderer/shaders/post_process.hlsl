#include "common.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

struct shader_input_t
{
    uint color_index;
    uint rt_index;
};

ConstantBuffer<shader_input_t> g_input : register(b2, space0);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    Texture2D<float4> color_in = get_resource<Texture2D>(g_input.color_index);
    RWTexture2D<float4> rt = get_resource<RWTexture2D<float4> >(g_input.rt_index);
    
    // Make the primary ray and start tracing
    uint2 pixel_pos = uint2(dispatch_id.xy);
    float3 final_color = color_in[pixel_pos].xyz;
    
    // Apply tone mapping
    final_color = final_color / (1.0f + final_color);
    
    // Convert from linear to sRGB
    final_color = linear_to_srgb(final_color);
    
    rt[pixel_pos] = float4(final_color, color_in[pixel_pos].w);
}
