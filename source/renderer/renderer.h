#pragma once
#include "renderer_fwd.h"

struct camera_t;
struct material_t;

namespace renderer
{

	void init(uint32_t render_width, uint32_t render_height);
	void exit();

	void begin_frame();
	void end_frame();

	void begin_scene(const camera_t& scene_camera, render_texture_handle_t env_render_texture_handle);
	void render();
	void end_scene();

	void render_ui();

	struct render_texture_params_t
	{
		uint32_t width;
		uint32_t height;
		uint8_t bytes_per_channel;
		uint8_t channel_count;
		uint8_t* ptr_data;

		const wchar_t* debug_name;
	};
	render_texture_handle_t create_render_texture(const render_texture_params_t& texture_params);

	struct render_mesh_params_t
	{
		uint32_t vertex_count;
		vertex_t* vertices;
		uint32_t index_count;
		uint32_t* indices;

		const wchar_t* debug_name;
	};
	render_mesh_handle_t create_render_mesh(const render_mesh_params_t& mesh_params);
	void submit_render_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material);

}
