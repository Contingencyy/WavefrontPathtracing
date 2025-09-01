#pragma once
#include "asset_types.h"

namespace asset_loader
{

	texture_asset_t* load_texture(memory_arena_t& arena, const char* filepath, bool srgb = true);
	scene_asset_t* load_scene(memory_arena_t& arena, const char* filepath);

}
