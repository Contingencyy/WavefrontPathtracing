#include "pch.h"
#include "application.h"

#include "core/logger.h"
#include "core/scene.h"
#include "core/input.h"

#include "renderer/renderer.h"

#include "imgui/imgui.h"

#include <chrono>

void create_window(int32_t desired_width, int32_t desired_height);
void get_window_client_area(int32_t& out_window_width, int32_t& out_window_height);
void set_window_capture_mouse(bool capture);
bool poll_window_events();

namespace application
{

	static bool s_should_close = false;

	struct instance_t
	{
		memory_arena_t arena;

		scene_t* active_scene;

		std::chrono::duration<float> delta_time = std::chrono::duration<float>(0.0f);
		bool running = false;
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
		scene::update(*inst->active_scene, inst->delta_time.count());
	}

	static void render_ui()
	{
		ImGui::Begin("General");

		uint32_t fps = 1.0f / inst->delta_time.count();
		float frametime_ms = inst->delta_time.count() * 1000.0f;

		ImGui::Text("FPS: %u", fps);
		ImGui::Text("Frametime: %.3f ms", frametime_ms);

		ImGui::End();

		scene::render_ui(*inst->active_scene);
		renderer::render_ui();
	}

	static void render()
	{
		renderer::begin_frame();
		scene::render(*inst->active_scene);

		render_ui();

		renderer::end_frame();
	}

	void init(const command_line_args_t& cmd_args)
	{
		LOG_INFO("Application", "Init");

		create_window(cmd_args.window_width, cmd_args.window_height);
		inst = ARENA_BOOTSTRAP(instance_t, 0);

		int32_t client_width = 0, client_height = 0;
		get_window_client_area(client_width, client_height);

		// We could do one arena per system eventually, but for now everything that needs
		// to live for as long as the application does will use the same arena
		renderer::init(client_width, client_height);

		inst->active_scene = ARENA_ALLOC_STRUCT_ZERO(&inst->arena, scene_t);
		scene::create(*inst->active_scene);

		inst->running = true;
	}

	void exit()
	{
		LOG_INFO("Application", "Exit");

		renderer::exit();
		scene::destroy(*inst->active_scene);
		inst->active_scene = nullptr;

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

	bool should_close()
	{
		return s_should_close;
	}

}
