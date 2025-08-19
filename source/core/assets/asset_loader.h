#pragma once
#include "asset_types.h"

namespace asset_loader
{

	texture_asset_t* load_texture_rgba(memory_arena_t& arena, const char* filepath, TEXTURE_FORMAT format);
	texture_asset_t* load_texture_hdr(memory_arena_t& arena, const char* filepath, TEXTURE_FORMAT format);

	scene_asset_t* load_scene(memory_arena_t& arena, const char* filepath);

}
