#pragma once

#ifdef __cplusplus
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
	uint use_wavefront_pathtracing;
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

struct bvh_triangle_t
{
	float3 p0;
	float3 p1;
	float3 p2;
};

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

struct tlas_node_t
{
	float3 aabb_min;
	// left_right will store two separate indices, 16 bit for each
	uint left_right;
	float3 aabb_max;
	uint instance_idx;
};

struct intersection_result_t
{
	uint instance_idx;
	uint primitive_idx;
	float t;
	float2 bary;
};

// We need a RayDesc2 struct that is an exact copy of the DXR RayDesc struct because doing a
// ByteAddressBuffer.Load<RayDesc> will result in a deadlock!
// See: https://github.com/microsoft/DirectXShaderCompiler/issues/5261
struct RayDesc2
{
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
};

#ifdef __cplusplus
#undef int
#undef uint
#undef float
#undef float2
#undef float3
#undef float4
#undef float3x3
#undef float4x4
#endif
