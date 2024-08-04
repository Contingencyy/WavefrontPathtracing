#pragma once
#include "Renderer/RendererFwd.h"

struct TextureAsset
{
	RenderTextureHandle RTextureHandle;
};

struct SceneAsset
{
	RenderMeshHandle* RMeshHandles;
	u32 MeshHandleCount;
};
