#pragma once
#include "core/camera/camera_controller.h"
#include "renderer/renderer_fwd.h"
#include "renderer/raytracing_utils.h"

struct texture_asset_t;
struct scene_asset_t;

struct scene_object_t
{
	render_mesh_handle_t render_mesh_handle;
	glm::mat4 transform_mat;
	material_t material;
};

class scene_t
{
public:
	/*static void CreateAndInit();
	static void DestroyAndDeinit();*/
	void init();
	void destroy();

	void update(f32 DeltaTime);
	void render();

	void render_ui();

private:
	void create_scene_object(render_mesh_handle_t render_mesh_handle, const material_t& material,
		const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);

private:
	memory_arena_t m_arena;

	camera_controller_t m_camera_controller;

	// TODO: free-list for scene objects
	u32 m_scene_object_count;
	u32 m_scene_object_at;
	scene_object_t* m_scene_objects;

	texture_asset_t* m_hdr_env_texture_asset;
	scene_asset_t* m_dragon_scene_asset;

};
