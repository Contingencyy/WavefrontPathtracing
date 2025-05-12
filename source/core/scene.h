#pragma once
#include "core/camera/camera_controller.h"
#include "camera/camera.h"

#include "renderer/renderer_fwd.h"
#include "renderer/material.h"

struct texture_asset_t;
struct scene_asset_t;

struct scene_object_t
{
	render_mesh_handle_t render_mesh_handle;
	glm::mat4 transform_mat;
	material_t material;
};

struct scene_t
{
	memory_arena_t arena;
	
	camera_controller_t camera_controller;
	texture_asset_t* hdr_env;
	scene_asset_t* dragon_mesh;
	scene_asset_t* sponza_mesh;

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
