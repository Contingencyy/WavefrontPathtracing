#pragma once

/*
	RenderResourceHandle
*/

template<typename T>
union RenderResourceHandle
{
	struct
	{
		uint32_t index;
		uint32_t version;
	};

	uint64_t handle = ~0u;

	RenderResourceHandle() = default;
		
	bool IsValid() const { return handle != ~0u; }

	inline bool operator==(const RenderResourceHandle& rhs) const { return handle == rhs.handle; }
	inline bool operator!=(const RenderResourceHandle& rhs) const { return handle != rhs.handle; }
};

using RenderTextureHandle = RenderResourceHandle<struct RenderTextureTag>;
using RenderMeshHandle = RenderResourceHandle<struct RenderMeshTag>;

/*
	Render data structures
*/

struct Vertex
{
	glm::vec3 position;
};
