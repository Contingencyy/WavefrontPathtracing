#pragma once

#define CPLUSPLUS !defined(__HLSL_VERSION)
#define HLSL defined(__HLSL_VERSION)

#ifdef CPLUSPLUS
#define STATIC_ASSERT(x) static_assert(x)
#else
#define STATIC_ASSERT(x)
#endif

#if CPLUSPLUS
#define int i32
#define uint u32
#define uint2 glm::uvec2
#define uint3 glm::uvec3
#define uint4 glm::uvec4
#define float f32
#define float2 glm::vec2
#define float3 glm::vec3
#define float4 glm::vec4
#define float3x3 glm::mat3
#define float4x4 glm::mat4
#endif

struct render_settings_shader_data_t
{
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

#define TRIANGLE_SIZE 72
struct triangle_t
{
	float3 p0;
	float3 p1;
	float3 p2;

	float3 n0;
	float3 n1;
	float3 n2;
};

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

#define INSTANCE_DATA_SIZE 184
struct instance_data_t
{
	float4x4 local_to_world;
	float4x4 world_to_local;
	material_t material;
};

// ---------------------------------------------------------------------------------------
// Acceleration structure

struct bvh_header_t
{
	uint nodes_offset;
	uint triangles_offset;
	uint indices_offset;
};

#define BVH_NODE_SIZE 32
struct bvh_node_t
{
	float3 aabb_min;
	uint left_first;
	float3 aabb_max;
	uint prim_count;
};

#define BVH_TRIANGLE_INDEX_SIZE 4
#define BVH_TRIANGLE_SIZE 36
struct bvh_triangle_t
{
	float3 p0;
	float3 p1;
	float3 p2;
};

#define BVH_INSTANCE_SIZE 92
struct bvh_instance_t
{
	float4x4 world_to_local;
	float3 aabb_min;
	float3 aabb_max;
	uint bvh_index;
};

struct tlas_header_t
{
	uint nodes_offset;
	uint instances_offset;
};

#define TLAS_NODE_SIZE 32
struct tlas_node_t
{
	float3 aabb_min;
	// left_right will store two separate indices, 16 bit for each
	uint left_right;
	float3 aabb_max;
	uint instance_idx;
};

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
