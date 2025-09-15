#pragma once

#include <string>
#include "Source/Engine/Utils/ReferenceHelper.h"
#include "Source/Engine/Utils/UIDHelper.h"

enum class AssetType
{
	Unknown,
	Texture,
	Mesh,
	Audio,
	Shader,
	Text,
	Json,

	COUNT
};

class Asset : public IRefTarget<Asset>, public Identifiable
{
private:
	std::string _name;
	std::string _storagePath;
	AssetType _type = AssetType::Unknown;

protected:
	bool _isLoaded = false;

public:
	Asset(std::string_view name, std::string_view path, AssetType type) : _name(name), _storagePath(path), _type(type) { }
	virtual ~Asset() = default;

	[[nodiscard]] const std::string &GetName() const noexcept	{ return _name; }
	[[nodiscard]] const std::string &GetPath() const noexcept	{ return _storagePath; }
	[[nodiscard]] AssetType GetType() const noexcept			{ return _type; }

	virtual void Load() = 0;
	virtual void Unload() = 0;
	[[nodiscard]] bool IsLoaded() const noexcept
	{
		return _isLoaded;
	}
};
