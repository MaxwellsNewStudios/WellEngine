#include "stdafx.h"
#include "FontAtlas.h"
#include "Source/Engine/Content/DefaultVertex.h"

bool GlyphData::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) const
{
	obj.AddMember("UV Rect", SerializerUtils::SerializeVec(uvRect, docAlloc), docAlloc);
	obj.AddMember("Size", SerializerUtils::SerializeVec(size, docAlloc), docAlloc);
	obj.AddMember("Offset", SerializerUtils::SerializeVec(offset, docAlloc), docAlloc);
	obj.AddMember("Advance", advance, docAlloc);

	return true;
}
bool GlyphData::Deserialize(const json::Value &obj)
{
	SerializerUtils::DeserializeVec(uvRect, obj["UV Rect"]);
	SerializerUtils::DeserializeVec(size, obj["Size"]);
	SerializerUtils::DeserializeVec(offset, obj["Offset"]);
	advance = obj["Advance"].GetFloat();

	return true;
}

bool FontAtlas::Initialize(const Content *content, std::string name)
{
	// If file exists, load it
	if (Deserialize(name, content))
		return true;

	// Otherwise, create a new default atlas
	_fontName = std::move(name);

	return true;
}

void FontAtlas::AppendGlyph(UINT codepoint, std::vector<GlyphVertex> &vertices, dx::XMFLOAT2 &cursor) const
{
	const GlyphData *glyph = GetGlyph(codepoint);
	if (!glyph)
	{
		WarnF("Missing glyph for codepoint: {}", codepoint);
		return;
	}

	glyph->ToVerts(vertices, cursor);
}

const GlyphData *FontAtlas::GetGlyph(UINT codepoint) const
{
	if (_glyphs.empty())
		return nullptr;

	auto it = _glyphs.find(codepoint);
	if (it != _glyphs.end())
		return &it->second;
	
	if (_fallbackGlyphID != CONTENT_NULL)
	{
		auto fallbackIt = _glyphs.find(_fallbackGlyphID);
		if (fallbackIt != _glyphs.end())
			return &fallbackIt->second;
	}

	return &_glyphs.begin()->second;
}

dx::XMFLOAT2 FontAtlas::CalcTextSize(std::wstring_view text) const
{
	dx::XMFLOAT2 size{ 0.0f, 0.0f };
	dx::XMFLOAT2 cursor{ 0.0f, 0.0f };
	
	for (wchar_t codepoint : text)
	{
		switch (codepoint)
		{
		case L'\0':
			return size;

		case L'\n':
			cursor = { 0.0f, cursor.y + _lineHeight };
			break;

		case L'\r':
			break;

		case L'\t':
			// Snap to next 4-space tab stop
			cursor.x = std::ceil((cursor.x + 1.0f) / (_spacing * 4.0f)) * (_spacing * 4.0f);
			break;

		case L' ':
			cursor.x += _spacing;
			break;

		default:
			auto glyph = GetGlyph((UINT)codepoint);
			size.x = max(size.x, cursor.x + glyph->size.x - glyph->offset.x);
			size.y = max(size.y, cursor.y + glyph->size.y - glyph->offset.y);
			cursor.x += glyph->advance;
			break;
		}
	}

	return size;
}
dx::XMFLOAT2 FontAtlas::CalcTextSize(std::string_view text) const
{
	std::wstring wideText = StringUtils::NarrowToWide(text);
	return CalcTextSize(wideText);
}

std::vector<GlyphVertex> FontAtlas::Generate(std::wstring_view text) const
{
	std::vector<GlyphVertex> vertices;
	dx::XMFLOAT2 cursor{ 0.0f, 0.0f };

	for (wchar_t codepoint : text)
	{
		switch (codepoint)
		{
		case L'\0':
			return vertices;

		case L'\n':
			cursor = { 0.0f, cursor.y + _lineHeight };
			break;

		case L'\r':
			break;

		case L'\t':
			// Snap to next 4-space tab stop
			cursor.x = std::ceil((cursor.x + 1.0f) / (_spacing * 4.0f)) * (_spacing * 4.0f);
			break;

		case L' ':
			cursor.x += _spacing;
			break;

		default:
			AppendGlyph((UINT)codepoint, vertices, cursor);
			break;
		}
	}

	return vertices;
}
std::vector<GlyphVertex> FontAtlas::Generate(std::string_view text) const
{
	std::wstring wideText = StringUtils::NarrowToWide(text);
	return Generate(wideText);
}

MeshData *FontAtlas::ToMesh(const std::vector<GlyphVertex> &verts) const
{
	MeshData *meshData = new MeshData();

	size_t vertCount = verts.size();
	size_t vertInSize = sizeof(GlyphVertex);
	size_t vertOutSize = sizeof(ContentData::FormattedVertex);
	size_t vertInSizeF = vertInSize / sizeof(float);
	size_t vertOutSizeF = vertOutSize / sizeof(float);

	meshData->vertexInfo.nrOfVerticesInBuffer = (UINT)vertCount;
	meshData->vertexInfo.sizeOfVertex = vertOutSize;
	meshData->vertexInfo.vertexData = new float[vertCount * vertOutSize];

	for (size_t i = 0; i < vertCount; i++)
	{
		const GlyphVertex &vertIn = verts[i];
		ContentData::FormattedVertex *vertOut = (ContentData::FormattedVertex *)&meshData->vertexInfo.vertexData[i * vertOutSizeF];

		vertOut->px = vertIn.position.x;
		vertOut->py = -vertIn.position.y; // Flip Y position
		vertOut->pz = 0.0f;

		vertOut->u = vertIn.uv.x;
		vertOut->v = vertIn.uv.y;

		vertOut->nx = 0.0f;
		vertOut->ny = 0.0f;
		vertOut->nz = 1.0f;

		vertOut->tx = 0.0f;
		vertOut->ty = 1.0f;
		vertOut->tz = 0.0f;
	}

	meshData->indexInfo.nrOfIndicesInBuffer = (UINT)vertCount;
	meshData->indexInfo.indexData = new UINT[vertCount];

	for (size_t i = 0; i + 5 < vertCount; i += 6)
	{
		meshData->indexInfo.indexData[i + 0] = i + 2;
		meshData->indexInfo.indexData[i + 1] = i + 1;
		meshData->indexInfo.indexData[i + 2] = i + 0;

		meshData->indexInfo.indexData[i + 3] = i + 5;
		meshData->indexInfo.indexData[i + 4] = i + 4;
		meshData->indexInfo.indexData[i + 5] = i + 3;
	}

	// Single sub-mesh
	meshData->subMeshInfo.push_back(MeshData::SubMeshInfo());
	meshData->subMeshInfo[0].startIndexValue = 0;
	meshData->subMeshInfo[0].nrOfIndicesInSubMesh = vertCount;

	dx::XMFLOAT3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
	dx::XMFLOAT3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i + 5 < vertCount; i += 6)
	{
		ContentData::FormattedVertex *v = (ContentData::FormattedVertex *)&meshData->vertexInfo.vertexData[i * vertOutSizeF];

		dx::XMFLOAT3 *minVert = (dx::XMFLOAT3 *)&(v[1].px);
		minBounds.x = min(minBounds.x, minVert->x);
		minBounds.y = min(minBounds.y, minVert->y);

		dx::XMFLOAT3 *maxVert = (dx::XMFLOAT3 *)&(v[2].px);
		maxBounds.x = max(maxBounds.x, maxVert->x);
		maxBounds.y = max(maxBounds.y, maxVert->y);
	}

	meshData->boundingBox.Center = { (minBounds.x + maxBounds.x) / 2.0f, (minBounds.y + maxBounds.y) / 2.0f, 0.0f };
	meshData->boundingBox.Extents = { (maxBounds.x - minBounds.x) / 2.0f, (maxBounds.y - minBounds.y) / 2.0f, 1.0f };

	return meshData;
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
		atlasObj.AddMember("Line Height", _lineHeight, docAlloc);
		atlasObj.AddMember("Spacing", _spacing, docAlloc);

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
			return false; // Atlas doesn't exist

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

		memberName = "Line Height";
		if (atlas.HasMember(memberName.c_str()))
			_lineHeight = atlas[memberName.c_str()].GetFloat();

		memberName = "Spacing";
		if (atlas.HasMember(memberName.c_str()))
			_spacing = atlas[memberName.c_str()].GetFloat();

		memberName = "Glyphs";
		if (atlas.HasMember(memberName.c_str()))
		{
			const json::Value &glyphsArr = atlas[memberName.c_str()];
			if (glyphsArr.IsArray())
			{
				for (json::SizeType i = 0; i < glyphsArr.Size(); i++)
				{
					const json::Value &glyphObj = glyphsArr[i];
					if (!glyphObj.IsObject())
						continue;

					UINT codepoint = CONTENT_NULL;
					if (glyphObj.HasMember("Codepoint"))
						codepoint = glyphObj["Codepoint"].GetUint();

					GlyphData glyph{};
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

	return true;
}

bool FontAtlas::AddListener(size_t id, std::function<void(void)> func)
{
	if (_modifyCallback.find(id) != _modifyCallback.end())
		return false;

	_modifyCallback[id] = func;
	return true;
}
bool FontAtlas::RemoveListener(size_t id)
{
	if (_modifyCallback.find(id) == _modifyCallback.end())
		return false;

	_modifyCallback.erase(id);
	return true;
}

#ifdef USE_IMGUI
bool FontAtlas::RenderUI(const Content *content)
{
	bool modified = false;

	auto tex = content->GetTexture(_fontTextureID);
	std::string texName = content->GetTextureName(_fontTextureID);

	// Texture selection
	{
		int inputTexID = (int)_fontTextureID;
		bool isChanged = false;

		std::vector<std::string> textureNames;
		content->GetTextureNames(&textureNames);

		ImGuiComboFlags comboFlags = ImGuiComboFlags_None;
		comboFlags |= ImGuiComboFlags_HeightLarge;

		ImGui::BeginGroup();
		ImGui::Text("Texture:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		if (ImGui::BeginCombo("##FontTextureCombo", texName.c_str(), comboFlags))
		{
			static std::string filter = "";

			ImVec2 currSize = ImGui::GetContentRegionMax();
			const float popupMinWidth = 100.0f;
			float padding = ImGui::GetStyle().WindowPadding.x;
			float popupWidth = max(currSize.x - padding, popupMinWidth);
			float inputBoxPosX = ImGui::GetCursorPosX();

			if (ImGui::IsWindowAppearing())
				ImGui::SetKeyboardFocusHere(0);

			ImGui::SetNextItemWidth(popupWidth - padding);
			ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
			if (!ImGui::IsItemActive() && filter.empty())
			{
				ImGui::SameLine(inputBoxPosX + padding);
				ImGui::TextDisabled("Search");
			}

			if (!filter.empty())
				std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

			ImGui::Separator();

			ImGui::SetNextWindowSizeConstraints({ 50.0f, 50.0f }, { 500.0f, 300.0f });
			ImGui::SetWindowSize({ popupWidth, currSize.y }, ImGuiCond_Always);

			ImGui::BeginChild("ContentList", ImVec2(popupWidth - padding, 300.0f), ImGuiChildFlags_ResizeY);
			{
				bool isSelected = (inputTexID == -1);
				if (ImGui::Selectable("None", isSelected))
				{
					inputTexID = -1;
					isChanged = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
					if (ImGui::IsWindowAppearing())
						ImGui::SetScrollHereY();
				}
			}

			for (UINT i = 0; i < textureNames.size(); i++)
			{
				if (!filter.empty())
				{
					std::string lower = textureNames[i];
					std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

					if (lower.find(filter) == std::string::npos)
						continue;
				}

				bool isSelected = (inputTexID == i);
				if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
				{
					inputTexID = i;
					isChanged = true;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
					if (ImGui::IsWindowAppearing())
						ImGui::SetScrollHereY();
				}
			}
			ImGui::EndChild();
			ImGui::EndCombo();
		}
		ImGui::EndGroup();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
				ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

				inputTexID = contentPayload.id;
				isChanged = true;
			}
			ImGui::EndDragDropTarget();
		}

		if (isChanged)
		{
			modified = true;

			inputTexID += textureNames.size();
			inputTexID %= textureNames.size();
			_fontTextureID = (UINT)inputTexID;

			tex = content->GetTexture(_fontTextureID);
			texName = content->GetTextureName(_fontTextureID);
		}
	}

	if (!tex)
	{
		if (modified)
		{
			for (const auto &[id, func] : _modifyCallback)
				func();
		}

		return true;
	}

	ImGui::Separator();

	dx::XMUINT2 texSize = tex->GetSize();

	if (ImGui::Button("Save"))
	{
		if (!Serialize(_fontName, content))
		{
			ErrMsg("Could not save atlas!");
			return false;
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Add Glyph"))
		ImGui::OpenPopup("AddGlyphPopup");

	if (ImGui::BeginPopup("AddGlyphPopup"))
	{
		static int code = (int)' ';
		static GlyphData glyph{};

		std::string charStr = "";
		if (code >= 32)
		{
			// Convert to UTF-8
			std::wstring wCharStr = std::wstring(1, (wchar_t)code);
			charStr = StringUtils::WideToNarrow(wCharStr);
		}
		else
		{
			charStr = "?";
		}

		ImGui::Text("Codepoint:"); ImGui::SameLine();
		ImGui::InputInt("##InputCodepointInt", &code);

		ImGui::Text("Character:"); ImGui::SameLine();
		if (ImGui::InputText("##InputCodepointChar", &charStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite))
		{
			// Extract the last character from the string, accounting for multibyte characters
			UINT c;

			if (charStr.length() > 1)
			{
				// Keep only the last character
				std::wstring wCharStr = StringUtils::NarrowToWide(charStr);
				if (wCharStr.length() > 1)
				{
					wCharStr = wCharStr.substr(wCharStr.length() - 1, 1);
					charStr = StringUtils::WideToNarrow(wCharStr);
				}

				c = (UINT)wCharStr[0];
			}
			else
			{
				c = (UINT)charStr[0];
			}

			if (c != (UINT)'\0')
			{
				if (!charStr.empty())
				{
					code = (int)c;
				}
				else
				{
					code = (int)' ';
				}
			}
		}

		dx::XMFLOAT4 pixRect = {
			(glyph.uvRect.x * (float)texSize.x),
			(glyph.uvRect.y * (float)texSize.y),
			(glyph.uvRect.z * (float)texSize.x),
			(glyph.uvRect.w * (float)texSize.y)
		};

		if (ImGui::DragFloat4("Rect", &pixRect.x))
		{
			pixRect.x = std::clamp(pixRect.x, 0.0f, (float)texSize.x);
			pixRect.y = std::clamp(pixRect.y, 0.0f, (float)texSize.y);
			pixRect.z = std::clamp(pixRect.z, pixRect.x, (float)texSize.x);
			pixRect.w = std::clamp(pixRect.w, pixRect.y, (float)texSize.y);

			glyph.uvRect = {
				pixRect.x / (float)texSize.x,
				pixRect.y / (float)texSize.y,
				pixRect.z / (float)texSize.x,
				pixRect.w / (float)texSize.y
			};
		}
		ImGuiUtils::LockMouseOnActive();

		dx::XMFLOAT2 pixOffset = {
			(glyph.offset.x * (float)texSize.x),
			(glyph.offset.y * (float)texSize.y)
		};

		if (ImGui::DragFloat2("Offset", &pixOffset.x, 0.2f))
		{
			glyph.offset = {
				pixOffset.x / (float)texSize.x,
				pixOffset.y / (float)texSize.y
			};
		}
		ImGuiUtils::LockMouseOnActive();

		float pixAdvance = glyph.advance * (float)texSize.x;

		if (ImGui::DragFloat("Advance", &pixAdvance, 0.1f))
		{
			glyph.advance = pixAdvance / (float)texSize.x;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Button("Create"))
		{
			glyph.size = {
				pixRect.z - pixRect.x,
				pixRect.w - pixRect.y
			};

			_glyphs[(UINT)code] = glyph;
			_selectedGlyphIDs.push_back((UINT)code);

			modified = true;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Create Tileset"))
		ImGui::OpenPopup("CreateTilesPopup");

	if (ImGui::BeginPopup("CreateTilesPopup"))
	{
		static ImVec2i tileSize = { 32, 32 };
		static ImVec2i tilePadding = { 0, 0 };
		static ImVec2i tileOffset = { 8, 4 };
		static float spacing = 16.0f;

		ImGui::DragInt2("Tile Size", &tileSize.x, 1.0f, 1, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGuiUtils::LockMouseOnActive();

		ImGui::DragInt2("Tile Padding", &tilePadding.x, 0.1f, 0, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGuiUtils::LockMouseOnActive();

		ImGui::DragInt2("Tile Offset", &tileOffset.x, 0.1f, 0, INT_MAX, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGuiUtils::LockMouseOnActive();

		ImGui::DragFloat("Spacing", &spacing, 0.1f, 0.0f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGuiUtils::LockMouseOnActive();

		static ImVec2i offset = { 0, 0 };
		static std::string inputStr = "";
		if (ImGui::Button("Create"))
		{
			_glyphs.clear();
			modified = true;

			ImGui::OpenPopup("DefineTilesPopup");
			offset = { 0, 0 };
		}

		bool closePopup = false;
		if (ImGui::BeginPopup("DefineTilesPopup"))
		{
			ImGui::Image(
				(ImTextureID)tex->GetSRV(),
				{ 96.0f, 96.0f * ((float)tileSize.y / (float)tileSize.x) },
				{ (float)offset.x / (float)texSize.x, (float)offset.y / (float)texSize.y },
				{ (float)(offset.x + tileSize.x) / (float)texSize.x, (float)(offset.y + tileSize.y) / (float)texSize.y }
			);

			static bool refocusInput = true;
			if (refocusInput)
			{
				ImGui::SetKeyboardFocusHere(0);
				refocusInput = false;
			}

			ImGui::Text("Code:"); ImGui::SameLine();
			if (ImGui::InputText("##InputCodepointChar", &inputStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite))
			{
				if (inputStr.length() > 1)
				{
					// Keep only the last character
					std::wstring wInputStr = StringUtils::NarrowToWide(inputStr);
					if (wInputStr.length() > 1)
					{
						wInputStr = wInputStr.substr(wInputStr.length() - 1, 1);
						inputStr = StringUtils::WideToNarrow(wInputStr);
					}
				}
			}

			UINT c = '\0';

			if (inputStr.length() > 1)
			{
				std::wstring wInputStr = StringUtils::NarrowToWide(inputStr);
				c = (UINT)wInputStr[0];
			}
			else if (inputStr.length() == 1)
			{
				c = (UINT)inputStr[0];
			}

			bool advance = false;
			ImGui::BeginDisabled(c == (UINT)'\0');
			if ((ImGui::Button("Next") || Input::Instance().GetKey(KeyCode::Enter, true) == KeyState::Pressed) && c != (UINT)'\0')
			{
				GlyphData glyph{};

				glyph.uvRect = {
					(float)offset.x / (float)texSize.x,
					(float)offset.y / (float)texSize.y,
					(float)(offset.x + tileSize.x) / (float)texSize.x,
					(float)(offset.y + tileSize.y) / (float)texSize.y
				};
				glyph.size = { (float)tileSize.x, (float)tileSize.y };
				glyph.offset = { (float)tileOffset.x, (float)tileOffset.y };
				glyph.advance = spacing;

				_glyphs[c] = glyph;
				modified = true;

				advance = true;
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Skip") || Input::Instance().GetKey(KeyCode::Delete, true) == KeyState::Pressed)
			{
				advance = true;
			}

			bool reachedEnd = false;
			if (advance)
			{
				refocusInput = true;
				inputStr.clear();
				offset.x += tileSize.x + tilePadding.x;

				if ((offset.x + tileSize.x) > (int)texSize.x)
				{
					offset.x = 0;
					offset.y += tileSize.y + tilePadding.y;

					if ((offset.y + tileSize.y) > (int)texSize.y)
					{
						offset.y = 0;
						reachedEnd = true;
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel") || reachedEnd)
			{
				ImGui::CloseCurrentPopup();
				closePopup = true;
			}

			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel") || closePopup)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button("Clear Glyphs"))
	{
		_glyphs.clear();
		modified = true;
	}

	static ImGuiSelectionBasicStorage selection;

	// Multi-select
	{
		ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_NoSelectAll | ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect2d;

		if (ImGui::TreeNode(std::format("Texture '{}'", texName).c_str()))
		{
			ImGui::SetItemTooltip("Zoom with ctrl + scroll \nPan with right mouse button");

			static float zoom = 1.0f;
			ImVec2 texSizeImVec = ImVec2(texSize.x, texSize.y) * zoom;

			ImVec2 windowSize = texSizeImVec;
			windowSize.x = min(windowSize.x, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize);

			ImGuiChildFlags childFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY;
			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None | ImGuiWindowFlags_HorizontalScrollbar;

			ImGui::BeginChild("AtlasTexture", windowSize, childFlags, windowFlags);

			ImVec2 screenPos = ImGui::GetCursorScreenPos();
			ImVec2 texMin = ImGui::GetCursorPos();
			ImVec2 texMax = { texMin.x + texSizeImVec.x, texMin.y + texSizeImVec.y };
			ImVec2 windowOffset = screenPos - texMin;

			ImGui::Image(
				(ImTextureID)tex->GetSRV(),
				texSizeImVec
			);

			static bool isPanning = false;

			// Zoom + Pan control
			if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered())
			{
				// Zoom with ctrl + scroll
				float scrollY = Input::Instance().GetMouse().scroll.y;

				if (scrollY != 0.0f && ImGui::GetKeyData(ImGuiKey_LeftCtrl)->Down)
				{
					zoom *= 1.0f + (scrollY * 0.075f);
					zoom = std::clamp(zoom, 0.1f, 25.0f);

					// Adjust scroll to keep mouse position stable
					ImVec2 mousePos = ImGui::GetIO().MousePos;
					ImVec2 mouseOffset = mousePos - screenPos;
					ImVec2 offsetRatio = { mouseOffset.x / (texSizeImVec.x), mouseOffset.y / (texSizeImVec.y) };
					ImVec2 newTexSizeImVec = ImVec2(texSize.x, texSize.y) * zoom;
					ImVec2 newMouseOffset = { newTexSizeImVec.x * offsetRatio.x, newTexSizeImVec.y * offsetRatio.y };
					ImVec2 deltaOffset = newMouseOffset - mouseOffset;

					ImGui::SetScrollX(ImGui::GetScrollX() + deltaOffset.x);
					ImGui::SetScrollY(ImGui::GetScrollY() + deltaOffset.y);
				}
			
				static ImVec2 lastMousePos = {};

				// Pan with right mouse button
				if (ImGui::GetIO().MouseDown[1])
				{
					ImVec2 mousePos = ImGui::GetIO().MousePos;

					if (!isPanning)
					{
						isPanning = true;
					}
					else
					{
						ImVec2 delta = mousePos - lastMousePos;
						ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
						ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
					}

					lastMousePos = mousePos;
				}
				else
				{
					isPanning = false;
				}
			}
			else 
			{
				isPanning = false;
			}

			ImGuiMultiSelectIO *ms_io = ImGui::BeginMultiSelect(flags, selection.Size, 0);
			selection.ApplyRequests(ms_io);

			// Draw selectable glyph outlines
			for (const auto &[codepoint, glyph] : _glyphs)
			{
				bool isSelected = selection.Contains((ImGuiID)codepoint);
				ImGui::SetNextItemSelectionUserData(codepoint);

				ImVec2 glyphMin = {
					Lerp(texMin.x, texMax.x, glyph.uvRect.x),
					Lerp(texMin.y, texMax.y, glyph.uvRect.y)
				};
				ImVec2 glyphMax = {
					Lerp(texMin.x, texMax.x, glyph.uvRect.z),
					Lerp(texMin.y, texMax.y, glyph.uvRect.w)
				};
				ImVec2 glyphSize = glyphMax - glyphMin;

				ImGui::SetCursorPos(glyphMin);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
				if (ImGui::Selectable(std::format("##Glyph{}", codepoint).c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap, glyphSize))
				{
					_selectedGlyphIDs.push_back(codepoint);
				}
				ImGui::PopStyleVar();

				// Draw outline
				ImGui::GetWindowDrawList()->AddRect(
					windowOffset + glyphMin, windowOffset + glyphMax,
					isSelected ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 128)
				);

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}

			ms_io = ImGui::EndMultiSelect();
			selection.ApplyRequests(ms_io);

			ImGui::EndChild();
			ImGui::TreePop();
		}
		else
		{
			ImGui::SetItemTooltip("Zoom with ctrl + scroll \nPan with right mouse button");
		}

		if (ImGui::TreeNode("Glyph Table"))
		{
			ImGuiChildFlags childFlags = ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY;
			ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH;
			ImGuiSelectableFlags selectableFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;

			ImGui::BeginChild("GlyphTableChild", ImVec2(0, 200), childFlags);

			if (ImGui::BeginTable("GlyphTable", 3, tableFlags))
			{
				ImGui::TableSetupColumn("Preview");
				ImGui::TableSetupColumn("Char");
				ImGui::TableSetupColumn("Code");
				ImGui::TableHeadersRow();

				ImGuiMultiSelectIO *ms_io = ImGui::BeginMultiSelect(flags, selection.Size, 0);
				selection.ApplyRequests(ms_io);

				bool foundSelection = false;
				for (const auto &[codepoint, glyph] : _glyphs)
				{
					bool isSelected = selection.Contains((ImGuiID)codepoint);
					ImGui::TableNextRow();

					float rowHeight = max(ImGui::GetTextLineHeightWithSpacing(), glyph.size.y);

					// Preview
					ImGui::TableSetColumnIndex(0);
					ImVec2 rowBegin = ImGui::GetCursorPos();

					ImGui::Image(
						(ImTextureID)tex->GetSRV(),
						{ glyph.size.x, glyph.size.y },
						{ glyph.uvRect.x, glyph.uvRect.y },
						{ glyph.uvRect.z, glyph.uvRect.w }
					);

					// Char
					ImGui::TableSetColumnIndex(1);
					if (codepoint >= 32u)
					{
						// Display as UTF-8
						std::wstring wCharStr = std::wstring(1, (wchar_t)codepoint);
						std::string charStr = StringUtils::WideToNarrow(wCharStr);
						ImGui::Text("%s", charStr.c_str());
					}
					else
					{
						ImGui::Text("?");
					}

					// Codepoint
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%u", codepoint);

					ImGui::SetCursorPos(rowBegin);
					ImGui::SetNextItemSelectionUserData(codepoint);
					if (ImGui::Selectable(std::format("##GlyphTableSelect{}", codepoint).c_str(), isSelected, selectableFlags, {0, rowHeight}))
					{
						_selectedGlyphIDs.push_back(codepoint);
					}

					if (isSelected && !foundSelection)
					{
						ImGui::SetItemDefaultFocus();
						if (ImGui::IsWindowAppearing())
							ImGui::SetScrollHereY();

						foundSelection = true;
					}
				}

				ms_io = ImGui::EndMultiSelect();
				selection.ApplyRequests(ms_io);

				ImGui::EndTable();
			}

			ImGui::EndChild();
			ImGui::TreePop();
		}

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
		{
			selection.Clear();
			for (const auto &[codepoint, glyph] : _glyphs)
				selection.SetItemSelected((ImGuiID)codepoint, true);
		}

		// Process selection
		{
			_selectedGlyphIDs.clear();

			void *it = NULL;
			ImGuiID id;

			while (selection.GetNextSelectedItem(&it, &id))
			{
				if (_glyphs.find((UINT)id) == _glyphs.end())
					continue;

				_selectedGlyphIDs.push_back((UINT)id);
			}
		}
	}

	if (ImGui::TreeNode("Glyph Inspector"))
	{
		if (_selectedGlyphIDs.empty())
		{
			ImGui::Text("No glyph selected.");
		}
		else
		{
			if (ImGui::Button("Delete"))
			{
				for (UINT codepoint : _selectedGlyphIDs)
					_glyphs.erase(codepoint);

				_selectedGlyphIDs.clear();
				selection.Clear();

				modified = true;
				goto SkipInspector;
			}

			ImGui::SameLine();

			if (ImGui::Button("Bake Offsets"))
			{
				for (UINT i = 0; i < _selectedGlyphIDs.size(); i++)
				{
					UINT codepoint = _selectedGlyphIDs[i];
					GlyphData &glyph = _glyphs[codepoint];

					dx::XMFLOAT4 pixRect = {
						(glyph.uvRect.x * (float)texSize.x),
						(glyph.uvRect.y * (float)texSize.y),
						(glyph.uvRect.z * (float)texSize.x),
						(glyph.uvRect.w * (float)texSize.y)
					};

					pixRect.x += glyph.offset.x;
					pixRect.y += glyph.offset.y;
					pixRect.z = pixRect.x + glyph.advance;
					pixRect.w -= glyph.offset.y;

					glyph.uvRect = {
						pixRect.x / (float)texSize.x,
						pixRect.y / (float)texSize.y,
						pixRect.z / (float)texSize.x,
						pixRect.w / (float)texSize.y
					};
					glyph.size = { pixRect.z - pixRect.x, pixRect.w - pixRect.y };
					glyph.offset = { 0.0f, 0.0f };
				}

				modified = true;
			}

			ImGui::Separator();

			bool multiSelection = (_selectedGlyphIDs.size() > 1);

			UINT firstID = _selectedGlyphIDs[0];
			GlyphData &firstGlyph = _glyphs[firstID];

			// Preview
			ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 1.0f);
			ImGui::Image(
				(ImTextureID)tex->GetSRV(), 
				{ firstGlyph.size.x, firstGlyph.size.y },
				{ firstGlyph.uvRect.x, firstGlyph.uvRect.y },
				{ firstGlyph.uvRect.z, firstGlyph.uvRect.w }
			);
			ImGui::PopStyleVar();

			// Highlight offset
			ImGui::GetWindowDrawList()->AddLine(
				{ ImGui::GetItemRectMin().x,					 ImGui::GetItemRectMin().y + firstGlyph.offset.y },
				{ ImGui::GetItemRectMin().x + firstGlyph.size.x, ImGui::GetItemRectMin().y + firstGlyph.offset.y },
				IM_COL32(255, 0, 0, 127)
			);
			ImGui::GetWindowDrawList()->AddLine(
				{ ImGui::GetItemRectMin().x + firstGlyph.offset.x,	ImGui::GetItemRectMin().y					  },
				{ ImGui::GetItemRectMin().x + firstGlyph.offset.x,	ImGui::GetItemRectMin().y + firstGlyph.size.y },
				IM_COL32(255, 0, 0, 127)
			);

			// Highlight advance
			ImGui::GetWindowDrawList()->AddLine(
				{ ImGui::GetItemRectMin().x + firstGlyph.offset.x + firstGlyph.advance, ImGui::GetItemRectMin().y					  },
				{ ImGui::GetItemRectMin().x + firstGlyph.offset.x + firstGlyph.advance, ImGui::GetItemRectMin().y + firstGlyph.size.y },
				IM_COL32(0, 255, 0, 127)
			);

			bool isChanged = false;
			UINT inputCodepoint = firstID;

			if (!multiSelection)
			{
				std::string charStr = "";
				if (inputCodepoint >= 32u)
				{
					// Convert to UTF-8
					std::wstring wCharStr = std::wstring(1, (wchar_t)inputCodepoint);
					charStr = StringUtils::WideToNarrow(wCharStr);
				}
				else
				{
					charStr = "?";
				}

				ImGui::Text("Codepoint:");
				ImGui::SameLine();
				if (ImGui::InputScalar("##InputCodepointInt", ImGuiDataType_U32, &inputCodepoint))
					isChanged = true;

				ImGui::Text("Character:"); ImGui::SameLine();
				if (ImGui::InputText("##InputCodepointChar", &charStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite))
				{
					// Extract the last character from the string, accounting for multibyte characters
					UINT c;

					if (charStr.length() > 1)
					{
						// Keep only the last character
						std::wstring wCharStr = StringUtils::NarrowToWide(charStr);
						if (wCharStr.length() > 1)
						{
							wCharStr = wCharStr.substr(wCharStr.length() - 1, 1);
							charStr = StringUtils::WideToNarrow(wCharStr);
						}

						c = (UINT)wCharStr[0];
					}
					else
					{
						c = (UINT)charStr[0];
					}

					// Discard if input character is unchanged
					if (c == inputCodepoint)
					{
						c = '\0';
					}

					if (c != (UINT)'\0')
					{
						if (!charStr.empty())
						{
							inputCodepoint = c;
						}
						else
						{
							inputCodepoint = -1;
						}
						isChanged = true;
					}
				}
			}

			dx::XMFLOAT4 pixRect = {
				firstGlyph.uvRect.x * (float)texSize.x,
				firstGlyph.uvRect.y * (float)texSize.y,
				firstGlyph.uvRect.z * (float)texSize.x,
				firstGlyph.uvRect.w * (float)texSize.y
			};
			dx::XMFLOAT4 pixDeltaRect = pixRect;

			if (ImGui::DragFloat4("Rect", &pixRect.x))
			{
				pixRect.x = std::clamp(pixRect.x, 0.0f, (float)texSize.x);
				pixRect.y = std::clamp(pixRect.y, 0.0f, (float)texSize.y);
				pixRect.z = std::clamp(pixRect.z, pixRect.x, (float)texSize.x);
				pixRect.w = std::clamp(pixRect.w, pixRect.y, (float)texSize.y);
				
				pixDeltaRect = {
					pixRect.x - pixDeltaRect.x,
					pixRect.y - pixDeltaRect.y,
					pixRect.z - pixDeltaRect.z,
					pixRect.w - pixDeltaRect.w
				};

				dx::XMFLOAT4 deltaRect = { 
					pixDeltaRect.x / (float)texSize.x,
					pixDeltaRect.y / (float)texSize.y,
					pixDeltaRect.z / (float)texSize.x,
					pixDeltaRect.w / (float)texSize.y
				};

				firstGlyph.uvRect.x += deltaRect.x;
				firstGlyph.uvRect.y += deltaRect.y;
				firstGlyph.uvRect.z += deltaRect.z;
				firstGlyph.uvRect.w += deltaRect.w;

				dx::XMFLOAT2 deltaSize = firstGlyph.size;
				firstGlyph.size = { pixRect.z - pixRect.x, pixRect.w - pixRect.y };
				deltaSize = { firstGlyph.size.x - deltaSize.x, firstGlyph.size.y - deltaSize.y };

				if (multiSelection)
				{
					for (UINT i = 1; i < _selectedGlyphIDs.size(); i++)
					{
						GlyphData &glyph = _glyphs[_selectedGlyphIDs[i]];
						glyph.uvRect.x += deltaRect.x;
						glyph.uvRect.y += deltaRect.y;
						glyph.uvRect.z += deltaRect.z;
						glyph.uvRect.w += deltaRect.w;

						glyph.size.x += deltaSize.x;
						glyph.size.y += deltaSize.y;
					}
				}

				modified = true;
			}
			ImGuiUtils::LockMouseOnActive();

			if (ImGui::DragFloat2("Offset", &firstGlyph.offset.x, 0.1f))
			{
				modified = true;
				if (multiSelection)
				{
					for (UINT i = 1; i < _selectedGlyphIDs.size(); i++)
					{
						_glyphs[_selectedGlyphIDs[i]].offset = firstGlyph.offset;
					}
				}
			}
			ImGuiUtils::LockMouseOnActive();

			if (ImGui::DragFloat("Advance", &firstGlyph.advance, 0.1f))
			{
				modified = true;
				if (multiSelection)
				{
					for (UINT i = 1; i < _selectedGlyphIDs.size(); i++)
					{
						_glyphs[_selectedGlyphIDs[i]].advance = firstGlyph.advance;
					}
				}
			}
			ImGuiUtils::LockMouseOnActive();

			if (!multiSelection)
			{
				if (isChanged && inputCodepoint != CONTENT_NULL)
				{
					// Move the glyph to the new codepoint & update selection
					_glyphs[inputCodepoint] = firstGlyph;
					_glyphs.erase(_glyphs.find(firstID));

					selection.Clear();
					selection.SetItemSelected((ImGuiID)inputCodepoint, true);

					modified = true;
				}
			}
		}

	SkipInspector:

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("General"))
	{
		ImGui::Text("Total Glyphs: %zu", _glyphs.size());

		int fallbackGlyphID = (int)_fallbackGlyphID;
		ImGui::Text("Fallback Glyph:"); ImGui::SameLine();
		if (ImGui::InputInt("##FallbackGlyph", &fallbackGlyphID))
		{
			modified = true;
			_fallbackGlyphID = (UINT)fallbackGlyphID;

			if (_glyphs.find(_fallbackGlyphID) == _glyphs.end())
				_fallbackGlyphID = CONTENT_NULL;
		}

		ImGui::Text("Line Height:"); ImGui::SameLine();
		if (ImGui::DragFloat("##LineHeight", &_lineHeight, 0.1f, 0.0f))
			modified = true;
		ImGuiUtils::LockMouseOnActive();

		ImGui::Text("Spacing:"); ImGui::SameLine();
		if (ImGui::DragFloat("##Spacing", &_spacing, 0.1f, 0.0f))
			modified = true;
		ImGuiUtils::LockMouseOnActive();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Preview"))
	{
		static float sizeScale = 1.0f;
		ImGui::DragFloat("Font Scale", &sizeScale, 0.01f, 0.01f);
		ImGuiUtils::LockMouseOnActive();

		static std::string inputText = 
			"The quick brown fox jumps over the lazy dog.\n"
			"0123456789\n"
			"!?@#$ % ^&*()_ + -= [] {} | ; ':\",./<>\n"
			"\n"
			"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n"
			"A aBbCc  JjKk L l M mNn qR ru VvWw   XxYy Zz";

		ImGui::SeparatorText("Example Text");
		ImGui::InputTextMultiline("##InputText", &inputText);

		auto textMesh = Generate(inputText);
		ImGui::SeparatorText("Atlas Text");
		ImVec2 startPos = ImGui::GetCursorScreenPos();

		static ImVec2 textSize = { 64, 16 };
		ImGui::Dummy(textSize + ImVec2(4, 4));
		textSize = { 64, 16 };

		for (size_t i = 0; i + 5 < textMesh.size(); i += 6)
		{
			GlyphVertex &vMin = textMesh[i + 1];
			GlyphVertex &vMax = textMesh[i + 2];

			ImVec2 posMin = { 
				startPos.x + vMin.position.x * sizeScale, 
				startPos.y + vMin.position.y * sizeScale 
			};
			ImVec2 posMax = { 
				startPos.x + vMax.position.x * sizeScale, 
				startPos.y + vMax.position.y * sizeScale 
			};

			ImVec2 uvMin = { vMin.uv.x, vMin.uv.y };
			ImVec2 uvMax = { vMax.uv.x, vMax.uv.y };

			ImGui::GetWindowDrawList()->AddImage(
				(ImTextureID)tex->GetSRV(), 
				posMin, posMax, uvMin, uvMax
			);

			textSize.x = max(textSize.x, (posMin.x - startPos.x));
			textSize.y = max(textSize.y, (posMin.y - startPos.y));
			textSize.x = max(textSize.x, (posMax.x - startPos.x));
			textSize.y = max(textSize.y, (posMax.y - startPos.y));
		}

		ImGui::SeparatorText("Info");
		ImGui::Text("Characters: %zu", inputText.length());
		ImGui::Text("Generated Vertices: %zu", textMesh.size());

		ImGui::TreePop();
	}

	if (modified)
	{
		for (const auto &[id, func] : _modifyCallback)
			func();
	}

	return true;
}
#endif
