#pragma once
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <d3d11.h>

#include "ContentManager/Common.h"

namespace fs = std::filesystem;

namespace ContentManager::Registry
{
	// Structs for asset properties. These are serialized into the registry file's properties vector.

	struct AssetPropertiesMesh
	{

	};

	struct AssetPropertiesTexture
	{
		// DX type, mipmapped, downsample number
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		bool mipmapped = true;
		int downsample = 0;
	};

	struct AssetPropertiesShader
	{
		ShaderType type = ShaderType::VERTEX_SHADER;
	};

	struct AssetPropertiesAudio
	{

	};
}

namespace ContentManager::Registry
{
	// Manages registry files used to register assets and their properties.

	struct RegistryHeader
	{
		AssetType assetType = AssetType::None;
		std::string alias = "";
		std::string assetPath = ""; // Relative to solution directory
		std::string registryPath = ""; // Relative to registry directory
		std::string compiledPath = ""; // Relative to compiled directory
		fs::file_time_type compileTime = {};
	};

	struct RegistryData
	{
		RegistryHeader header = {};
		std::vector<char> properties = {};
	};

	void RegisterAsset(const std::string &assetPath, const RegistryData &registry);

	// path = Solution-relative or absolute path to either asset or registry
	[[nodiscard]] RegistryData GetAssetRegistry(const std::string &path);

	[[nodiscard]] std::vector<RegistryData> GetAssetRegistriesInDirectory(const std::string &directoryPath, bool recursive = false);
}