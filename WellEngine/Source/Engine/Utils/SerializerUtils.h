#pragma once
#include <vector>
#include <string>
#include "rapidjson/document.h"

namespace json = rapidjson;

namespace SerializerUtils
{
	json::Value SerializeString(const std::string &str, json::Document::AllocatorType &alloc);

	json::Value SerializeVec(const dx::XMFLOAT2 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMFLOAT3 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMFLOAT4 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMINT2 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMINT3 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMINT4 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMUINT2 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMUINT3 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const dx::XMUINT4 &vec, json::Document::AllocatorType &alloc);
#ifdef USE_IMGUI
	json::Value SerializeVec(const ImVec2 &vec, json::Document::AllocatorType &alloc);
	json::Value SerializeVec(const ImVec4 &vec, json::Document::AllocatorType &alloc);
#endif

	void DeserializeVec(dx::XMFLOAT2 &vec, const json::Value &val);
	void DeserializeVec(dx::XMFLOAT3 &vec, const json::Value &val);
	void DeserializeVec(dx::XMFLOAT4 &vec, const json::Value &val);
	void DeserializeVec(dx::XMINT2 &vec, const json::Value &val);
	void DeserializeVec(dx::XMINT3 &vec, const json::Value &val);
	void DeserializeVec(dx::XMINT4 &vec, const json::Value &val);
	void DeserializeVec(dx::XMUINT2 &vec, const json::Value &val);
	void DeserializeVec(dx::XMUINT3 &vec, const json::Value &val);
	void DeserializeVec(dx::XMUINT4 &vec, const json::Value &val);
#ifdef USE_IMGUI
	void DeserializeVec(ImVec2 &vec, const json::Value &val);
	void DeserializeVec(ImVec4 &vec, const json::Value &val);
#endif
};