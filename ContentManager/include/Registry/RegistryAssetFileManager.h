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

	struct AssetRegistryPair
	{
		std::string assetPath = "";
		RegistryData reg = {};
	};

	void RegisterAsset(const std::string &assetPath, const RegistryData &registry);

	// path = Solution-relative or absolute path to either asset or registry
	[[nodiscard]] RegistryData GetAssetRegistry(const std::string &path);

	[[nodiscard]] std::vector<RegistryData> GetAssetRegistriesInDirectory(const std::string &directoryPath, bool recursive = false);
}