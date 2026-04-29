#pragma once

#include <string>

#include "Engine/EngineSettings.h"

namespace ContentRegistration
{
	const std::string SolutionDir = TO_SOLUTION_PATH;
	const std::string AssetDir = WE_D_ASSET;
	const std::string InternalDir = WE_D_INTERNAL;
	const std::string RegistryDir =  WE_D_REGISTRY "\\";
	const std::string CompiledDir =  WE_D_COMPILED "\\";
	const std::string RegistryFile = RegistryDir + "ContentRegistry.json";
}