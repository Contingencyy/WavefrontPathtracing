#pragma once
#include "renderer/renderer_fwd.h"

struct camera_t;
struct material_t;

namespace cpupathtracer
{

	void init(u32 render_width, u32 render_height);
	void exit();

	void begin_frame();
	void end_frame();

	void begin_scene(const camera_t& scene_camera, render_texture_handle_t env_render_texture_handle);
	void render();
	void end_scene();
	
	void render_ui();

	render_texture_handle_t create_texture(u32 width, u32 height, f32* ptr_pixel_data);
	render_mesh_handle_t create_mesh(vertex_t* vertices, u32 vertex_count, u32* indices, u32 index_count);
	void submit_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material);

}
