#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace ContentManager::Registry
{
	// Manages registry files used to register assets and their properties.

	struct RegistryHeader
	{
		std::string assetType = "";
		std::string assetPath = "";
		std::string alias = "";
		std::string compiledPath = "";
		fs::file_time_type compileTime = {};
	};

	struct RegistryData
	{
		RegistryHeader header = {};
		std::vector<char> properties = {};
	};

	struct AssetRegistryPair
	{
		std::string assetPath = "";
		RegistryData reg = {};
	};

	void RegisterAsset(const std::string &assetPath, const RegistryData &registry);

	// Solution-relative or absolute path to either asset or registry
	[[nodiscard]] RegistryData GetAssetRegistry(const std::string &path);

	[[nodiscard]] std::vector<std::string> GetRegisteredAssetsInDirectory(const std::string &directoryPath, bool recursive = false);
}