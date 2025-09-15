#pragma once

#include <string>
#include "Source/Engine/Utils/ReferenceHelper.h"
#include "Source/Engine/Utils/UIDHelper.h"

class Asset : public IRefTarget<Asset>, public Identifiable
{
private:
	std::string _name;
	std::string _storagePath;

protected:
	bool _isLoaded = false;

public:
	Asset(std::string_view name, std::string_view path) : _name(name), _storagePath(path) { }
	virtual ~Asset() = default;

	[[nodiscard]] const std::string &GetName() const noexcept
	{
		return _name;
	}
	[[nodiscard]] const std::string &GetPath() const noexcept
	{
		return _storagePath;
	}

	virtual void Load() = 0;
	virtual void Unload() = 0;
	[[nodiscard]] bool IsLoaded() const noexcept
	{
		return _isLoaded;
	}
};
