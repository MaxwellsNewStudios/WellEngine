#include "stdafx.h"
#include "FontAtlas.h"


bool Glyph::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) const
{
	obj.AddMember("UV Rect", SerializerUtils::SerializeVec(uvRect, docAlloc), docAlloc);
	obj.AddMember("Size", SerializerUtils::SerializeVec(size, docAlloc), docAlloc);
	obj.AddMember("Offset", SerializerUtils::SerializeVec(offset, docAlloc), docAlloc);
	obj.AddMember("Advance", advance, docAlloc);

	return true;
}
bool Glyph::Deserialize(const json::Value &obj)
{
	SerializerUtils::DeserializeVec(uvRect, obj["UV Rect"]);
	SerializerUtils::DeserializeVec(size, obj["Size"]);
	SerializerUtils::DeserializeVec(offset, obj["Offset"]);
	advance = obj["Advance"].GetFloat();

	return true;
}


void FontAtlas::AppendGlyph(UINT codepoint, std::vector<GlyphVertex> &vertices, dx::XMFLOAT2 &cursor, float lineHeight) const
{
	const Glyph *glyph = GetGlyph(codepoint);
	if (!glyph)
	{
		WarnF("Missing glyph for codepoint: {}", codepoint);
		return;
	}

	glyph->ToVerts(vertices, cursor);
}

const Glyph *FontAtlas::GetGlyph(UINT codepoint) const
{
	auto it = _glyphs.find(codepoint);
	if (it != _glyphs.end())
		return &it->second;
	
	if (_fallbackGlyphID != CONTENT_NULL)
	{
		auto fallbackIt = _glyphs.find(_fallbackGlyphID);
		if (fallbackIt != _glyphs.end())
			return &fallbackIt->second;
	}

	return nullptr;
}

std::vector<GlyphVertex> FontAtlas::Generate(std::string_view text, float lineHeight) const
{
	std::vector<GlyphVertex> vertices;
	dx::XMFLOAT2 cursor{ 0.0f, 0.0f };

	for (char c : text)
	{
		UINT codepoint = static_cast<UINT>(static_cast<unsigned char>(c));

		switch (codepoint)
		{
		case '\n':
			cursor = { 0.0f, cursor.y + lineHeight };
			break;

		case '\r':
			break;

		default:
			AppendGlyph(codepoint, vertices, cursor, lineHeight);
			break;
		}
	}

	return vertices;
}
std::vector<GlyphVertex> FontAtlas::Generate(std::wstring_view text, float lineHeight) const
{
	std::vector<GlyphVertex> vertices;
	dx::XMFLOAT2 cursor{ 0.0f, 0.0f };

	for (wchar_t c : text)
	{
		UINT codepoint = static_cast<UINT>(c);

		switch (codepoint)
		{
		case '\n':
			cursor = { 0.0f, cursor.y + lineHeight };
			break;

		case '\r':
			break;

		default:
			AppendGlyph(codepoint, vertices, cursor, lineHeight);
			break;
		}
	}

	return vertices;
}

bool FontAtlas::Serialize(std::string_view fileName, const Content *content) const
{
	// Create JSON document
	json::Document doc;
	json::Document::AllocatorType &docAlloc = doc.GetAllocator();

	json::Value atlasObj(json::kObjectType);
	{
		atlasObj.AddMember("Name", SerializerUtils::SerializeString(_fontName, docAlloc), docAlloc);
		atlasObj.AddMember("Texture", SerializerUtils::SerializeString(content->GetTextureName(_fontTextureID), docAlloc), docAlloc);
		atlasObj.AddMember("Fallback Glyph", _fallbackGlyphID, docAlloc);

		// Serialize glyphs to array
		json::Value glyphsArr(json::kArrayType);
		for (const auto &[codepoint, glyph] : _glyphs)
		{
			json::Value glyphObj(json::kObjectType);
			glyphObj.AddMember("Codepoint", codepoint, docAlloc);
			if (!glyph.Serialize(docAlloc, glyphObj))
			{
				ErrMsg("Could not serialize glyph!");
				return false;
			}
			glyphsArr.PushBack(glyphObj, docAlloc);
		}
		atlasObj.AddMember("Glyphs", glyphsArr, docAlloc);
	}
	doc.SetObject().AddMember("Atlas", atlasObj, docAlloc);

	// Write doc to file
	std::ofstream file(PATH_FILE_EXT(ASSET_PATH_FONTS, fileName, "atlas"), std::ios::out);
	if (!file)
	{
		ErrMsg("Could not save atlas!");
		return false;
	}

	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file << buffer.GetString();
	file.close();

	return true;
}
bool FontAtlas::Deserialize(std::string_view fileName, const Content *content)
{
	json::Document doc;
	{
		// Check that the atlas file exists
		std::ifstream atlasFile(PATH_FILE_EXT(ASSET_PATH_FONTS, fileName, "atlas"));

		if (!atlasFile.is_open())
			return; // Atlas doesn't exist

		std::string fileContents;
		atlasFile.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(atlasFile)), std::istreambuf_iterator<char>());
		atlasFile.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
		{
			ErrMsgF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
			return false;
		}
	}

	json::Value &atlas = doc["Atlas"];
	{
		std::string memberName = "";

		memberName = "Name";
		if (atlas.HasMember(memberName.c_str()))
			_fontName = atlas[memberName.c_str()].GetString();

		memberName = "Texture";
		if (atlas.HasMember(memberName.c_str()))
		{
			std::string texName = atlas[memberName.c_str()].GetString();
			_fontTextureID = content->GetTextureID(texName);
		}

		memberName = "Fallback Glyph";
		if (atlas.HasMember(memberName.c_str()))
			_fallbackGlyphID = atlas[memberName.c_str()].GetUint();

		memberName = "Glyphs";
		if (atlas.HasMember(memberName.c_str()))
		{
			const json::Value &glyphsArr = atlas[memberName.c_str()];
			if (glyphsArr.IsArray())
			{
				for (json::SizeType i = 0; i < glyphsArr.Size(); i++)
				{
					const json::Value &glyphObj = glyphsArr[i];
					if (glyphObj.IsObject())
					{
						UINT codepoint = CONTENT_NULL;
						if (glyphObj.HasMember("Codepoint"))
							codepoint = glyphObj["Codepoint"].GetUint();

						Glyph glyph{};
						if (!glyph.Deserialize(glyphObj))
						{
							ErrMsg("Could not deserialize glyph!");
							return false;
						}

						if (codepoint != CONTENT_NULL)
							_glyphs[codepoint] = glyph;
					}
				}
			}
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool FontAtlas::RenderUI(const Content *content)
{
	
}
#endif
