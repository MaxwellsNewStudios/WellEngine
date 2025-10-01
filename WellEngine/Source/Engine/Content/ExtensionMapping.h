#pragma once

#include <unordered_map>
#include <string>
#include <algorithm>
#include "./Asset.h"

namespace ExtensionMapping
{
	static const std::unordered_map<const std::string, const AssetType> AssetTypeExtMap = {
		{ "png",		AssetType::Texture },
		{ "dds",		AssetType::Texture },
		{ "jpg",		AssetType::Texture },
		{ "jpeg",		AssetType::Texture },
		{ "obj",		AssetType::Mesh },
		{ "wav",		AssetType::Audio },
		{ "hlsl",		AssetType::Shader },
		{ "hlsli",		AssetType::Shader },
		{ "txt",		AssetType::Text },
		{ "json",		AssetType::Json }
	};

	static inline const AssetType GetAssetTypeFromExtension(std::string_view ext)
	{
		if (ext.empty())
			return AssetType::Unknown;

		std::string extStr((ext[0] == '.') ? ext.substr(1) : ext);
		std::transform(extStr.begin(), extStr.end(), extStr.begin(), static_cast<int(*)(int)>(std::tolower));

		auto it = AssetTypeExtMap.find(extStr);
		if (it != AssetTypeExtMap.end())
			return it->second;

		return AssetType::Unknown;
	}
};
