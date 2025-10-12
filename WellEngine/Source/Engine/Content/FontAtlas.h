#include <intsafe.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

struct GlyphVertex
{
	dx::XMFLOAT3 position;
	dx::XMFLOAT2 uv;
};

struct Glyph
{
	UINT codepoint;
	dx::XMFLOAT4 uvRect; // Atlas texture coordinates (min-max)
	dx::XMFLOAT2 size;   // Size of the glyph in pixels
	dx::XMFLOAT2 offset; // Offset from the cursor position to the top-left of the glyph
	float advance;       // How much to move the cursor after drawing this glyph

	void ToVerts(std::vector<GlyphVertex> &out, dx::XMFLOAT2 &cursor) const
	{
		float x = cursor.x + offset.x;
		float y = cursor.y + offset.y;
		float w = size.x;
		float h = size.y;

		// Define the four corners of the glyph quad
		GlyphVertex topLeft     = { { x,     y,     0.0f }, { uvRect.x, uvRect.y } };
		GlyphVertex topRight    = { { x + w, y,     0.0f }, { uvRect.z, uvRect.y } };
		GlyphVertex bottomRight = { { x + w, y + h, 0.0f }, { uvRect.z, uvRect.w } };
		GlyphVertex bottomLeft  = { { x,     y + h, 0.0f }, { uvRect.x, uvRect.w } };

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
};

class FontAtlas
{
private:
	UINT _fontTextureID;
	UINT _fallbackGlyphID = -1;
	std::unordered_map<UINT, Glyph> _glyphs;

	void AppendGlyph(UINT codepoint, std::vector<GlyphVertex> &vertices, dx::XMFLOAT2 &cursor, float lineHeight) const;

public:
	FontAtlas() = default;
	~FontAtlas() = default;

	const Glyph *GetGlyph(UINT codepoint) const;
	UINT GetFontTextureID() const { return _fontTextureID; }

	std::vector<GlyphVertex> Generate(std::string_view text, float lineHeight) const;
	std::vector<GlyphVertex> Generate(std::wstring_view text, float lineHeight) const;
};