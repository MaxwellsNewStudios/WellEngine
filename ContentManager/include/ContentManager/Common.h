#pragma once

#include <cstdint>

#include "WellEngine/Source/Engine/EngineDefinitions.h"

namespace ContentManager
{
	enum class AssetType : uint8_t
	{
		None		= 0,
		Mesh		= 1,
		Texture		= 2,
		Cubemap		= 3,
		Shader		= 4,
		Audio		= 5,
		Scene		= 6,
		Prefab		= 7,
		Font		= 8,

		Count
	};

	enum class ShaderType
	{
		VERTEX_SHADER	= 0,
		HULL_SHADER		= 1,
		DOMAIN_SHADER	= 2,
		GEOMETRY_SHADER = 3,
		PIXEL_SHADER	= 4,
		COMPUTE_SHADER	= 5,
	};

	namespace ShaderTypeUtils
	{
		inline constexpr const char *ShaderTypeToString(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::VERTEX_SHADER:		return "Vertex";
			case ShaderType::HULL_SHADER:		return "Hull";
			case ShaderType::DOMAIN_SHADER:		return "Domain";
			case ShaderType::GEOMETRY_SHADER:	return "Geometry";
			case ShaderType::PIXEL_SHADER:		return "Pixel";
			case ShaderType::COMPUTE_SHADER:	return "Compute";
			default:							return "Unknown";
			}
		}

		inline const ShaderType ShaderTypeFromString(const char *str)
		{
			// Ordered by frequency, for performance
			if (std::strcmp(str, "Pixel") == 0)		return ShaderType::PIXEL_SHADER;
			if (std::strcmp(str, "Vertex") == 0)	return ShaderType::VERTEX_SHADER;
			if (std::strcmp(str, "Compute") == 0)	return ShaderType::COMPUTE_SHADER;
			if (std::strcmp(str, "Geometry") == 0)	return ShaderType::GEOMETRY_SHADER;
			if (std::strcmp(str, "Hull") == 0)		return ShaderType::HULL_SHADER;
			if (std::strcmp(str, "Domain") == 0)	return ShaderType::DOMAIN_SHADER;
			return ShaderType::PIXEL_SHADER;
		}
	}


}