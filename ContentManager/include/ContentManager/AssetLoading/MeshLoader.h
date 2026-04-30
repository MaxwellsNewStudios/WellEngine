#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <DirectXMath.h>

#include "ContentManager/Common.h"
#include "ContentManager/AssetData/MeshData.h"

namespace ContentManager::AssetLoader
{
	AssetData::Mesh LoadMesh(const std::string &path);
	AssetData::SkinnedMesh LoadSkinnedMesh(const std::string &path);
}
