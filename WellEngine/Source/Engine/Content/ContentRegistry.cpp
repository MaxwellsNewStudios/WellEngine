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
		return false;

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
		return false;

	std::vector<std::string> folderChain = SplitPath(dir);

	return InitDir(folderChain, outFolder);
}


bool ContentRegistry::OpenRegistry()
{
	if (IsOpen())
		return true;

	std::ifstream registryFile(GetRegistryPath(), std::ios::in);

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
	_registryDoc = nullptr;
	_dirty = false;
}

bool ContentRegistry::SaveRegistry()
{
	if (!_registryDoc)
		return false;

	if (!_dirty)
		return true; // No changes to save

	// Write doc to file
	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	_registryDoc->Accept(writer);

	std::ofstream registryFile(GetRegistryPath(), std::ios::out);
	if (!registryFile)
	{
		ErrMsg("Could not save registry!");
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


bool ContentRegistry::AddAssetToRegistry(const std::string &path, json::Value &obj, bool override)
{
	if (!IsOpen())
		return false;

	if (path.empty())
		return false;

	std::vector<std::string> folderChain = SplitPath(path);

	std::string assetName = folderChain.back();
	folderChain.pop_back();

	json::Value *containingFolder = nullptr;
	if (!InitDir(folderChain, &containingFolder))
		return false;

	if (containingFolder == nullptr)
		return false;

	json::Document::AllocatorType &docAlloc = _registryDoc->GetAllocator();
	json::Value &assets = (*containingFolder)[ASSET_TAG];

	if (assets.HasMember(assetName.c_str()))
	{
		if (!override)
			return false;

		// Replace existing asset with same name
		assets[assetName.c_str()] = obj;
	}
	else
	{
		assets.AddMember(json::Value(assetName.c_str(), docAlloc), obj, docAlloc);
	}

	_dirty = true;
	return true;
}
