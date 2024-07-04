#pragma once
#include "AssetTypes.h"

#include <filesystem>

namespace AssetLoader
{

	TextureAsset LoadImageHDR(const std::filesystem::path& filepath);

	SceneAsset LoadGLTF(const std::filesystem::path& filepath);

}
