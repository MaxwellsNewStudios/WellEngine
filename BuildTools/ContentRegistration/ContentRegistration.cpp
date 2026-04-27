#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

#include "MeshLoader.h"
#include "TextureLoader.h"

#include "Engine/EngineSettings.h"
#include "Engine/Content/ContentRegistry.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
namespace json = rapidjson;

const std::string SolutionDir = SOLUTION_DIR;
const std::string AssetDir = ASSET_PATH;
const std::string InternalDir = INTERNAL_PATH;
const std::string RegistryDir = INTERNAL_REGISTRY_PATH "\\";
const std::string CompiledDir = INTERNAL_COMPILED_PATH "\\";
const std::string RegistryFile = RegistryDir + "ContentRegistry.json";


int main()
{
	// TODO:
	// - Fetch registerred assets from the registry file using ContentRegistry.
	// - Load & process assets as defined in the registry file.
	// - Write compiled assets to the compiled directory, using the file structure defined in the registry file.

	std::cout << "Content Registration Done.\n";
}
