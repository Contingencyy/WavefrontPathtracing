#pragma once

/*
	RenderResourceHandle
*/

template<typename T>
union RenderResourceHandle
{
	struct
	{
		u32 Index;
		u32 Version;
	};

	uint64_t Handle = ~0u;

	RenderResourceHandle() = default;
		
	b8 IsValid() const { return Handle != ~0u; }

	inline b8 operator==(const RenderResourceHandle& Rhs) const { return Handle == Rhs.Handle; }
	inline b8 operator!=(const RenderResourceHandle& Lhs) const { return Handle != Lhs.Handle; }
};

using RenderTextureHandle = RenderResourceHandle<struct RenderTextureTag>;
using RenderMeshHandle = RenderResourceHandle<struct RenderMeshTag>;

/*
	Render data structures
*/

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
};
