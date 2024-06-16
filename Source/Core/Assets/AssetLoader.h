#pragma once
#include "AssetTypes.h"

#include <filesystem>

namespace AssetLoader
{

	SceneAsset LoadGLTF(const std::filesystem::path& filepath);

}
