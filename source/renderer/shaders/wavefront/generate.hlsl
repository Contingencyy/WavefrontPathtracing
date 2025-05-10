#include "../common.hlsl"

struct shader_input_t
{
    uint buffer_ray_index;
    uint buffer_energy_index;
    uint buffer_throughput_index;
    uint buffer_pixelpos_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel_pos = uint2(dispatch_id.x % cb_view.render_dim.x, dispatch_id.x / cb_view.render_dim.x);

    // Initialize energy, throughput, and pixelpos buffer
    RWByteAddressBuffer buffer_energy = get_resource<RWByteAddressBuffer>(cb_in.buffer_energy_index);
    buffer_energy.Store<float3>(dispatch_id.x * sizeof(float3), float3(0.0, 0.0, 0.0));

    RWByteAddressBuffer buffer_throughput = get_resource<RWByteAddressBuffer>(cb_in.buffer_throughput_index);
    buffer_throughput.Store<float3>(dispatch_id.x * sizeof(float3), float3(1.0, 1.0, 1.0));

    RWByteAddressBuffer buffer_pixelpos = get_resource<RWByteAddressBuffer>(cb_in.buffer_pixelpos_index);
    buffer_pixelpos.Store<uint2>(dispatch_id.x * sizeof(uint2), pixel_pos);

    // Make primary ray and write to buffer
    RayDesc2 ray = make_primary_ray(pixel_pos, cb_view.render_dim);
    RWByteAddressBuffer buffer_ray = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_index);
    buffer_ray.Store<RayDesc2>(dispatch_id.x * sizeof(RayDesc2), ray);
}
