#include "../common.hlsl"

struct shader_input_t
{
    uint buffer_ray_counts_index;
    uint buffer_ray_index;
    uint buffer_intersection_index;
    uint buffer_scene_tlas_index;
    uint recursion_depth;
};

ConstantBuffer<shader_input_t> cb_in : register(b2, space0);

intersection_result_t make_intersection_result()
{
    intersection_result_t result = (intersection_result_t)0;
    result.instance_idx = INSTANCE_IDX_INVALID;
    result.primitive_idx = PRIMITIVE_IDX_INVALID;
    result.t = RAY_MAX_T;
    result.bary = (float2)0;
    
    return result;
}

void intersect_primary_ray_scene(RaytracingAccelerationStructure scene_tlas, RayDesc2 ray, inout intersection_result_t intersection_result)
{
    RayDesc ray_desc = raydesc2_to_raydesc(ray);
#if TRIANGLE_BACKFACE_CULLING
    RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> ray_query;
    ray_query.TraceRayInline(scene_tlas, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0u, ray_desc);
#else
    RayQuery<RAY_FLAG_NONE> ray_query; // For shadow rays, use RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH (https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag)
    ray_query.TraceRayInline(scene_tlas, RAY_FLAG_NONE, ~0u, ray_desc);
#endif
    
    while (ray_query.Proceed())
    {
        switch (ray_query.CandidateType())
        {
        case CANDIDATE_NON_OPAQUE_TRIANGLE:
            {
                ray_query.CommitNonOpaqueTriangleHit();
            } break;
        }
    }

    switch (ray_query.CommittedStatus())
    {
    case COMMITTED_TRIANGLE_HIT:
        {
            intersection_result.instance_idx = ray_query.CommittedInstanceIndex();
            intersection_result.primitive_idx = ray_query.CommittedPrimitiveIndex();
            intersection_result.bary = ray_query.CommittedTriangleBarycentrics();
            intersection_result.t = ray_query.CommittedRayT();
        } break;
    }
}

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
    intersection_result_t intersection_result = make_intersection_result();
    intersect_primary_ray_scene(buffer_scene_tlas, ray, intersection_result);

    RWByteAddressBuffer buffer_intersection = get_resource<RWByteAddressBuffer>(cb_in.buffer_intersection_index);
    buffer_intersection.Store<intersection_result_t>(dispatch_id.x * sizeof(intersection_result_t), intersection_result);
}
