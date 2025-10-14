#include "stdafx.h"
#include "FontAtlas.h"
#include <unordered_set>

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

std::vector<GlyphVertex> FontAtlas::Generate(std::wstring_view text) const
{
	std::vector<GlyphVertex> vertices;
	dx::XMFLOAT2 cursor{ 0.0f, 0.0f };

	for (wchar_t c : text)
	{
		UINT codepoint = static_cast<UINT>(c);

		switch (codepoint)
		{
		case '\0':
			return vertices;

		case '\n':
			cursor = { 0.0f, cursor.y + _lineHeight };
			break;

		case '\r':
			break;

		case '\t':
			// Snap to next 4-space tab stop
			cursor.x = std::ceil((cursor.x + 1.0f) / (_spacing * 4.0f)) * (_spacing * 4.0f);
			break;

		case ' ':
			cursor.x += _spacing;
			break;

		default:
			AppendGlyph(codepoint, vertices, cursor);
			break;
		}
	}

	return vertices;
}
std::vector<GlyphVertex> FontAtlas::Generate(std::string_view text) const
{
	std::wstring wText(text.begin(), text.end());
	return Generate(wText);
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

	if (!tex)
		return true;

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
			ImGui::InputText("##InputCodepointChar", &inputStr, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_AlwaysOverwrite);
			
			if (inputStr.length() > 1)
				inputStr = std::string(1, inputStr.back());

			char c = '\0';
			if (inputStr.length() == 1)
			{
				c = inputStr[0];
			}
			else
			{
				c = '\0';
			}

			bool advance = false;
			ImGui::BeginDisabled(c == '\0');
			if ((ImGui::Button("Next") || Input::Instance().GetKey(KeyCode::Enter, true) == KeyState::Pressed) && c != '\0')
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

				_glyphs[(UINT)c] = glyph;
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
		_glyphs.clear();

	if (ImGui::TreeNode(std::format("Texture '{}'", texName).c_str()))
	{
		ImVec2 texSizeImVec = { (float)texSize.x, (float)texSize.y };
		ImGui::BeginChild("AtlasTexture", texSizeImVec);

		ImVec2 screenPos = ImGui::GetCursorScreenPos();
		ImVec2 texMin = ImGui::GetCursorPos();
		ImVec2 texMax = { texMin.x + texSizeImVec.x, texMin.y + texSizeImVec.y };
		ImVec2 windowOffset = screenPos - texMin;

		ImGui::Image(
			(ImTextureID)tex->GetSRV(), 
			texSizeImVec
		);

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
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
			if (ImGui::Selectable(std::format("##Glyph{}", codepoint).c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap, glyphSize))
			{
				_uiSelectedGlyphID = codepoint;
			}
			ImGui::PopStyleVar();

			// Draw outline
			ImGui::GetWindowDrawList()->AddRect(
				windowOffset + glyphMin, windowOffset + glyphMax,
				isSelected ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 128)
			);

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetScrollHereX();
					ImGui::SetScrollHereY();
				}
			}
		}

		ImGui::EndChild();
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Glyph Inspector"))
	{
		if (_uiSelectedGlyphID != CONTENT_NULL)
		{
			auto it = _glyphs.find(_uiSelectedGlyphID);
			if (it != _glyphs.end())
			{
				GlyphData &glyph = it->second;

				UINT inputCodepoint = it->first;
				bool isChanged = false;

				// Preview
				ImGui::Image(
					(ImTextureID)tex->GetSRV(), 
					{ glyph.size.x, glyph.size.y }, 
					{ glyph.uvRect.x, glyph.uvRect.y }, 
					{ glyph.uvRect.z, glyph.uvRect.w }
				);

				// Highlight offset
				ImGui::GetWindowDrawList()->AddLine(
					{ ImGui::GetItemRectMin().x,					ImGui::GetItemRectMin().y + glyph.offset.y },
					{ ImGui::GetItemRectMin().x + glyph.size.x,		ImGui::GetItemRectMin().y + glyph.offset.y },
					IM_COL32(255, 0, 0, 255)
				);
				ImGui::GetWindowDrawList()->AddLine(
					{ ImGui::GetItemRectMin().x + glyph.offset.x,	ImGui::GetItemRectMin().y				 },
					{ ImGui::GetItemRectMin().x + glyph.offset.x,	ImGui::GetItemRectMin().y + glyph.size.y },
					IM_COL32(255, 0, 0, 255)
				);

				// Highlight advance
				ImGui::GetWindowDrawList()->AddLine(
					{ ImGui::GetItemRectMin().x + glyph.offset.x + glyph.advance, ImGui::GetItemRectMin().y				   },
					{ ImGui::GetItemRectMin().x + glyph.offset.x + glyph.advance, ImGui::GetItemRectMin().y + glyph.size.y },
					IM_COL32(0, 255, 0, 255)
				);

				std::string charStr = "";
				if (inputCodepoint >= 32u)
					charStr = std::string(1, (char)inputCodepoint);
				else
					charStr = "?";
				
				ImGui::Text("Codepoint:");
				ImGui::SameLine();
				if (ImGui::InputScalar("##InputCodepointInt", ImGuiDataType_U32, &inputCodepoint))
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
					if (c == (char)inputCodepoint)
						c = '\0';

					if (c != '\0')
					{
						if (!charStr.empty())
							inputCodepoint = (UINT)charStr[0];
						else
							inputCodepoint = -1;
						isChanged = true;
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

				ImGui::DragFloat2("Offset", &glyph.offset.x, 0.1f);
				ImGuiUtils::LockMouseOnActive();

				ImGui::DragFloat("Advance", &glyph.advance, 0.1f);
				ImGuiUtils::LockMouseOnActive();

				if (isChanged && inputCodepoint != -1)
				{
					// Move the glyph to the new codepoint & update selection
					_glyphs[inputCodepoint] = glyph;
					_uiSelectedGlyphID = inputCodepoint;
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
	
	if (ImGui::TreeNode("General"))
	{
		ImGui::Text("Total Glyphs: %zu", _glyphs.size());

		int fallbackGlyphID = (int)_fallbackGlyphID;
		ImGui::Text("Fallback Glyph:"); ImGui::SameLine();
		if (ImGui::InputInt("##FallbackGlyph", &fallbackGlyphID))
		{
			_fallbackGlyphID = (UINT)fallbackGlyphID;

			if (_glyphs.find(_fallbackGlyphID) == _glyphs.end())
				_fallbackGlyphID = CONTENT_NULL;
		}

		ImGui::Text("Line Height:"); ImGui::SameLine();
		ImGui::DragFloat("##LineHeight", &_lineHeight, 0.1f, 0.0f);

		ImGui::Text("Spacing:"); ImGui::SameLine();
		ImGui::DragFloat("##Spacing", &_spacing, 0.1f, 0.0f);

		ImGui::TreePop();
	}

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
				ImGui::Selectable(std::format("U+{:04X}", codepoint).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
					if (ImGui::IsWindowAppearing())
						ImGui::SetScrollHereY();
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::Image(
					(ImTextureID)tex->GetSRV(), 
					{ glyph.size.x, glyph.size.y }, 
					{ glyph.uvRect.x, glyph.uvRect.y }, 
					{ glyph.uvRect.z, glyph.uvRect.w }
				);
			}

			ImGui::EndTable();
		}
		*/

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
			"!@#$ % ^&*()_ + -= [] {} | ; ':\",./<>?\n"
			"\n"
			"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n"
			"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz\n"
			"AaB bCc DdEeF fGgHhI iJjKkL lMmNnOo PpQqRrSsTtUu VvWwXxYyZz\n"
			"A aBbCc  DdEeF fGgHhI iJjKk L l M mNn  Oo PpQqR rSs TtUu VvWw   XxYy Zz";

		ImGui::SeparatorText("Example Text");
		ImGui::InputTextMultiline("##InputText", &inputText);

		auto textMesh = Generate(inputText);
		ImGui::SeparatorText("Atlas Text");
		ImVec2 startPos = ImGui::GetCursorScreenPos();

		static ImVec2 textSize = { 64, 16 };
		ImGui::Dummy(textSize + ImVec2(4, 4));
		textSize = { 64, 16 };

		for (size_t i = 0; i < textMesh.size(); i += 6)
		{
			if (i + 5 >= textMesh.size())
				break;

			GlyphVertex &vMin = textMesh[i + 0];
			GlyphVertex &vMax = textMesh[i + 5];

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

			textSize.x = max(textSize.x, (posMax.x - startPos.x));
			textSize.y = max(textSize.y, (posMax.y - startPos.y));
		}

		ImGui::SeparatorText("Info");
		ImGui::Text("Characters: %zu", inputText.length());
		ImGui::Text("Generated Vertices: %zu", textMesh.size());

		ImGui::TreePop();
	}

	return true;
}
#endif
