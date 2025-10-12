#include "stdafx.h"
#include "FontAtlas.h"

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
