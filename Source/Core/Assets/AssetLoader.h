#pragma once
#include "AssetTypes.h"

namespace AssetLoader
{

	TextureAsset* LoadImageHDR(MemoryArena* Arena, const char* Filepath);

	SceneAsset* LoadGLTF(MemoryArena* Arena, const char* Filepath);

}
