#include "stdafx.h"
#include "FontAtlas.h"

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

void FontAtlas::AppendGlyph(UINT codepoint, std::vector<GlyphVertex> &vertices, dx::XMFLOAT2 &cursor, float lineHeight) const
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
	}

	return true;
}

#ifdef USE_IMGUI
bool FontAtlas::RenderUI(const Content *content)
{
	auto tex = content->GetTexture(_fontTextureID);
	std::string texName = content->GetTextureName(_fontTextureID);

	if (ImGui::Button("Save"))
	{
		if (!Serialize(_fontName, content))
		{
			ErrMsg("Could not save atlas!");
			return false;
		}
	}

	if (tex)
	{
		if (ImGui::Button("Add Glyph"))
			ImGui::OpenPopup("AddGlyphPopup");

		if (ImGui::BeginPopup("AddGlyphPopup"))
		{
			static int code = (int)' ';
			static GlyphData glyph{};

			std::string charStr = "";
			if (code >= 32 && code <= 126)
				charStr = std::string(1, static_cast<char>(code));
			else
				charStr = "?";

			ImGui::Text("Codepoint:");
			ImGui::SameLine();
			ImGui::InputInt("##InputCodepointInt", &code);

			ImGui::Text("Character:");
			ImGui::SameLine();
			if (ImGui::InputText("##InputCodepointChar", &charStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite))
			{
				char c;
				if (charStr.length() == 1)
				{
					c = charStr[0];
				}
				else if (charStr.length() > 1)
				{
					// Use the last character
					c = charStr.back();
				}
				else
				{
					c = '\0';
				}

				if (c != '\0')
				{
					if (!charStr.empty())
						code = static_cast<int>(static_cast<unsigned char>(charStr[0]));
					else
						code = (int)' ';
				}
			}

			dx::XMUINT2 texSize = tex->GetSize();

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

			float pixAdvance = glyph.advance * (float)texSize.x;

			if (ImGui::DragFloat("Advance", &pixAdvance, 0.1f))
			{
				glyph.advance = pixAdvance / (float)texSize.x;
			}

			if (ImGui::Button("Create"))
			{
				glyph.size = { 
					pixRect.z - pixRect.x, 
					pixRect.w - pixRect.y 
				};

				_glyphs[(UINT)code] = glyph;
				_uiSelectedGlyphID = (UINT)code;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (ImGui::TreeNode("Glyph Inspector"))
		{
			if (_uiSelectedGlyphID != CONTENT_NULL)
			{
				auto it = _glyphs.find(_uiSelectedGlyphID);
				if (it != _glyphs.end())
				{
					GlyphData &glyph = it->second;

					int inputCodepoint = (int)it->first;
					bool isChanged = false;

					// Preview
					ImGui::Image((ImTextureID)tex->GetSRV(), { glyph.size.x, glyph.size.y }, { glyph.uvRect.x, glyph.uvRect.y }, { glyph.uvRect.z, glyph.uvRect.w });

					std::string charStr = "";
					if (inputCodepoint >= 32 && inputCodepoint <= 126)
						charStr = std::string(1, static_cast<char>(inputCodepoint));
					else
						charStr = "?";

					ImGui::Text("Codepoint:");
					ImGui::SameLine();
					if (ImGui::InputInt("##InputCodepointInt", &inputCodepoint))
						isChanged = true;

					ImGui::Text("Character:");
					ImGui::SameLine();
					if (ImGui::InputText("##InputCodepointChar", &charStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite))
					{
						char c;
						if (charStr.length() == 1)
						{
							c = charStr[0];
						}
						else if (charStr.length() > 1)
						{
							// Use the last character
							c = charStr.back();
						}
						else
						{
							c = '\0';
						}

						// Discard if input character is unchanged
						if (c == static_cast<char>(inputCodepoint))
							c = '\0';

						if (c != '\0')
						{
							if (!charStr.empty())
								inputCodepoint = static_cast<int>(static_cast<unsigned char>(charStr[0]));
							else
								inputCodepoint = -1;
							isChanged = true;
						}
					}

					if (isChanged)
					{
						// Move the glyph to the new codepoint & update selection
						_glyphs[(UINT)inputCodepoint] = glyph;
						_uiSelectedGlyphID = (UINT)inputCodepoint;
						_glyphs.erase(it);
					}
				}
				else
				{
					ImGui::Text("Selected glyph ID %u not found!", _uiSelectedGlyphID);
				}
			}
			else
			{
				ImGui::Text("No glyph selected.");
			}

			ImGui::TreePop();
		}
	}

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
			inputTexID += textureNames.size();
			inputTexID %= textureNames.size();
			_fontTextureID = (UINT)inputTexID;

			tex = content->GetTexture(_fontTextureID);
			texName = content->GetTextureName(_fontTextureID);
		}
	}

	dx::XMUINT2 texSize = tex ? tex->GetSize() : dx::XMUINT2{0, 0};

	if (ImGui::TreeNode(std::format("Texture '{}'", texName).c_str()))
	{
		ImVec2 texSizeImVec = { (float)texSize.x, (float)texSize.y };
		ImGui::BeginChild("AtlasTexture", texSizeImVec, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_None);

		ImVec2 texMin = ImGui::GetCursorPos();
		ImVec2 texMax = { texMin.x + texSizeImVec.x, texMin.y + texSizeImVec.y };

		ImGui::Image((ImTextureID)tex->GetSRV(), texSizeImVec);

		// Draw selectable glyph outlines
		for (const auto &[codepoint, glyph] : _glyphs)
		{
			bool isSelected = (codepoint == _uiSelectedGlyphID);

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
			if (ImGui::Selectable(std::format("##Glyph{}", codepoint).c_str(), isSelected, ImGuiSelectableFlags_AllowItemOverlap, glyphSize))
			{
				_uiSelectedGlyphID = codepoint;
			}

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetScrollHereX();
					ImGui::SetScrollHereY();
				}
			}

			ImGui::GetWindowDrawList()->AddRect(glyphMin, glyphMax, IM_COL32(255, 255, 0, 255));
		}

		ImGui::EndChild();
		ImGui::TreePop();
	}

	// Glyph table
	if (ImGui::TreeNode("Glyphs"))
	{
		ImGui::Text("WIP");

		/*
		if (ImGui::BeginTable("GlyphTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed, 70.0f); // Fixed
			ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 70.0f); // Fixed
			ImGui::TableSetupColumn("UV Rect", ImGuiTableColumnFlags_WidthStretch, 200.0f); // Editable
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100.0f); // Fixed
			ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 100.0f); // Editable
			ImGui::TableSetupColumn("Advance", ImGuiTableColumnFlags_WidthFixed, 100.0f); // Editable

			ImGui::TableHeadersRow();
			for (const auto &[codepoint, glyph] : _glyphs)
			{
				bool isSelected = (codepoint == _uiSelectedGlyphID);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Selectable(std::format("U+{:04X}", codepoint).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap);

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
					if (ImGui::IsWindowAppearing())
						ImGui::SetScrollHereY();
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::Image((ImTextureID)tex->GetSRV(), { glyph.size.x, glyph.size.y }, { glyph.uvRect.x, glyph.uvRect.y }, { glyph.uvRect.z, glyph.uvRect.w });
			}

			ImGui::EndTable();
		}
		*/

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Font Info"))
	{
		ImGui::Text("Name: %s", _fontName.c_str());
		ImGui::Text("Texture ID: %u (%s)", _fontTextureID, texName.c_str());
		ImGui::Text("Fallback Glyph: U+%04X", _fallbackGlyphID);
		ImGui::Text("Total Glyphs: %zu", _glyphs.size());
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Preview"))
	{
		static float sizeScale = 1.0f;
		static float lineHeight = 24.0f;
		ImGui::DragFloat("Line Height", &lineHeight, 0.1f, 1.0f, 100.0f, "%.1f");
		ImGui::DragFloat("Font Scale", &sizeScale, 0.01f, 0.01f, 100.0f, "%.01f");

		const static char inputText[87] = "The quick brown fox jumps over the lazy dog.\n0123456789\n!@#$%^&*()_+-=[]{}|;':\",./<>?";

		ImGui::SeparatorText("Example Text");
		ImGui::Text(inputText);

		auto textMesh = Generate(inputText, lineHeight);
		ImGui::SeparatorText("Atlas Text");
		ImVec2 startPos = ImGui::GetCursorPos();

		for (size_t i = 0; i < textMesh.size(); i += 6)
		{
			if (i + 5 >= textMesh.size())
				break;

			const GlyphVertex &v0 = textMesh[i + 0];
			const GlyphVertex &v1 = textMesh[i + 1];
			const GlyphVertex &v2 = textMesh[i + 2];
			const GlyphVertex &v3 = textMesh[i + 4];

			ImVec2 pos0 = { startPos.x + v0.position.x * sizeScale, startPos.y + v0.position.y * sizeScale };
			ImVec2 pos1 = { startPos.x + v1.position.x * sizeScale, startPos.y + v1.position.y * sizeScale };
			ImVec2 pos2 = { startPos.x + v2.position.x * sizeScale, startPos.y + v2.position.y * sizeScale };
			ImVec2 pos3 = { startPos.x + v3.position.x * sizeScale, startPos.y + v3.position.y * sizeScale };

			ImVec2 uv0 = { v0.uv.x, v0.uv.y };
			ImVec2 uv1 = { v1.uv.x, v1.uv.y };
			ImVec2 uv2 = { v2.uv.x, v2.uv.y };
			ImVec2 uv3 = { v3.uv.x, v3.uv.y };

			ImGui::GetWindowDrawList()->AddImageQuad((ImTextureID)tex->GetSRV(), pos0, pos1, pos3, pos2, uv0, uv1, uv3, uv2);
		}

		ImGui::SeparatorText("Info");
		ImGui::Text("Characters: %zu", strlen(inputText));
		ImGui::Text("Generated Vertices: %zu", textMesh.size());

		ImGui::TreePop();
	}

	return true;
}
#endif
