#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "Internal/Internal.h"

namespace fs = std::filesystem;

namespace ContentManager::Registry
{
	// Manages registry (.reg) files used to register assets and their properties.

	struct RegistryHeader
	{
		std::string assetType;
		std::string compiledPath = "";
		fs::file_time_type compileTime = fs::file_time_type::min();
	};

	struct RegistryData
	{
		RegistryHeader header;
		std::vector<char> properties;
	};

	void RegisterAsset(const std::string &assetPath, const RegistryData &registry);

	[[nodiscard]] RegistryData GetAssetRegistry(const std::string &assetPath);

	[[nodiscard]] std::vector<std::string> GetRegisteredAssetsInDirectory(const std::string &directoryPath, bool recursive = false);
}