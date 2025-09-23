#include "../common.hlsl"

struct shader_input_t
{
    uint buffer_rays_index;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

static const RWByteAddressBuffer buffer_rays = get_resource_uniform<RWByteAddressBuffer>(cb_in.buffer_rays_index);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel_pos = uint2(dispatch_id.x % cb_view.render_dim.x, dispatch_id.x / cb_view.render_dim.x);

    // Make primary ray and write to buffer
    RayDesc2 ray = make_primary_ray(pixel_pos, cb_view.render_dim);
    buffer_rays.Store<RayDesc2>(dispatch_id.x * sizeof(RayDesc2), ray);
}
