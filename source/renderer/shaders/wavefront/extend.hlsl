#include "../common.hlsl"
#include "../accelstruct.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint buffer_ray_index;
    uint buffer_intersection_index;
    uint buffer_scene_tlas_index;
    uint recursion_depth;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    ByteAddressBuffer buffer_ray_counts = get_resource<ByteAddressBuffer>(cb_in.buffer_ray_counts_index);
    uint ray_count = buffer_ray_counts.Load<uint>(cb_in.recursion_depth * 4);

    // Dispatches might have work that is not divisible by the dispatch thread dimensions, so we skip those
    if (dispatch_id.x >= ray_count)
        return;
    
    ByteAddressBuffer buffer_ray = get_resource<ByteAddressBuffer>(cb_in.buffer_ray_index);
    RayDesc2 ray = buffer_ray.Load<RayDesc2>(dispatch_id.x * sizeof(RayDesc2));
    
    RaytracingAccelerationStructure buffer_scene_tlas = get_resource<RaytracingAccelerationStructure>(cb_in.buffer_scene_tlas_index);
    hit_result_t hit = make_hit_result();
    trace_ray_tlas(buffer_scene_tlas, ray, hit);

    RWByteAddressBuffer buffer_intersection = get_resource<RWByteAddressBuffer>(cb_in.buffer_intersection_index);
    buffer_intersection.Store<hit_result_t>(dispatch_id.x * sizeof(hit_result_t), hit);
}
