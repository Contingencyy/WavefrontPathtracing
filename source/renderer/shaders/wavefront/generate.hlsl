#include "../common.hlsl"

#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

struct shader_input_t
{
    uint buffer_ray_index;
    uint buffer_energy_index;
    uint buffer_throughput_index;
    uint buffer_pixelpos_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint data_offset = dispatch_id.y * cb_view.render_dim.x + dispatch_id.x;
    uint2 pixel_pos = dispatch_id.xy;

    // Initialize energy, throughput, and pixelpos buffer
    RWByteAddressBuffer buffer_energy = get_resource<RWByteAddressBuffer>(cb_in.buffer_energy_index);
    buffer_energy.Store<float3>(data_offset * sizeof(float3), float3(0.0, 0.0, 0.0));

    RWByteAddressBuffer buffer_throughput = get_resource<RWByteAddressBuffer>(cb_in.buffer_throughput_index);
    buffer_throughput.Store<float3>(data_offset * sizeof(float3), float3(1.0, 1.0, 1.0));

    RWByteAddressBuffer buffer_pixelpos = get_resource<RWByteAddressBuffer>(cb_in.buffer_pixelpos_index);
    buffer_pixelpos.Store<uint2>(data_offset * sizeof(uint2), pixel_pos);

    // Make primary ray and write to buffer
    RayDesc2 ray = make_primary_ray(pixel_pos, cb_view.render_dim);
    RWByteAddressBuffer buffer_ray = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_index);
    buffer_ray.Store<RayDesc2>(data_offset * sizeof(RayDesc2), ray);
}
