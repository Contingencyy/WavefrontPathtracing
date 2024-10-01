#pragma once
#include "renderer_fwd.h"

struct camera_t;

namespace gpupathtracer
{

	void init(memory_arena_t* arena);
	void exit();

	void begin_frame();
	void end_frame();

	void begin_scene(const camera_t& scene_camera);
	void render();
	void end_scene();

	void render_ui(b8 reset_accumulation);

}
