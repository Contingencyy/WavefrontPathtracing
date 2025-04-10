#pragma once
#include "asset_types.h"

namespace asset_loader
{

	texture_asset_t* load_image_hdr(memory_arena_t& arena, const char* filepath);

	scene_asset_t* load_gltf(memory_arena_t& arena, const char* filepath);

}
