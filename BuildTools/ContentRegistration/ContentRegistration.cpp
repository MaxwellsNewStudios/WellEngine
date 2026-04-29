#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

#include "Shared.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

#include "Engine/EngineSettings.h"
//#include "Engine/Content/ContentRegistry.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
namespace json = rapidjson;


int main()
{
	// TODO:
	// - Fetch registerred assets from the registry file using ContentRegistry.
	// - Load & process assets as defined in the registry file.
	// - Write compiled assets to the compiled directory, using the file structure defined in the registry file.

	/*
	using namespace WellEngine;
	ContentRegistry &cReg = ContentRegistry::Instance();

	if (!cReg.OpenRegistry())
	{
		return -1;
	}

	auto assetMap = cReg.GetAssetTypeMap();

	{
		auto &meshList = assetMap["mesh"];

	}

	{
		auto &tex2dList = assetMap["tex2d"];

	}

	{
		auto &texcubeList = assetMap["texcube"];

	}

	{
		auto &shaderList = assetMap["shader"];

	}

	// ...

	cReg.CloseRegistry();
	*/

	std::cout << "Content Registration Done.\n";
}
