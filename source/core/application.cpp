#include "application.h"

#include "core/logger.h"
#include "core/scene.h"
#include "core/input.h"

#include "platform/platform.h"
#include "renderer/renderer.h"

#include "imgui/imgui.h"

namespace application
{

	static bool s_should_close = false;

	struct instance_t
	{
		memory_arena_t arena;
		
		scene_t* active_scene;
		double delta_time;
		bool running = false;
	} static *inst;

	static void handle_events()
	{
		s_should_close = !platform::window_poll_events();

		if (input::is_mouse_captured())
		{
			if (input::is_key_pressed(input::KEYCODE_RIGHT_MOUSE))
			{
				platform::window_set_capture_mouse(false);
			}
		}
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			if (!input::is_mouse_captured() && input::is_key_pressed(input::KEYCODE_LEFT_MOUSE))
			{
				platform::window_set_capture_mouse(true);
			}
		}
	}

	static void update()
	{
		scene::update(*inst->active_scene, inst->delta_time);
	}

	static void render_ui()
	{
		ImGui::Begin("General");

		uint32_t fps = 1.0f / inst->delta_time;
		float frametime_ms = inst->delta_time * 1000.0f;

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

	void init(memory_arena_t& arena, const command_line_args_t& cmd_args)
	{
		LOG_INFO("Application", "Init");

		platform::window_create(cmd_args.window_width, cmd_args.window_height);
		inst = ARENA_ALLOC_STRUCT_ZERO(arena, instance_t);
		inst->arena = arena;

		int32_t client_width = 0, client_height = 0;
		platform::window_get_client_area(client_width, client_height);

		// We could do one arena per system eventually, but for now everything that needs
		// to live for as long as the application does will use the same arena
		renderer::init_params_t renderer_init = {};
		renderer_init.render_width = client_width;
		renderer_init.render_height = client_height;
		renderer_init.backbuffer_count = 2u;
		renderer_init.vsync = false;
		renderer::init(renderer_init);

		inst->active_scene = ARENA_ALLOC_STRUCT_ZERO(inst->arena, scene_t);
		scene::create(*inst->active_scene);

		inst->running = true;
	}

	void exit()
	{
		LOG_INFO("Application", "Exit");

		renderer::exit();
		scene::destroy(*inst->active_scene);
		inst->active_scene = nullptr;

		ARENA_RELEASE(inst->arena);
	}

	void run()
	{
		timer_t time_curr = platform::get_ticks();
		timer_t time_prev = platform::get_ticks();

		while (inst->running && !s_should_close)
		{
			time_curr = platform::get_ticks();
			inst->delta_time = platform::get_elapsed_seconds(time_prev, time_curr);

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
