#include "stdafx.h"
#include "ContentRegistry.h"

using namespace WellEngine;
using namespace SerializerUtils;
using namespace WellEngine::ContentRegistryTags;


ContentRegistry::~ContentRegistry()
{
	if (!IsOpen())
		return;

	if (IsDirty())
		Warn("Destroying registry with unsaved changes!");

	CloseRegistry();
}

#pragma region Internal
// Split path into folder chain, separated by '/' or '\\'
static std::vector<std::string> SplitPath(const std::string &path)
{
	std::vector<std::string> result;
	std::string currentPart;

	for (char c : path)
	{
		if (c != '/' && c != '\\')
		{
			currentPart += c;
			continue;
		}

		if (currentPart.empty())
			continue;

		if (currentPart == "." || currentPart == "..")
			WarnF("Path includes '{}', which is not supported. Was the path formatted correctly? Path: {}", currentPart, path);
		
		result.emplace_back(currentPart);
		currentPart.clear();
	}

	if (!currentPart.empty())
		result.emplace_back(currentPart);

	return result;
}

static void InitFolder(json::Value *dirObj, json::Document::AllocatorType &docAlloc)
{
	json::Value folderObj(json::kObjectType);
	dirObj->AddMember(SerializeString(FOLDER_TAG, docAlloc), folderObj, docAlloc);

	json::Value assetObj(json::kObjectType);
	dirObj->AddMember(SerializeString(ASSET_TAG, docAlloc), assetObj, docAlloc);
}

bool ContentRegistry::InitDir(const std::vector<std::string> &folderChain, json::Value **outFolder)
{
	if (!IsOpen())
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return false;
	}

	json::Document::AllocatorType &docAlloc = _registryDoc->GetAllocator();
	json::Value *currDir = _registryDoc.get();

	if (folderChain.empty())
	{
		if (outFolder)
			*outFolder = currDir;

		return true;
	}

	// Start at root & navigate to dir, initializing any directories along the way that don't exist
	for (const std::string &folder : folderChain)
	{
		json::Value &folders = (*currDir)[FOLDER_TAG];

		if (!folders.HasMember(folder.c_str()))
		{
			json::Value folderObj(json::kObjectType);
			InitFolder(&folderObj, docAlloc);
			folders.AddMember(json::Value(folder.c_str(), docAlloc), folderObj, docAlloc);

			_dirty = true;
		}

		currDir = &folders[folder.c_str()];
	}

	if (outFolder)
		*outFolder = currDir;

	return true;
}

bool ContentRegistry::InitDir(const std::string &dir, json::Value **outFolder)
{
	if (!IsOpen())
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return false;
	}

	std::vector<std::string> folderChain = SplitPath(dir);

	return InitDir(folderChain, outFolder);
}

json::Value *ContentRegistry::NavToFolder(const std::vector<std::string> &folderChain)
{
	if (!IsOpen())
		return nullptr;

	json::Value *currDir = _registryDoc.get();
	for (const std::string &folder : folderChain)
	{
		json::Value &folders = (*currDir)[FOLDER_TAG];
		if (!folders.HasMember(folder.c_str()))
			return nullptr;
		currDir = &folders[folder.c_str()];
	}

	return currDir;
}

json::Value *ContentRegistry::NavToFolder(const std::string &path)
{
	if (!IsOpen())
		return nullptr;

	return NavToFolder(SplitPath(path));
}

std::string ContentRegistry::IterateName(const json::Value &folderAssets, const std::string &name)
{
	if (!folderAssets.HasMember(name.c_str()))
		return name; // Name doesn't exist, original is first available iteration

	// If name already ends with "_#", extract base name for search, and begin search at #.
	int iterator = 1;
	std::string baseName = name;

	size_t underscorePos = baseName.find_last_of('_');
	if (underscorePos != std::string::npos && underscorePos != 0 && underscorePos != baseName.size() - 1)
	{
		std::string suffix = baseName.substr(underscorePos + 1);

		if (std::all_of(suffix.begin(), suffix.end(), ::isdigit))
		{
			iterator = std::stoi(suffix) + 1;
			baseName = baseName.substr(0, underscorePos);
		}
	}

	// Iterate until we find an available name
	for (;; ++iterator)
	{
		std::string iterName = std::format("{}_{}", baseName, iterator);
		if (!folderAssets.HasMember(iterName.c_str()))
			return iterName;
	}

}
#pragma endregion // Internal

#pragma region General
bool ContentRegistry::OpenRegistry()
{
	_failState = RegistryFailState::None;

	if (IsOpen())
		return true;

	std::ifstream registryFile(WE_F_REGISTRY, std::ios::in);

	if (registryFile.is_open())
	{
		// Initialize with contents of registry file
		std::string fileContents;
		registryFile.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(registryFile)), std::istreambuf_iterator<char>());
		registryFile.close();

		_registryDoc = std::make_unique<json::Document>();
		_registryDoc->Parse(fileContents.c_str());

		if (_registryDoc->HasParseError())
		{
			ErrMsgF("Failed to parse JSON file: {}", (UINT)_registryDoc->GetParseError());
			_failState = RegistryFailState::JsonParseError;
			return false;
		}
	}
	else
	{
		// if no registry file exists, create an empty one
		_registryDoc = std::make_unique<json::Document>();
		_registryDoc->SetObject();
		InitFolder(_registryDoc.get(), _registryDoc->GetAllocator());
		_dirty = false;
	}

	return true;
}

void ContentRegistry::CloseRegistry()
{
	_failState = RegistryFailState::None;

	_registryDoc = nullptr;
	_dirty = false;
}

bool ContentRegistry::SaveRegistry()
{
	_failState = RegistryFailState::None;

	if (!_registryDoc)
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return false;
	}

	if (!_dirty)
		return true; // No changes to save

	// Write doc to file
	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	_registryDoc->Accept(writer);

	std::ofstream registryFile(WE_F_REGISTRY, std::ios::out);
	if (!registryFile)
	{
		ErrMsg("Could not save registry!");
		_failState = RegistryFailState::IOError;
		return false;
	}

	registryFile << buffer.GetString();
	registryFile.close();

	_dirty = false;
	return true;
}

const json::Document *ContentRegistry::GetDoc() const
{
	if (!IsOpen())
		return nullptr;
	return _registryDoc.get();
}

json::Document *ContentRegistry::GetDoc()
{
	if (!IsOpen())
		return nullptr;
	_dirty = true; // Can't know if the caller is modifying the doc, so assume they are and mark dirty
	return _registryDoc.get();
}

json::Document::AllocatorType *ContentRegistry::GetDocAlloc()
{
	if (!IsOpen())
		return nullptr;
	return &_registryDoc->GetAllocator();
}
#pragma endregion // General

#pragma region Read
json::Value *ContentRegistry::GetAsset(const std::string &path)
{
	_failState = RegistryFailState::None;

	if (!IsOpen())
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return nullptr;
	}

	std::vector<std::string> folderChain = SplitPath(path);
	std::string assetName = folderChain.back();
	folderChain.pop_back();

	json::Value *containingFolder = NavToFolder(folderChain);

	if (containingFolder == nullptr)
	{
		_failState = RegistryFailState::DirNotFound;
		return nullptr;
	}

	json::Value &assets = (*containingFolder)[ASSET_TAG];
	if (!assets.HasMember(assetName.c_str()))
	{
		_failState = RegistryFailState::AssetNotFound;
		return nullptr;
	}

	return &assets[assetName.c_str()];
}

// Recursively find all assets and add them to a map keyed by asset type (stored in each asset's "type" field)
std::unordered_map<std::string, std::vector<AssetRegistryEntry>> ContentRegistry::GetAssetTypeMap()
{
	_failState = RegistryFailState::None;

	if (!IsOpen())
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return {};
	}

	std::unordered_map<std::string, std::vector<AssetRegistryEntry>> result;

	std::function<void(json::Value &, std::string)> recurseFolders = [&](json::Value &folder, std::string path)
	{
		if (!path.empty())
			path += "\\";

		json::Value &assets = folder[ASSET_TAG];
		for (json::Value::MemberIterator it = assets.MemberBegin(); it != assets.MemberEnd(); ++it)
		{
			AssetRegistryEntry entry = {
				path + it->name.GetString(),
				&it->value
			};

			if (!entry.asset->HasMember(TYPE_TAG))
			{
				_failState = RegistryFailState::InvalidAssetData;
				WarnF("Asset {} is missing type field!", entry.path);
				continue;
			}

			std::string type = (*entry.asset)[TYPE_TAG].GetString();
			result[type].push_back(entry);
		}

		json::Value &subFolders = folder[FOLDER_TAG];
		for (json::Value::MemberIterator it = subFolders.MemberBegin(); it != subFolders.MemberEnd(); ++it)
		{
			recurseFolders(it->value, path + it->name.GetString());
		}
	};

	recurseFolders(*_registryDoc, "");

	return result;
}
#pragma endregion // Read

#pragma region Write
bool ContentRegistry::AddAsset(const std::string &path, json::Value &obj, NameConflictAction conflictAction)
{
	_failState = RegistryFailState::None;

	if (!IsOpen())
	{
		_failState = RegistryFailState::RegistryNotOpen;
		return false;
	}

	if (path.empty())
	{
		_failState = RegistryFailState::InvalidPath;
		return false;
	}

	std::vector<std::string> folderChain = SplitPath(path);

	std::string assetName = folderChain.back();
	folderChain.pop_back();

	json::Value *containingFolder = nullptr;
	if (!InitDir(folderChain, &containingFolder))
		return false;

	if (containingFolder == nullptr)
	{
		_failState = RegistryFailState::DirNotFound;
		return false;
	}

	json::Document::AllocatorType &docAlloc = _registryDoc->GetAllocator();
	json::Value &assets = (*containingFolder)[ASSET_TAG];

	bool doAdd = true;
	if (assets.HasMember(assetName.c_str()))
	{
		switch (conflictAction)
		{
		case NameConflictAction::Fail:
			_failState = RegistryFailState::NameConflict;
			return false; // Don't add asset, and report failure

		case NameConflictAction::Override:
			doAdd = false;
			assets[assetName.c_str()] = obj; // Replace existing asset with same name
			break;

		case NameConflictAction::Rename:
			assetName = IterateName(assets, assetName);
			break;

		default:
			break;
		}
	}
	
	if (doAdd)
		assets.AddMember(json::Value(assetName.c_str(), docAlloc), obj, docAlloc);

	_dirty = true;
	return true;
}
#pragma endregion // Write
