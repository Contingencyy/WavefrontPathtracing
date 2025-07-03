#pragma once
#include "core/api_types.h"

/*
	Declare renderer resource handles
*/

DECLARE_HANDLE_TYPE(render_texture_handle_t);
DECLARE_HANDLE_TYPE(render_mesh_handle_t);

/*
	Texture format
*/

enum TEXTURE_FORMAT : uint32_t
{
	TEXTURE_FORMAT_RG8,
	TEXTURE_FORMAT_RGBA8,
	TEXTURE_FORMAT_RGBA8_SRGB,
	TEXTURE_FORMAT_RGBA32_FLOAT,

	TEXTURE_FORMAT_UNKNOWN = 0xFFFFFFFF
};
