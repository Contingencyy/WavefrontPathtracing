#include "../common.hlsl"

struct shader_input_t
{
    uint recursion_depth;
    uint buffer_ray_counts_index;
    uint buffer_indirect_args_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    RWByteAddressBuffer buffer_indirect_args = get_resource<RWByteAddressBuffer>(cb_in.buffer_indirect_args_index);
    ByteAddressBuffer buffer_ray_counts = get_resource<ByteAddressBuffer>(cb_in.buffer_ray_counts_index);

    uint write_offset = cb_in.recursion_depth * 12;
    uint ray_count_for_recursion = buffer_ray_counts.Load<uint>(cb_in.recursion_depth * 4);
    buffer_indirect_args.Store<uint3>(write_offset, uint3((ray_count_for_recursion + 63) / 64, 1, 1));
}
