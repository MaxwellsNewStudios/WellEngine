#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <intsafe.h>
#include <unordered_map>
#include "./Asset.h"


struct AssetFile
{
	std::string name;
	std::string path;

	std::chrono::file_clock::time_point lastModified;
	AssetFolder *folder = nullptr;

	bool isLoaded = false;
	UINT iconID = -1;
	AssetType type = AssetType::Unknown;

	AssetFile(std::string name, AssetFolder *folder, AssetType type)
		: name(std::move(name)), folder(folder), type(type)
	{
		if (folder)
			path = folder->GetFullPath() + "/" + name;
	}
};

struct AssetFolder
{
	std::string name;

	std::chrono::file_clock::time_point lastModified;
	AssetFolder *parent = nullptr;

	std::unordered_map<std::string, AssetFolder> folders;
	std::unordered_map<std::string, AssetFile> files;

	AssetFolder(std::string name, AssetFolder *parent) : name(std::move(name)), parent(parent) {}

	[[nodiscard]] std::string GetFullPath() const
	{
		if (parent)
			return parent->GetFullPath() + "/" + name;
		return name;
	}

	void Clear()
	{
		folders.clear();
		files.clear();
	}

	[[nodiscard]] AssetFolder *AddFolder(const std::string &name)
	{
		auto it = folders.find(name);
		if (it != folders.end())
			return &it->second;

		auto [newIt, success] = folders.emplace(name, AssetFolder(name, this));
		if (success)
			return &newIt->second;

		return nullptr;
	}

	[[nodiscard]] AssetFile *AddFile(const std::string &name, AssetType type)
	{
		auto it = files.find(name);
		if (it != files.end())
			return nullptr;

		auto [newIt, success] = files.emplace(name, AssetFile(name, this, type));
		if (success)
			return &newIt->second;

		return nullptr;
	}
};


class AssetDirectory
{
private:
	AssetFolder _rootFolder = AssetFolder("Assets", nullptr);

public:
	AssetDirectory() = default;
	~AssetDirectory() = default;

	static inline AssetDirectory &Get()
	{
		static AssetDirectory instance;
		return instance;
	}
};