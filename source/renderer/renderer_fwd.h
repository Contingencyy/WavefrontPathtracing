#pragma once

/*
	render_resource_handle_t
*/

template<typename T>
union render_resource_handle_t
{
	struct
	{
		u32 index;
		u32 version;
	};

	uint64_t handle = ~0u;

	render_resource_handle_t() = default;
		
	b8 is_valid() const { return handle != ~0u; }

	inline b8 operator==(const render_resource_handle_t& rhs) const { return handle == rhs.handle; }
	inline b8 operator!=(const render_resource_handle_t& rhs) const { return handle != rhs.handle; }
};

using render_texture_handle_t = render_resource_handle_t<struct render_texture_tag_t>;
using render_mesh_handle_t = render_resource_handle_t<struct render_mesh_tag_t>;

/*
	render data structures
*/

struct vertex_t
{
	glm::vec3 position;
	glm::vec3 normal;
};
