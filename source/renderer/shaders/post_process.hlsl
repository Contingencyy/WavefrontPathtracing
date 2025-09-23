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

static const Texture2D<float4> texture_energy = get_resource_uniform<Texture2D<float4> >(cb_in.texture_energy_index);
static const RWTexture2D<float4> texture_color_accum = get_resource_uniform<RWTexture2D<float4> >(cb_in.texture_color_accum_index);
static const RWTexture2D<float4> texture_color_final = get_resource_uniform<RWTexture2D<float4> >(cb_in.texture_color_final_index);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint data_offset = dispatch_id.y * cb_view.render_dim.x + dispatch_id.x;
    uint2 pixel_pos = uint2(dispatch_id.xy);
    
    // Get the final energy buffer from path trace
    float3 energy = texture_energy[pixel_pos].xyz;

    // Badness detector for NaN/INF in energy buffer
    if (is_nan(energy.x) || is_nan(energy.y) || is_nan(energy.z) || any(isinf(energy)))
    {
        energy = float3(1.0, 0.0, 1.0);
    }
    
    // Update HDR color accumulator
    float4 color_accum = texture_color_accum[pixel_pos];
    float sample_weight = 1.0f / cb_in.sample_count;

    if (cb_settings.accumulate && cb_settings.render_view_mode == RENDER_VIEW_MODE_NONE)
    {
        texture_color_accum[pixel_pos] = color_accum * (1.0f - sample_weight) + float4(energy, 1.0) * sample_weight;
    }
    else
    {
        texture_color_accum[pixel_pos] = float4(energy, 1.0);
    }

    // Write final LDR color
    float4 final_color = texture_color_accum[pixel_pos];

    if (cb_settings.render_view_mode == RENDER_VIEW_MODE_NONE)
    {
        // Apply tone mapping
        final_color.xyz = final_color.xyz / (1.0f + final_color.xyz);
    }

    // TODO-justink: If the output is a color of any kind, I should convert back to sRGB (e.g. base color/emissive textures)
    if (cb_settings.render_view_mode == RENDER_VIEW_MODE_NONE ||
        cb_settings.render_view_mode == RENDER_VIEW_MODE_MATERIAL_BASE_COLOR ||
        cb_settings.render_view_mode == RENDER_VIEW_MODE_MATERIAL_EMISSIVE)
    {
        // Convert from linear to sRGB
        final_color.xyz = linear_to_srgb(final_color.xyz);
    }
    
    texture_color_final[pixel_pos] = final_color;
}
