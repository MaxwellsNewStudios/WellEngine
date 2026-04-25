#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

#include "Engine/EngineSettings.h"
#include "Engine/Content/ContentRegistry.h"

#ifdef _DEBUG
#define DBG_MSG(msg) std::cout << msg
#else
#define DBG_MSG(msg)
#endif

const std::string AssetDir = ASSET_PATH;
const std::string RegistryDir = ASSET_PATH "_Registry\\";
const std::string CompiledDir = ASSET_PATH "_Compiled\\";
const std::string RegistryFile = RegistryDir + "ContentRegistry.json";


int main()
{

	std::cout << "Content Registration Done.\n";
}
