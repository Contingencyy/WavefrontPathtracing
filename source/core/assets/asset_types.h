#pragma once
#include "renderer/renderer_fwd.h"

struct texture_asset_t
{
	render_texture_handle_t render_texture_handle;
};

struct scene_asset_t
{
	render_mesh_handle_t* render_mesh_handles;
	u32 mesh_handle_count;
};
