#pragma once
#include "Renderer/RendererFwd.h"

struct TextureAsset
{
	RenderTextureHandle renderTextureHandle;
};

struct SceneAsset
{
	std::vector<RenderMeshHandle> renderMeshHandles;
};
