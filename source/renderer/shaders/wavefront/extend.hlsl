#include "../common.hlsl"
#include "../accelstruct.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint buffer_rays_index;
    uint buffer_hit_results_index;
    uint buffer_scene_tlas_index;
    uint recursion_depth;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

#if RAYTRACING_MODE_SOFTWARE
static const ByteAddressBuffer buffer_scene_tlas = get_resource_uniform<ByteAddressBuffer>(cb_in.buffer_scene_tlas_index);
#else
static const RaytracingAccelerationStructure buffer_scene_tlas = get_resource_uniform<RaytracingAccelerationStructure>(cb_in.buffer_scene_tlas_index);
#endif

static const ByteAddressBuffer buffer_ray_counts = get_resource_uniform<ByteAddressBuffer>(cb_in.buffer_ray_counts_index);
static const ByteAddressBuffer buffer_rays = get_resource_uniform<ByteAddressBuffer>(cb_in.buffer_rays_index);
static const RWByteAddressBuffer buffer_hit_results = get_resource_uniform<RWByteAddressBuffer>(cb_in.buffer_hit_results_index);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint ray_count = buffer_ray_counts.Load<uint>(cb_in.recursion_depth * 4);

    // Dispatches might have work that is not divisible by the dispatch thread dimensions, so we skip those
    if (dispatch_id.x >= ray_count)
        return;
    
    RayDesc2 ray = buffer_rays.Load<RayDesc2>(dispatch_id.x * sizeof(RayDesc2));
    
    hit_result_t hit = make_hit_result();
    trace_ray_tlas(buffer_scene_tlas, ray, hit);

    buffer_hit_results.Store<hit_result_t>(dispatch_id.x * sizeof(hit_result_t), hit);
}
