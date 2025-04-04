#pragma once

#define CPLUSPLUS !defined(__HLSL_VERSION)
#define HLSL defined(__HLSL_VERSION)

#if CPLUSPLUS
#define int int32_t
#define uint uint32_t
#define uint2 glm::uvec2
#define uint3 glm::uvec3
#define uint4 glm::uvec4
#define float float
#define float2 glm::vec2
#define float3 glm::vec3
#define float4 glm::vec4
#define float3x3 glm::mat3
#define float4x4 glm::mat4
#endif

struct render_settings_shader_data_t
{
	uint use_software_rt;
	uint render_view_mode;
	uint ray_max_recursion;
	uint cosine_weighted_diffuse;
	uint accumulate;
};

struct view_shader_data_t
{
    float4x4 world_to_view; // View matrix
    float4x4 view_to_world; // Inverse view matrix
    float4x4 view_to_clip;  // Projection matrix
    float4x4 clip_to_view;  // Inverse projection matrix

    float2 render_dim; // Render dimensions/resolution in pixels
};

struct vertex_t
{
	float3 position;
	float3 normal;
};

struct triangle_t
{
	vertex_t v0;
	vertex_t v1;
	vertex_t v2;
};
#define TRIANGLE_SIZE 72

struct material_t
{
	float3 albedo;
	float specular;

	float refractivity;
	float ior;
	float3 absorption;

	uint emissive;
	float3 emissive_color;
	float emissive_intensity;
};

struct instance_data_t
{
	float4x4 local_to_world;
	float4x4 world_to_local;
	material_t material;
	uint triangle_buffer_idx;
};
#define INSTANCE_DATA_SIZE 188

// ---------------------------------------------------------------------------------------
// Acceleration structure

struct bvh_header_t
{
	uint nodes_offset;
	uint triangles_offset;
	uint indices_offset;
};

struct bvh_node_t
{
	float3 aabb_min;
	uint left_first;
	float3 aabb_max;
	uint prim_count;
};
#define BVH_NODE_SIZE 32

struct bvh_triangle_t
{
	float3 p0;
	float3 p1;
	float3 p2;
};
#define BVH_TRIANGLE_INDEX_SIZE 4
#define BVH_TRIANGLE_SIZE 36

struct bvh_instance_t
{
	float4x4 world_to_local;
	float3 aabb_min;
	float3 aabb_max;
	uint bvh_index;
};
#define BVH_INSTANCE_SIZE 92

struct tlas_header_t
{
	uint nodes_offset;
	uint instances_offset;
};

struct tlas_node_t
{
	float3 aabb_min;
	// left_right will store two separate indices, 16 bit for each
	uint left_right;
	float3 aabb_max;
	uint instance_idx;
};
#define TLAS_NODE_SIZE 32

#ifdef __CPLUSPLUS
static_assert(sizeof(triangle_t) == TRIANGLE_SIZE);
static_assert(sizeof(instance_data_t) == INSTANCE_DATA_SIZE);
static_assert(sizeof(bvh_node_t) == BVH_NODE_SIZE);
static_assert(sizeof(bvh_triangle_t) == BVH_TRIANGLE_SIZE);
static_assert(sizeof(bvh_instance_t) == BVH_INSTANCE_SIZE);
static_assert(sizeof(tlas_node_t) == TLAS_NODE_SIZE);
#endif

#if !defined(__HLSL_VERSION)
#undef int
#undef uint
#undef float
#undef float2
#undef float3
#undef float4
#undef float3x3
#undef float4x4
#endif

#undef CPLUSPLUS
#undef HLSL
