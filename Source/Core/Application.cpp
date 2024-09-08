#include "pch.h"
#include "application.h"

#include "core/logger.h"
#include "core/scene.h"
#include "core/input.h"

#include "renderer/renderer.h"

#include "imgui/imgui.h"

#include <chrono>

void get_window_client_area(i32& out_window_width, i32& out_window_height);
void set_window_capture_mouse(b8 capture);
b8 poll_window_events();

namespace application
{

	static b8 s_should_close = false;

	struct instance_t
	{
		memory_arena_t arena;

		scene_t* active_scene;

		std::chrono::duration<f32> delta_time = std::chrono::duration<f32>(0.0f);
		b8 running = false;
	} static *inst;

	static void handle_events()
	{
		s_should_close = !poll_window_events();

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			if (!input::is_mouse_captured() && input::is_key_pressed(input::KeyCode_LeftMouse))
			{
				set_window_capture_mouse(true);
			}
		}
		if (input::is_mouse_captured())
		{
			if (input::is_key_pressed(input::KeyCode_RightMouse))
			{
				set_window_capture_mouse(false);
			}
		}
	}

	static void update()
	{
		inst->active_scene->update(inst->delta_time.count());
	}

	static void render_ui()
	{
		ImGui::Begin("General");

		u32 fps = 1.0f / inst->delta_time.count();
		f32 frametime_ms = inst->delta_time.count() * 1000.0f;

		ImGui::Text("FPS: %u", fps);
		ImGui::Text("Frametime: %.3f ms", frametime_ms);

		ImGui::End();

		inst->active_scene->render_ui();
		renderer::render_ui();
	}

	static void render()
	{
		renderer::begin_frame();
		inst->active_scene->render();
		render_ui();
		renderer::end_frame();
	}

	void init()
	{
		LOG_INFO("Application", "Init");

		inst = ARENA_BOOTSTRAP(instance_t, 0);

		i32 client_width = 0, client_height = 0;
		get_window_client_area(client_width, client_height);

		// We could do one arena per system eventually, but for now everything that needs
		// to live for as long as the application does will use the same arena
		renderer::init(client_width, client_height);

		inst->active_scene = ARENA_ALLOC_STRUCT_ZERO(&inst->arena, scene_t);
		inst->active_scene->init();

		inst->running = true;
	}

	void exit()
	{
		LOG_INFO("Application", "Exit");

		renderer::exit();

		inst->active_scene->destroy();
		ARENA_RELEASE(&inst->arena);
	}

	void run()
	{
		std::chrono::high_resolution_clock::time_point time_curr = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point time_prev = std::chrono::high_resolution_clock::now();

		while (inst->running && !s_should_close)
		{
			time_curr = std::chrono::high_resolution_clock::now();
			inst->delta_time = time_curr - time_prev;

			handle_events();
			update();
			render();

			time_prev = time_curr;
		}
	}

	b8 should_close()
	{
		return s_should_close;
	}

}
