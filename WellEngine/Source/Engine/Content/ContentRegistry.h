#pragma once

#include <vector>
#include <memory>
#include <string>

#include "rapidjson/document.h"
#include "Engine/EngineSettings.h"

namespace WellEngine
{
	namespace json = rapidjson;

	namespace ContentRegistryTags
	{
		static const char *ASSET_TAG	= "a";
		static const char *FOLDER_TAG	= "f";
		static const char *NAME_TAG		= "n";
		static const char *TYPE_TAG		= "t";
	}

	class ContentRegistry
	{
	private:
		std::unique_ptr<json::Document> _registryDoc;

		bool _dirty = false;


		ContentRegistry() = default;

		[[nodiscard]] bool InitDir(const std::vector<std::string> &folderChain, json::Value **outFolder = nullptr);
		[[nodiscard]] bool InitDir(const std::string &dir, json::Value **outFolder = nullptr);

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

		[[nodiscard]] static const std::string &GetRegistryPath()
		{
			static const std::string path = ASSET_REGISTRY_PATH "\\ContentRegistry.json";
			return path;
		}


		// General
		[[nodiscard]] bool OpenRegistry();
		[[nodiscard]] bool SaveRegistry();
		void CloseRegistry();

		[[nodiscard]] bool IsOpen() const { return _registryDoc != nullptr; }
		[[nodiscard]] bool IsDirty() const { return _dirty; }

		[[nodiscard]] const json::Document *GetDoc() const;
		[[nodiscard]] json::Document *GetDoc();
		[[nodiscard]] json::Document::AllocatorType *GetDocAlloc();


		// Read


		// Write
		[[nodiscard]] bool AddAssetToRegistry(const std::string &path, json::Value &obj, bool override = false);
	};
}