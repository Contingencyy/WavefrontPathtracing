#pragma once
#include "core/api_types.h"

/*
	Declare renderer resource handles
*/

DECLARE_HANDLE_TYPE(render_texture_handle_t);
DECLARE_HANDLE_TYPE(render_mesh_handle_t);

/*
	render data structures
*/

struct vertex_t
{
	glm::vec3 position;
	glm::vec3 normal;
};
