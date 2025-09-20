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

enum RENDER_VIEW_MODE
{
	RENDER_VIEW_MODE_NONE,
	RENDER_VIEW_MODE_GEOMETRY_INSTANCE,
	RENDER_VIEW_MODE_GEOMETRY_PRIMITIVE,
	RENDER_VIEW_MODE_GEOMETRY_BARYCENTRICS,
	RENDER_VIEW_MODE_GEOMETRY_NORMAL,
	RENDER_VIEW_MODE_GEOMETRY_TEXCOORD,
	RENDER_VIEW_MODE_MATERIAL_BASE_COLOR,
	RENDER_VIEW_MODE_MATERIAL_NORMAL,
	RENDER_VIEW_MODE_MATERIAL_METALLIC_ROUGHNESS,
	RENDER_VIEW_MODE_MATERIAL_EMISSIVE,
	RENDER_VIEW_MODE_WORLD_NORMAL,
	RENDER_VIEW_MODE_RENDER_TARGET_DEPTH,
	RENDER_VIEW_MODE_COUNT
};

struct render_settings_t
{
	uint use_wavefront_pathtracing;
	uint use_software_rt;
	uint render_view_mode;
	uint max_bounces;
	uint cosine_weighted_diffuse;
	uint accumulate;

	float hdr_env_strength;
};

struct view_t
{
    float4x4 world_to_view; // View matrix
    float4x4 view_to_world; // Inverse view matrix
    float4x4 view_to_clip;  // Projection matrix
    float4x4 clip_to_view;  // Inverse projection matrix

    float2 render_dim; // Render dimensions/resolution in pixels
	float near_plane;
	float far_plane;
};

struct vertex_t
{
	float3 position;
	float3 normal;
	float4 tangent;
	float2 tex_coord;
};

struct triangle_t
{
	vertex_t v0;
	vertex_t v1;
	vertex_t v2;
};

struct material_t
{
	float3 base_color_factor;
	float metallic_factor;
	float roughness_factor;
	float3 emissive_factor;
	float emissive_strength;

	uint base_color_index;
	uint normal_index;
	uint metallic_roughness_index;
	uint emissive_index;
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
