#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include "rapidjson/document.h"
#include "Engine/EngineSettings.h"

namespace WellEngine
{
	namespace json = rapidjson;

	namespace ContentRegistryTags
	{
		static const char *ASSET_TAG	= "a";
		static const char *FOLDER_TAG	= "f";
		static const char *PATH_TAG		= "p";
		static const char *TYPE_TAG		= "t";
	}

	enum class NameConflictAction : uint8_t
	{
		Fail = 0,
		Override = 1,
		Rename
	};

	enum class RegistryFailState : uint8_t
	{
		None = 0,

		RegistryNotOpen,
		DirNotFound,
		AssetNotFound,
		NameConflict,
		InvalidPath,
		InvalidName,
		InvalidAssetData,
		JsonParseError,
		IOError,
	};

	struct AssetRegistryEntry
	{
		std::string path = "";
		json::Value *asset = nullptr;

		AssetRegistryEntry() = default;
		AssetRegistryEntry(const std::string &path, json::Value *data)
			: path(path), asset(data) {
		}
	};

	class ContentRegistry
	{
	private:
		std::unique_ptr<json::Document> _registryDoc;
		RegistryFailState _failState = RegistryFailState::None;
		bool _dirty = false;

		// TODO: Add working directory memory, for more efficient internal navigation


		ContentRegistry() = default;

		[[nodiscard]] bool InitDir(const std::vector<std::string> &folderChain, json::Value **outFolder = nullptr);
		[[nodiscard]] bool InitDir(const std::string &dir, json::Value **outFolder = nullptr);

		[[nodiscard]] json::Value *NavToFolder(const std::vector<std::string> &folderChain);
		[[nodiscard]] json::Value *NavToFolder(const std::string &path);

		[[nodiscard]] std::string IterateName(const json::Value &folderAssets, const std::string &name);

	public:
		~ContentRegistry();
		ContentRegistry(const ContentRegistry &other) = delete;
		ContentRegistry &operator=(const ContentRegistry &other) = delete;
		ContentRegistry(ContentRegistry &&other) = delete;
		ContentRegistry &operator=(ContentRegistry &&other) = delete;

		[[nodiscard]] static ContentRegistry &Instance()
		{
			static ContentRegistry instance;
			return instance;
		}

		// General
		bool OpenRegistry();
		bool SaveRegistry();
		void CloseRegistry();

		[[nodiscard]] bool Failed() const { return _failState != RegistryFailState::None; }
		[[nodiscard]] RegistryFailState GetFailure() const { return _failState; }

		[[nodiscard]] bool IsOpen() const { return _registryDoc != nullptr; }
		[[nodiscard]] bool IsDirty() const { return _dirty; }

		[[nodiscard]] const json::Document *GetDoc() const;
		[[nodiscard]] json::Document *GetDoc();
		[[nodiscard]] json::Document::AllocatorType *GetDocAlloc();

		// Read
		[[nodiscard]] json::Value *GetAsset(const std::string &path);

		[[nodiscard]] std::unordered_map<std::string, std::vector<AssetRegistryEntry>> GetAssetTypeMap();

		// Write
		bool AddAsset(const std::string &path, json::Value &obj, NameConflictAction conflictAction = NameConflictAction::Fail);
	};
}