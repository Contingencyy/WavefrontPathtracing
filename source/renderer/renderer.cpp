#include "pch.h"
#include "renderer.h"
#include "renderer_common.h"
#include "cpupathtracer.h"
#include "dx12/dx12_backend.h"

#include "acceleration_structure/bvh.h"
#include "acceleration_structure/bvh_instance.h"
#include "acceleration_structure/tlas.h"

#include "core/camera/camera.h"
#include "core/logger.h"

#include "resource_slotmap.h"

#include "imgui/imgui.h"

namespace renderer
{

	renderer_inst_t* g_renderer = nullptr;

	static render_settings_t get_default_render_settings()
	{
		render_settings_t defaults = {};
		defaults.render_view_mode = RenderViewMode_None;
		defaults.ray_max_recursion = 8;

		defaults.cosine_weighted_diffuse = true;
		defaults.russian_roulette = true;

		defaults.hdr_env_intensity = 1.0f;

		defaults.postfx.max_white = 10.0f;
		defaults.postfx.exposure = 1.0f;
		defaults.postfx.contrast = 1.0f;
		defaults.postfx.brightness = 0.0f;
		defaults.postfx.saturation = 1.0f;
		defaults.postfx.linear_to_srgb = true;

		return defaults;
	}

	void init(u32 render_width, u32 render_height)
	{
		LOG_INFO("Renderer", "Init");

		g_renderer = ARENA_BOOTSTRAP(renderer_inst_t, 0);

		g_renderer->render_width = render_width;
		g_renderer->render_height = render_height;
		g_renderer->inv_render_width = 1.0f / static_cast<f32>(g_renderer->render_width);
		g_renderer->inv_render_height = 1.0f / static_cast<f32>(g_renderer->render_height);

		g_renderer->texture_slotmap.init(&g_renderer->arena);
		g_renderer->bvh_slotmap.init(&g_renderer->arena);

		g_renderer->bvh_instances_count = 100;
		g_renderer->bvh_instances_at = 0;
		g_renderer->bvh_instances = ARENA_ALLOC_ARRAY_ZERO(&g_renderer->arena, bvh_instance_t, g_renderer->bvh_instances_count);
		g_renderer->bvh_instances_materials = ARENA_ALLOC_ARRAY_ZERO(&g_renderer->arena, material_t, g_renderer->bvh_instances_count);

		// Default render settings
		g_renderer->settings = get_default_render_settings();

		// The path tracer and dx12 backend use the same arena as the front-end renderer since the renderer initializes them and they have the same lifetimes
		cpupathtracer::init(&g_renderer->arena);
		dx12_backend::init(&g_renderer->arena);
	}

	void exit()
	{
		g_renderer->texture_slotmap.destroy();
		g_renderer->bvh_slotmap.destroy();

		dx12_backend::exit();
		cpupathtracer::exit();
	}

	void begin_frame()
	{
		dx12_backend::begin_frame();
		cpupathtracer::begin_frame();
	}

	void end_frame()
	{
		cpupathtracer::end_frame();

		dx12_backend::end_frame();
		dx12_backend::present();

		g_renderer->frame_index++;
	}

	void begin_scene(const camera_t& scene_camera, render_texture_handle_t env_render_texture_handle)
	{
		cpupathtracer::begin_scene(scene_camera);

		g_renderer->scene_camera = scene_camera;
		g_renderer->scene_hdr_env_texture = g_renderer->texture_slotmap.find(env_render_texture_handle);
		ASSERT(g_renderer->scene_hdr_env_texture);
	}

	void render()
	{
		cpupathtracer::render();
	}

	void end_scene()
	{
		g_renderer->bvh_instances_at = 0;

		cpupathtracer::end_scene();
	}

	void render_ui()
	{
		if (ImGui::Begin("Renderer"))
		{
			b8 should_reset_accumulators = false;

			//ImGui::Text("render time: %.3f ms", Profiler::GetTimerResult(GlobalProfilingScope_RenderTime).lastElapsed * 1000.0f);

			ImGui::Text("Resolution: %ux%u", g_renderer->render_width, g_renderer->render_height);

			// Debug category
			if (ImGui::CollapsingHeader("Debug"))
			{
				ImGui::Indent(10.0f);

				// Render data visualization
				ImGui::Text("Render data visualization mode");
				if (ImGui::BeginCombo("##Render data visualization mode", render_view_mode_labels[g_renderer->settings.render_view_mode], ImGuiComboFlags_None))
				{
					for (u32 i = 0; i < RenderViewMode_Count; ++i)
					{
						b8 selected = i == g_renderer->settings.render_view_mode;

						if (ImGui::Selectable(render_view_mode_labels[i], selected))
						{
							g_renderer->settings.render_view_mode = (RenderViewMode)i;
							should_reset_accumulators = true;
						}

						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::Unindent(10.0f);
			}

			// Render Settings
			if (ImGui::CollapsingHeader("Settings"))
			{
				ImGui::Indent(10.0f);

				if (ImGui::SliderInt("Max ray Recursion Depth: %u", reinterpret_cast<i32*>(&g_renderer->settings.ray_max_recursion), 0, 8)) should_reset_accumulators = true;

				// Enable/disable cosine weighted diffuse reflections, russian roulette
				if (ImGui::Checkbox("Cosine weighted diffuse", &g_renderer->settings.cosine_weighted_diffuse)) should_reset_accumulators = true;
				if (ImGui::Checkbox("Russian roulette", &g_renderer->settings.russian_roulette)) should_reset_accumulators = true;

				// HDR environment emissive_intensity
				if (ImGui::DragFloat("HDR env emissive_intensity", &g_renderer->settings.hdr_env_intensity, 0.001f)) should_reset_accumulators = true;

				// Post-process Settings, constract, brightness, saturation, sRGB
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
				if (ImGui::CollapsingHeader("Post-processing"))
				{
					ImGui::Indent(10.0f);

					if (ImGui::SliderFloat("Max white", &g_renderer->settings.postfx.max_white, 0.0f, 100.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Exposure", &g_renderer->settings.postfx.exposure, 0.0f, 100.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Contrast", &g_renderer->settings.postfx.contrast, 0.0f, 3.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Brightness", &g_renderer->settings.postfx.brightness, -1.0f, 1.0f)) should_reset_accumulators = true;
					if (ImGui::SliderFloat("Saturation", &g_renderer->settings.postfx.saturation, 0.0f, 10.0f)) should_reset_accumulators = true;
					if (ImGui::Checkbox("Linear to SRGB", &g_renderer->settings.postfx.linear_to_srgb)) should_reset_accumulators = true;

					ImGui::Unindent(10.0f);
				}

				ImGui::Unindent(10.0f);
			}

			if (ImGui::CollapsingHeader("CPU Pathtracer"))
			{
				cpupathtracer::render_ui(should_reset_accumulators);
			}
		}

		ImGui::End();
	}

	render_texture_handle_t create_render_texture(u32 width, u32 height, f32* ptr_pixel_data)
	{
		texture_t texture = {};
		texture.width = width;
		texture.height = height;
		texture.ptr_pixel_data = ARENA_ALLOC_ARRAY(&g_renderer->arena, f32, width * height * 4);
		memcpy(texture.ptr_pixel_data, ptr_pixel_data, width * height * sizeof(float) * 4);

		render_texture_handle_t handle = g_renderer->texture_slotmap.add(std::move(texture));
		return handle;
	}

	render_mesh_handle_t create_render_mesh(vertex_t* vertices, u32 vertex_count, u32* indices, u32 index_count)
	{
		bvh_t::build_options_t build_opts = {};

		bvh_t mesh_bvh = {};
		mesh_bvh.build(&g_renderer->arena, vertices, vertex_count, indices, index_count, build_opts);

		render_mesh_handle_t handle = g_renderer->bvh_slotmap.add(std::move(mesh_bvh));
		return handle;
	}

	void submit_render_mesh(render_mesh_handle_t render_mesh_handle, const glm::mat4& transform, const material_t& material)
	{
		const bvh_t* bvh = g_renderer->bvh_slotmap.find(render_mesh_handle);
		if (!bvh)
			return;

		ASSERT(g_renderer->bvh_instances_at < g_renderer->bvh_instances_count);

		bvh_instance_t* instance = &g_renderer->bvh_instances[g_renderer->bvh_instances_at];
		instance->set_bvh(bvh);
		instance->set_transform(transform);

		material_t* instance_material = &g_renderer->bvh_instances_materials[g_renderer->bvh_instances_at];
		*instance_material = material;

		g_renderer->bvh_instances_at++;
	}

}
