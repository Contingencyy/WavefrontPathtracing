#pragma once
#include "AssetTypes.h"

#include <filesystem>

namespace AssetLoader
{

	MeshAsset LoadGLTF(const std::filesystem::path& filepath);

}
