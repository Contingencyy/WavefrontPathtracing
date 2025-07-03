#pragma once
#include "renderer/renderer_fwd.h"

struct texture_asset_t
{
	render_texture_handle_t render_texture_handle;
};

struct material_asset_t
{
	glm::vec4 base_color_factor;
	float metallic_factor;
	float roughness_factor;
	glm::vec3 emissive_factor;
	float emissive_strength;
	texture_asset_t base_color_texture;
	texture_asset_t normal_texture;
	texture_asset_t metallic_roughness_texture;
	texture_asset_t emissive_texture;
};

struct mesh_asset_t
{
	render_mesh_handle_t render_mesh_handle;
};

struct scene_node_t
{
	glm::mat4 transform;
	uint32_t* children;
	uint32_t child_count;
	uint32_t* mesh_indices;
	uint32_t* material_indices;
	uint32_t mesh_count;
};

struct scene_asset_t
{
	material_asset_t* material_assets;
	uint32_t material_asset_count;
	mesh_asset_t* mesh_assets;
	uint32_t mesh_asset_count;

	uint32_t* root_node_indices;
	uint32_t root_node_count;
	scene_node_t* nodes;
	uint32_t node_count;
};
