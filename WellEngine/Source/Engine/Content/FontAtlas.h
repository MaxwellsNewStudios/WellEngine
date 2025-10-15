#pragma once

#include <intsafe.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d11.h>
#include "rapidjson/document.h"
#include "Source/Engine/D3D/MeshD3D11.h"

namespace dx = DirectX;
namespace json = rapidjson;

class Content;

struct GlyphVertex
{
	dx::XMFLOAT2 position;
	dx::XMFLOAT2 uv;
};

class GlyphData
{
public:
	dx::XMFLOAT4 uvRect; // Atlas texture coordinates (min-max)
	dx::XMFLOAT2 size;   // Size of the glyph in pixels
	dx::XMFLOAT2 offset; // Offset from the cursor position to the top-left of the glyph
	float advance;       // How much to move the cursor after drawing this glyph

	GlyphData() : uvRect(0, 0, 0, 0), size(0, 0), offset(0, 0), advance(0) {}

	// Generate vertices for this glyph and append them to the output vector
	void ToVerts(std::vector<GlyphVertex> &out, dx::XMFLOAT2 &cursor) const
	{
		float x = cursor.x - offset.x;
		float y = cursor.y - offset.y;
		float w = size.x;
		float h = size.y;

		// Define the four corners of the glyph quad
		GlyphVertex topLeft     = { { x,     y,    }, { uvRect.x, uvRect.y } };
		GlyphVertex topRight    = { { x + w, y,    }, { uvRect.z, uvRect.y } };
		GlyphVertex bottomRight = { { x + w, y + h }, { uvRect.z, uvRect.w } };
		GlyphVertex bottomLeft  = { { x,     y + h }, { uvRect.x, uvRect.w } };

		// Two triangles for the quad
		out.push_back(topLeft);
		out.push_back(bottomLeft);
		out.push_back(topRight);

		out.push_back(topRight);
		out.push_back(bottomLeft);
		out.push_back(bottomRight);

		// Advance the cursor position
		cursor.x += advance;
	}

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) const;
	[[nodiscard]] bool Deserialize(const json::Value &obj);
};

class FontAtlas
{
private:
	UINT _fontTextureID = -1;
	UINT _fallbackGlyphID = -1;
	std::unordered_map<UINT, GlyphData> _glyphs;
	std::string _fontName;
	float _lineHeight = 18.0f;
	float _spacing = 12.0f;

	std::unordered_map<size_t, std::function<void(void)>> _modifyCallback;

#ifdef USE_IMGUI
	std::vector<UINT> _selectedGlyphIDs;
#endif

	void AppendGlyph(UINT codepoint, std::vector<GlyphVertex> &vertices, dx::XMFLOAT2 &cursor) const;

public:
	FontAtlas() = default;
	~FontAtlas() = default;

	[[nodiscard]] bool Initialize(const Content *content, std::string name);

	const GlyphData *GetGlyph(UINT codepoint) const;
	UINT GetFontTextureID() const { return _fontTextureID; }

	std::vector<GlyphVertex> Generate(std::wstring_view text) const;
	std::vector<GlyphVertex> Generate(std::string_view text) const;

	MeshData *ToMesh(const std::vector<GlyphVertex> &verts) const;

	[[nodiscard]] bool Serialize(std::string_view fileName, const Content *content) const;
	[[nodiscard]] bool Deserialize(std::string_view fileName, const Content *content);

	bool AddListener(size_t id, std::function<void(void)> func);
	bool RemoveListener(size_t id);

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI(const Content *content);
#endif
};