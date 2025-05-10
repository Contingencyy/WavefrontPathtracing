#include "../common.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(1, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    RWByteAddressBuffer buffer_ray_counts = get_resource<RWByteAddressBuffer>(cb_in.buffer_ray_counts_index);
    
    [branch]
    if (dispatch_id.x == 0)
        buffer_ray_counts.Store<uint>(dispatch_id.x * 4, cb_view.render_dim.x * cb_view.render_dim.y);
    else
        buffer_ray_counts.Store<uint>(dispatch_id.x * 4, 0);
}
