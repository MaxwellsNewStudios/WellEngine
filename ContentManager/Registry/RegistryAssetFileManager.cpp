#include "RegistryAssetFileManager.h"
#include <fstream>
#include <chrono>

using namespace ContentManager;

void Registry::RegisterAsset(const std::string &assetPath, const RegistryData &registry)
{
	// Verify file exists
	fs::path assetFilePath(assetPath);
	if (!fs::exists(assetFilePath) || !fs::is_regular_file(assetFilePath))
		return;

	// Create registry file
	fs::path registryFilePath = assetFilePath;
	registryFilePath.replace_extension(REGISTRY_EXT);

	// Write registry data to file
	std::ofstream registryFile;
	registryFile.open(registryFilePath);
	if (!registryFile.is_open())
		return;

	// Ensure compiled path is relative to the solution directory
	fs::path rootPath(TO_SOLUTION_PATH);
	fs::path compiledPath(registry.header.compiledPath);
	fs::path relCompiledPath = fs::relative(compiledPath, rootPath);

	// Write header
	registryFile << registry.header.assetType << "\n";
	registryFile << relCompiledPath.string() << "\n";
	registryFile << registry.header.compileTime.time_since_epoch().count() << "\n";

	// Write properties
	registryFile << registry.properties.size() << "\n";
	registryFile.write(registry.properties.data(), registry.properties.size());

	registryFile.close();
}

Registry::RegistryData Registry::GetAssetRegistry(const std::string &assetPath)
{
	fs::path registryFilePath(assetPath);
	registryFilePath.replace_extension(REGISTRY_EXT);

	if (!fs::exists(registryFilePath) || !fs::is_regular_file(registryFilePath))
		return {}; // Return empty if the registry file doesn't exist

	std::ifstream registryFile;
	registryFile.open(registryFilePath);
	if (!registryFile.is_open())
		return {}; // Return empty if the registry file couldn't be opened

	RegistryData registry;

	// Read header
	std::getline(registryFile, registry.header.assetType);
	std::getline(registryFile, registry.header.compiledPath);

	// Convert compiled path from relative to absolute
	fs::path absCompiledPath = fs::absolute(registry.header.compiledPath);
	registry.header.compiledPath = absCompiledPath.string();

	std::string compileTimeStr;
	std::getline(registryFile, compileTimeStr);
	long long compileTimeCount = std::stoll(compileTimeStr);
	registry.header.compileTime = fs::file_time_type(fs::file_time_type::duration(compileTimeCount));

	// Read properties
	size_t propertiesSize = 0;
	registryFile >> propertiesSize;

	registryFile.ignore(); // Ignore the newline after the size

	registry.properties.resize(propertiesSize);
	registryFile.read(registry.properties.data(), propertiesSize);
	registryFile.close();

	return registry;
}

std::vector<std::string> Registry::GetRegisteredAssetsInDirectory(const std::string &directoryPath, bool recursive)
{
	fs::path dirPath(directoryPath);
	if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
		return {}; // Return empty if the directory doesn't exist

	std::vector<std::string> registeredAssets;
	fs::path rootPath(TO_SOLUTION_PATH);

	auto checkEntryFunc = [&](const fs::directory_entry &entry) {
		if (!entry.is_regular_file())
			return;

		if (entry.path().extension() != REGISTRY_EXT)
			return;

		fs::path relativePath = fs::relative(entry.path(), rootPath);
		registeredAssets.emplace_back(relativePath.string());
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
