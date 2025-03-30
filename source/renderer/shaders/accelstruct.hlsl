#pragma once
#include "common.hlsl"
#include "intersect.hlsl"

#if RAYTRACING_MODE_SOFTWARE
bvh_header_t bvh_get_header(ByteAddressBuffer buffer)
{
    bvh_header_t header = buffer.Load<bvh_header_t>(0);
    return header;
}

bvh_node_t bvh_get_node(ByteAddressBuffer buffer, bvh_header_t header, uint node_idx)
{
    uint byte_offset = header.nodes_offset + BVH_NODE_SIZE * node_idx;
    bvh_node_t node = buffer.Load<bvh_node_t>(byte_offset);
    return node;
}

bvh_triangle_t bvh_get_triangle(ByteAddressBuffer buffer, bvh_header_t header, uint tri_idx)
{
    uint byte_offset = header.triangles_offset + BVH_TRIANGLE_SIZE * tri_idx;
    bvh_triangle_t tri = buffer.Load<bvh_triangle_t>(byte_offset);
    return tri;
}

uint bvh_get_triangle_index(ByteAddressBuffer buffer, bvh_header_t header, uint tri_idx)
{
    uint byte_offset = header.indices_offset + BVH_TRIANGLE_INDEX_SIZE * tri_idx;
    uint triangle_index = buffer.Load<uint>(byte_offset);
    return triangle_index;
}

tlas_header_t tlas_get_header(ByteAddressBuffer buffer)
{
    tlas_header_t header = buffer.Load<tlas_header_t>(0);
    return header;
}

tlas_node_t tlas_get_node(ByteAddressBuffer buffer, tlas_header_t header, uint node_idx)
{
    uint byte_offset = header.nodes_offset + TLAS_NODE_SIZE * node_idx;
    tlas_node_t node = buffer.Load<tlas_node_t>(byte_offset);
    return node;
}

bvh_instance_t tlas_get_instance(ByteAddressBuffer buffer, tlas_header_t header, uint instance_idx)
{
    uint byte_offset = header.instances_offset + BVH_INSTANCE_SIZE * instance_idx;
    bvh_instance_t instance = buffer.Load<bvh_instance_t>(byte_offset);
    return instance;
}

bool tlas_node_is_leaf(tlas_node_t node)
{
    return node.left_right == 0;
}

bool trace_ray_bvh_local(ByteAddressBuffer buffer, inout ray_t ray, inout hit_result_t hit)
{
    bool has_hit = false;
    
    bvh_header_t header = bvh_get_header(buffer);
    bvh_node_t node = bvh_get_node(buffer, header, 0);
    bvh_node_t stack[64];
    uint stack_at = 0;
 
    while (true)
    {
        // Node is a leaf node, check for triangle intersections
        if (node.prim_count > 0)
        {
            for (uint i = node.left_first; i < node.left_first + node.prim_count; ++i)
            {
                uint tri_idx = bvh_get_triangle_index(buffer, header, i);
                bvh_triangle_t tri = bvh_get_triangle(buffer, header, tri_idx);
                bool intersected = intersect_ray_triangle(tri.p0, tri.p1, tri.p2, ray, hit.bary);
                
                if (intersected)
                {
                    hit.primitive_idx = tri_idx;
                    has_hit = intersected;
                }
            }
            
            if (stack_at == 0)
                break;
            else
                node = stack[--stack_at];
            continue;
        }
        
		// Current node is not a leaf node, keep traversing the BVH
        bvh_node_t node_left = bvh_get_node(buffer, header, node.left_first);
        bvh_node_t node_right = bvh_get_node(buffer, header, node.left_first + 1);
        
        // Determine the distance to the left and right child nodes
        float dist_left = intersect_ray_aabb(node_left.aabb_min, node_left.aabb_max, ray);
        float dist_right = intersect_ray_aabb(node_right.aabb_min, node_right.aabb_max, ray);
        
        // Swap the left and right child nodes to have the closest one first
        if (dist_left > dist_right)
        {
            float temp_dist = dist_left;
            dist_left = dist_right;
            dist_right = temp_dist;
            
            bvh_node_t temp_node = node_left;
            node_left = node_right;
            node_right = temp_node;
        }
        
        // If we have no intersected with the child nodes, we keep traversing the node stack
        if (dist_left == RAY_MAX_T)
        {
            if (stack_at == 0)
                break;
            else
                node = stack[--stack_at];
        }
        // We have intersected with at least one of the child nodes, check the closest one first
        // and push the other one onto the stack
        else
        {
            node = node_left;
            if (dist_right != RAY_MAX_T)
                stack[stack_at++] = node_right;
        }
    }
    
    return has_hit;
}

bool trace_ray_bvh_instance(bvh_instance_t instance, inout ray_t ray, inout hit_result_t hit)
{
    bool has_hit = false;
    
    // Create a new ray that will be in local/object space of the bvh we want to intersect here
    // Same thing as if we transformed the BVH triangles to world space, but much more convenient
    ray_t ray_local = ray;
    ray_local.Origin = mul(float4(ray.Origin, 1.0f), instance.world_to_local).xyz;
    ray_local.Direction = mul(float4(ray.Direction, 0.0f), instance.world_to_local).xyz;
    ray_local.inv_dir = 1.0f / ray_local.Direction;
    
    ByteAddressBuffer bvh_buffer = get_resource<ByteAddressBuffer>(instance.bvh_index);
    has_hit = trace_ray_bvh_local(bvh_buffer, ray_local, hit);
    ray.t = ray_local.t;
    
    return has_hit;
}

void trace_ray_tlas(ByteAddressBuffer buffer, inout ray_t ray, inout hit_result_t hit)
{
    tlas_header_t header = tlas_get_header(buffer);
    tlas_node_t node = tlas_get_node(buffer, header, 0);
    
    // Check if we miss the entire TLAS
    if (intersect_ray_aabb(node.aabb_min, node.aabb_max, ray) == RAY_MAX_T)
        return;
    
    tlas_node_t stack[64];
    uint stack_at = 0;
    
    while (true)
    {
        if (tlas_node_is_leaf(node))
        {
            bvh_instance_t instance = tlas_get_instance(buffer, header, node.instance_idx);
            bool intersected = trace_ray_bvh_instance(instance, ray, hit);
            
            if (intersected)
            {
                hit.instance_idx = node.instance_idx;
                hit.t = ray.t;
            }
            
            if (stack_at == 0)
                break;
            else
                node = stack[--stack_at];
            continue;
        }
        
        // Node is not a leaf node, keep traversing
        tlas_node_t node_left = tlas_get_node(buffer, header, node.left_right >> 16);
        tlas_node_t node_right = tlas_get_node(buffer, header, node.left_right & 0x0000FFFF);
        
        float dist_left = intersect_ray_aabb(node_left.aabb_min, node_left.aabb_max, ray);
        float dist_right = intersect_ray_aabb(node_right.aabb_min, node_right.aabb_max, ray);
        
        // Swap the left and right nodes if the right one so we always have the closest one first
        if (dist_left > dist_right)
        {
            float temp_dist = dist_left;
            dist_left = dist_right;
            dist_right = temp_dist;
            
            tlas_node_t temp_node = node_left;
            node_left = node_right;
            node_right = temp_node;
        }
        
        // If we are not intersecting with the closest node, we need to keep traversing
        // the stack or terminate if stack is empty
        if (dist_left == RAY_MAX_T)
        {
            if (stack_at == 0)
                break;
            else
                node = stack[--stack_at];
        }
        else
        {
            node = node_left;
            if (dist_right != RAY_MAX_T)
                stack[stack_at++] = node_right;
        }
    }
}
#else
void trace_ray_tlas(RaytracingAccelerationStructure scene_tlas, RayDesc ray, inout hit_result_t hit)
{
#if TRIANGLE_BACKFACE_CULLING
    RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES>ray_query;
    ray_query.TraceRayInline(scene_tlas, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0u, ray);
#else
    RayQuery<RAY_FLAG_NONE> ray_query; // For shadow rays, use RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH (https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag)
    ray_query.TraceRayInline(scene_tlas, RAY_FLAG_NONE, ~0u, ray);
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
            hit.instance_idx = ray_query.CommittedInstanceIndex();
            hit.primitive_idx = ray_query.CommittedPrimitiveIndex();
            hit.bary = ray_query.CommittedTriangleBarycentrics();
            hit.t = ray_query.CommittedRayT();
        } break;
    }
}
#endif
