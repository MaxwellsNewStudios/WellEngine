#include "RegistryAssetFileManager.h"
#include "ContentManager/Internal/Internal.h"
#include <fstream>
#include <chrono>

using namespace ContentManager;

void Registry::RegisterAsset(const std::string &assetPath, const RegistryData &registry)
{
	// Verify file exists
	fs::path assetFilePath(assetPath);
	if (!fs::exists(assetFilePath) || !fs::is_regular_file(assetFilePath))
		return;

	// Ensure paths are relative
	fs::path relAssetPath = fs::relative(assetFilePath, TO_SOLUTION_PATH);

	fs::path relRegistryPath(registry.header.registryPath);
	if (relRegistryPath.empty())
	{
		relRegistryPath = relAssetPath;
		relRegistryPath += "." + registry.header.alias + "." WE_E_REGISTRY;
	}

	fs::path relCompiledPath = fs::relative(registry.header.compiledPath, WE_D_COMPILED);

	// Create registry file with same name and registry extension
	// in identical folder structure from solution directory in the registry directory
	fs::path registryFilePath = fs::path(WE_D_REGISTRY) / relRegistryPath;

	// Create parent directories if they don't exist
	fs::create_directories(registryFilePath.parent_path());

	// Write registry data to file
	std::ofstream registryFile;
	registryFile.open(registryFilePath);
	if (!registryFile.is_open())
		return;

	// Write header
	registryFile << static_cast<char>(registry.header.assetType) << "\n";
	registryFile << registry.header.alias << "\n";
	registryFile << relAssetPath.string() << "\n";
	registryFile << relRegistryPath.string() << "\n";
	registryFile << relCompiledPath.string() << "\n";
	registryFile << registry.header.compileTime.time_since_epoch().count() << "\n";

	// Write properties
	registryFile << registry.properties.size() << "\n";
	registryFile.write(registry.properties.data(), registry.properties.size());

	registryFile.close();
}

Registry::RegistryData Registry::GetAssetRegistry(const std::string &path)
{
	fs::path registryFilePath(path);

	if (registryFilePath.is_absolute()) // Handle absolute
		registryFilePath = fs::relative(registryFilePath, TO_SOLUTION_PATH);

	if (registryFilePath.extension() == "." WE_E_REGISTRY) // Registry path
	{
		registryFilePath = fs::path(TO_SOLUTION_PATH) / registryFilePath;
	}
	else // Asset path
	{
		registryFilePath = fs::path(WE_D_REGISTRY) / registryFilePath;
		registryFilePath += "." WE_E_REGISTRY;
	}

	if (!fs::exists(registryFilePath))
		return {};

	if (!fs::is_regular_file(registryFilePath))
		return {};

	std::ifstream registryFile;
	registryFile.open(registryFilePath);
	if (!registryFile.is_open())
		return {}; // Return empty if the registry file couldn't be opened
	
	RegistryData registry;

	// Read header
	char assetTypeChar;
	registryFile >> assetTypeChar;
	registry.header.assetType = static_cast<AssetType>(assetTypeChar);
	registryFile.ignore(); // Ignore newline

	std::getline(registryFile, registry.header.alias);
	std::getline(registryFile, registry.header.assetPath);
	std::getline(registryFile, registry.header.registryPath);
	std::getline(registryFile, registry.header.compiledPath);

	std::string compileTimeStr;
	std::getline(registryFile, compileTimeStr);
	long long compileTimeCount = std::stoll(compileTimeStr);
	registry.header.compileTime = fs::file_time_type(fs::file_time_type::duration(compileTimeCount));

	// Read properties
	size_t propertiesSize = 0;
	registryFile >> propertiesSize;
	registryFile.ignore(); // Ignore newline

	registry.properties.resize(propertiesSize);
	registryFile.read(registry.properties.data(), propertiesSize);
	registryFile.close();

	return registry;
}

std::vector<Registry::RegistryData> Registry::GetAssetRegistriesInDirectory(const std::string &directoryPath, bool recursive)
{
	fs::path dirPath(directoryPath);
	if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
		return {}; // Return empty if the directory doesn't exist

	std::vector<RegistryData> registeredAssets;
	fs::path rootPath(TO_SOLUTION_PATH);

	auto checkEntryFunc = [&](const fs::directory_entry &entry) {
		if (!entry.is_regular_file())
			return;

		if (entry.path().extension() != "." WE_E_REGISTRY)
			return;

		std::string relPath = fs::relative(entry.path(), rootPath).string();

		registeredAssets.push_back(GetAssetRegistry(relPath));
	};

	if (recursive)
	{
		for (const auto &entry : fs::recursive_directory_iterator(dirPath))
			checkEntryFunc(entry);
	}
	else
	{
		for (const auto &entry : fs::directory_iterator(dirPath))
			checkEntryFunc(entry);
	}

	return registeredAssets;
}
