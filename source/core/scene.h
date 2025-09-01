#pragma once
#include "core/camera/camera_controller.h"
#include "camera/camera.h"
#include "renderer/renderer_fwd.h"

struct texture_asset_t;
struct scene_asset_t;
struct mesh_asset_t;
struct material_asset_t;

struct scene_object_t
{
	glm::mat4 transform_mat;
	mesh_asset_t* mesh;
	material_asset_t* material;
};

struct scene_t
{
	memory_arena_t arena;
	
	camera_controller_t camera_controller;
	texture_asset_t* hdr_env;
	scene_asset_t* dragon_scene_asset;
	scene_asset_t* damaged_helmet_scene_asset;
	scene_asset_t* sponza_scene_asset;
	scene_asset_t* bistro_exterior_scene_asset;
	scene_asset_t* bistro_interior_scene_asset;
	scene_asset_t* bistro_wine_scene_asset;
	scene_asset_t* sun_temple_scene_asset;

	camera_t camera;

	uint32_t scene_object_count;
	uint32_t scene_object_at;
	// TODO: Add a scene object free-list
	scene_object_t* scene_objects;
};

namespace scene
{

	void create(scene_t& scene);
	void destroy(scene_t& scene);

	void update(scene_t& scene, float dt);
	void render(scene_t& scene);

	void render_ui(scene_t& scene);

};
