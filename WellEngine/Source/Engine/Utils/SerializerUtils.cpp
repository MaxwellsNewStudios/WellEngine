#include "stdafx.h"
#include "SerializerUtils.h"

json::Value SerializerUtils::SerializeString(const std::string &str, json::Document::AllocatorType &alloc)
{
	json::Value strVal(json::kStringType);
	strVal.SetString(str.c_str(), static_cast<json::SizeType>(str.length()), alloc);
	return strVal;
}

json::Value SerializerUtils::SerializeVec(const dx::XMFLOAT2 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMFLOAT3 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMFLOAT4 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	arr.PushBack(vec.w, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMINT2 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMINT3 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMINT4 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	arr.PushBack(vec.w, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMUINT2 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMUINT3 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const dx::XMUINT4 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	arr.PushBack(vec.w, alloc);
	return arr;
}
#ifdef USE_IMGUI
json::Value SerializerUtils::SerializeVec(const ImVec2 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	return arr;
}
json::Value SerializerUtils::SerializeVec(const ImVec4 &vec, json::Document::AllocatorType &alloc)
{
	json::Value arr(json::kArrayType);
	arr.PushBack(vec.x, alloc);
	arr.PushBack(vec.y, alloc);
	arr.PushBack(vec.z, alloc);
	arr.PushBack(vec.w, alloc);
	return arr;
}
#endif

void SerializerUtils::DeserializeVec(dx::XMFLOAT2 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 2)
		throw std::runtime_error("Invalid JSON array for XMFLOAT2 deserialization.");

	vec.x = val[0].GetFloat();
	vec.y = val[1].GetFloat();
}
void SerializerUtils::DeserializeVec(dx::XMFLOAT3 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 3)
		throw std::runtime_error("Invalid JSON array for XMFLOAT3 deserialization.");

	vec.x = val[0].GetFloat();
	vec.y = val[1].GetFloat();
	vec.z = val[2].GetFloat();
}
void SerializerUtils::DeserializeVec(dx::XMFLOAT4 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 4)
		throw std::runtime_error("Invalid JSON array for XMFLOAT4 deserialization.");

	vec.x = val[0].GetFloat();
	vec.y = val[1].GetFloat();
	vec.z = val[2].GetFloat();
	vec.w = val[3].GetFloat();
}
void SerializerUtils::DeserializeVec(dx::XMINT2 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 2)
		throw std::runtime_error("Invalid JSON array for XMINT2 deserialization.");

	vec.x = val[0].GetInt();
	vec.y = val[1].GetInt();
}
void SerializerUtils::DeserializeVec(dx::XMINT3 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 3)
		throw std::runtime_error("Invalid JSON array for XMINT3 deserialization.");

	vec.x = val[0].GetInt();
	vec.y = val[1].GetInt();
	vec.z = val[2].GetInt();
}
void SerializerUtils::DeserializeVec(dx::XMINT4 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 4)
		throw std::runtime_error("Invalid JSON array for XMINT4 deserialization.");

	vec.x = val[0].GetInt();
	vec.y = val[1].GetInt();
	vec.z = val[2].GetInt();
	vec.w = val[3].GetInt();
}
void SerializerUtils::DeserializeVec(dx::XMUINT2 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 2)
		throw std::runtime_error("Invalid JSON array for XMINT2 deserialization.");

	vec.x = val[0].GetUint();
	vec.y = val[1].GetUint();
}
void SerializerUtils::DeserializeVec(dx::XMUINT3 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 3)
		throw std::runtime_error("Invalid JSON array for XMINT3 deserialization.");

	vec.x = val[0].GetUint();
	vec.y = val[1].GetUint();
	vec.z = val[2].GetUint();
}
void SerializerUtils::DeserializeVec(dx::XMUINT4 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 4)
		throw std::runtime_error("Invalid JSON array for XMINT4 deserialization.");

	vec.x = val[0].GetUint();
	vec.y = val[1].GetUint();
	vec.z = val[2].GetUint();
	vec.w = val[3].GetUint();
}
#ifdef USE_IMGUI
void SerializerUtils::DeserializeVec(ImVec2 &vec, const json::Value &val) 
{
	if (!val.IsArray() || val.Size() != 2)
		throw std::runtime_error("Invalid JSON array for ImVec2 deserialization.");

	vec.x = val[0].GetFloat();
	vec.y = val[1].GetFloat();
}
void SerializerUtils::DeserializeVec(ImVec4 &vec, const json::Value &val)
{
	if (!val.IsArray() || val.Size() != 4)
		throw std::runtime_error("Invalid JSON array for ImVec4 deserialization.");

	vec.x = val[0].GetFloat();
	vec.y = val[1].GetFloat();
	vec.z = val[2].GetFloat();
	vec.w = val[3].GetFloat();
}
#endif
