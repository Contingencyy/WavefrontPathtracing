#include "../common.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint texture_energy_index;
    uint texture_throughput_index;
    uint buffer_pixel_coords_index;
    uint buffer_pixel_coords_two_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

static const RWByteAddressBuffer buffer_ray_counts = get_resource_uniform<RWByteAddressBuffer>(cb_in.buffer_ray_counts_index);
static const RWByteAddressBuffer buffer_pixel_coords = get_resource_uniform<RWByteAddressBuffer>(cb_in.buffer_pixel_coords_index);
static const RWByteAddressBuffer buffer_pixel_coords_two = get_resource_uniform<RWByteAddressBuffer>(cb_in.buffer_pixel_coords_two_index);

static const RWTexture2D<float4> texture_energy = get_resource_uniform<RWTexture2D<float4> >(cb_in.texture_energy_index);
static const RWTexture2D<float4> texture_throughput = get_resource_uniform<RWTexture2D<float4> >(cb_in.texture_throughput_index);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    // Ray counts buffer stores up to 8 bounces, so we only need to clear it up to index 8
    [branch]
    if (dispatch_id.x == 0)
        buffer_ray_counts.Store<uint>(dispatch_id.x * sizeof(uint), cb_view.render_dim.x * cb_view.render_dim.y);
    else if (dispatch_id.x <= 8)
        buffer_ray_counts.Store<uint>(dispatch_id.x * sizeof(uint), 0);

    // Initialize energy, throughput, and pixel coord buffers
    uint2 pixel_pos = uint2(dispatch_id.x % cb_view.render_dim.x, dispatch_id.x / cb_view.render_dim.x);

    buffer_pixel_coords.Store<uint2>(dispatch_id.x * sizeof(uint2), pixel_pos);
    buffer_pixel_coords_two.Store<uint2>(dispatch_id.x * sizeof(uint2), uint2(0, 0));
    
    texture_energy[pixel_pos] = float4(0.0, 0.0, 0.0, 0.0);
    texture_throughput[pixel_pos] = float4(1.0, 1.0, 1.0, 0.0);
}
