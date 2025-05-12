#include "common.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

struct shader_input_t
{
    uint texture_energy_index;
    uint texture_color_accum_index;
    uint texture_color_final_index;
    uint sample_count;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint data_offset = dispatch_id.y * cb_view.render_dim.x + dispatch_id.x;
    uint2 pixel_pos = uint2(dispatch_id.xy);
    
    // Get the final energy buffer from path trace
    Texture2D<float4> energy_buffer = get_resource<Texture2D<float4> >(cb_in.texture_energy_index);
    float3 energy = energy_buffer[pixel_pos].xyz;

    // Badness detector for NaN/INF in energy buffer
    if (is_nan(energy.x) || is_nan(energy.y) || is_nan(energy.z) || any(isinf(energy)))
    {
        energy = float3(1.0, 0.0, 1.0);
    }
    
    // Update HDR color accumulator
    RWTexture2D<float4> texture_color_accum = get_resource<RWTexture2D<float4> >(cb_in.texture_color_accum_index);
    float4 color_accum = texture_color_accum[pixel_pos];
    float sample_weight = 1.0f / cb_in.sample_count;

    if (cb_settings.accumulate)
    {
        texture_color_accum[pixel_pos] = color_accum * (1.0f - sample_weight) + float4(energy, 1.0) * sample_weight;
    }
    else
    {
        texture_color_accum[pixel_pos] = float4(energy, 1.0);
    }

    // Write final LDR color
    RWTexture2D<float4> texture_color_final = get_resource<RWTexture2D<float4> >(cb_in.texture_color_final_index);
    float4 final_color = texture_color_accum[pixel_pos];
    
    // Apply tone mapping
    final_color.xyz = final_color.xyz / (1.0f + final_color.xyz);
    
    // Convert from linear to sRGB
    final_color.xyz = linear_to_srgb(final_color.xyz);
    
    texture_color_final[pixel_pos] = final_color;
}
