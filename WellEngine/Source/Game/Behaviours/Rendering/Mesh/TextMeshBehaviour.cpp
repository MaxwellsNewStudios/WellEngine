#include "stdafx.h"
#include "TextMeshBehaviour.h"
#include "Source/Game/Scenes/Scene.h"
#include "Source/Game/Behaviours/Debug/DebugPlayerBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

TextMeshBehaviour::~TextMeshBehaviour()
{
	if (_meshID != CONTENT_NULL)
		GetUnusedMeshIDs().push_back(_meshID);

	if (!GetScene()->IsDestroyed() && !GetEntity()->IsRemoved())
	{
		if (_meshBehaviour.IsValid())
		{
			Entity *meshEntity = _meshBehaviour.Get()->GetEntity();
			if (meshEntity)
				meshEntity->Destroy();
		}
	}
}

bool TextMeshBehaviour::Start()
{
	if (_name.empty())
		_name = "TextMeshBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();

	// Assign a mesh
	if (GetUnusedMeshIDs().empty())
	{
		static UINT counter = 0;
		_meshID = scene->GetContent()->AddMesh(scene->GetDevice(), std::format("TextMesh_{}", counter++));
	}
	else
	{
		_meshID = GetUnusedMeshIDs().back();
		GetUnusedMeshIDs().pop_back();
	}

	// Create a mesh behaviour as a child entity
	Material mat = {};
	mat.textureID = scene->GetContent()->GetTextureID("White");
	mat.vsID = scene->GetContent()->GetShaderID("VS_TextDefault");
	mat.psID = scene->GetContent()->GetShaderID("PS_TextDefault");

	Entity *entity;
	if (!scene->CreateMeshEntity(&entity, "Text Mesh", _meshID, mat, false, false, false))
	{
		Warn("Failed to create mesh entity!");
		return true;
	}
	entity->SetParent(GetEntity());
	entity->SetSerialization(false);
	entity->SetShowInHierarchy(false);

	MeshBehaviour *meshBehaviour;
	entity->GetBehaviourByType<MeshBehaviour>(meshBehaviour);
	_meshBehaviour = meshBehaviour;

	meshBehaviour->SetAlphaCutoff(0.5f);

	return true;
}

#ifdef USE_IMGUI
bool TextMeshBehaviour::RenderUI()
{
	if (!_meshBehaviour.IsValid())
		return true;

	if (ImGui::Button("Select Mesh"))
		GetScene()->GetDebugPlayer()->Select(_meshBehaviour.Get()->GetEntity());

	Transform *transform = GetEntity()->GetTransform();
	float pixelsPerUnit = 1.0f / transform->GetScale(Local).y;

	if (ImGui::DragFloat("Pixels Per Unit", &pixelsPerUnit, 0.1f, 0.1f))
	{
		if (pixelsPerUnit < 0.1f)
			pixelsPerUnit = 0.1f;

		float newScale = 1.0f / pixelsPerUnit;
		transform->SetScale({ newScale, newScale, newScale }, Local);
	}

	const Content *content = GetScene()->GetContent();

	// Set font atlas
	std::vector<std::string> fontNames;
	content->GetFontAtlasNames(&fontNames);

	ImGui::Text("Font Atlas:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::BeginCombo("##FontAtlasCombo", content->GetFontAtlasName(_fontAtlasID).c_str()))
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
		for (UINT i = 0; i < fontNames.size(); i++)
		{
			if (!filter.empty())
			{
				std::string lower = fontNames[i];
				std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

				if (lower.find(filter) == std::string::npos)
					continue;
			}

			bool isSelected = (_fontAtlasID == i);
			if (ImGui::Selectable(fontNames[i].c_str(), isSelected))
			{
				SetFontAtlasID(i);
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

	// Set text
	if (ImGui::InputTextMultiline("##InputText", &_text, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 6)))
	{
		SetText(_text);
	}

	return true;
}
#endif

bool TextMeshBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	std::string fontAtlasName = GetScene()->GetContent()->GetFontAtlasName(_fontAtlasID);
	obj.AddMember("Atlas", SerializerUtils::SerializeString(fontAtlasName, docAlloc), docAlloc);
	obj.AddMember("Text", SerializerUtils::SerializeString(_text, docAlloc), docAlloc);

	return true;
}
bool TextMeshBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Atlas"))
		_fontAtlasID = scene->GetContent()->GetFontAtlasID(obj["Atlas"].GetString());

	if (obj.HasMember("Text"))
		_text = obj["Text"].GetString();

	return true;
}
void TextMeshBehaviour::PostDeserialize()
{
	RecreateMesh();
}

void TextMeshBehaviour::RecreateMesh()
{
	if (_meshID == CONTENT_NULL)
	{
		Warn("No mesh ID set for TextMeshBehaviour!");
		return;
	}

	if (_text.empty())
		return;

	const Content *content = GetScene()->GetContent();

	const FontAtlas *fontAtlas = content->GetFontAtlas(_fontAtlasID);
	if (!fontAtlas)
	{
		Warn("Invalid font atlas ID set for TextMeshBehaviour!");
		return;
	}

	if (!_meshBehaviour.Get())
	{
		Warn("No valid MeshBehaviour found for TextMeshBehaviour!");
		return;
	}

	const Material *mat = _meshBehaviour.Get()->GetMaterial();

	Material newMat = *mat;
	newMat.textureID = fontAtlas->GetFontTextureID();

	if (!_meshBehaviour.Get()->SetMaterial(&newMat))
	{
		ErrMsg("Failed to set material for TextMeshBehaviour!");
		return;
	}

	auto glyphVerts = fontAtlas->Generate(_text);

	if (glyphVerts.empty())
		return;

	MeshData *meshData = fontAtlas->ToMesh(glyphVerts);

	_meshBehaviour.Get()->GetEntity()->SetEntityBounds(meshData->boundingBox);

	MeshD3D11 *mesh = content->GetMesh(_meshID);
	if (!mesh)
	{
		delete meshData;
		Warn("Invalid mesh ID set for TextMeshBehaviour!");
		return;
	}

	if (!mesh->Initialize(GetScene()->GetDevice(), &meshData))
	{
		delete meshData;
		Warn("Failed to initialize mesh for TextMeshBehaviour!");
		return;
	}
}

const std::string &TextMeshBehaviour::GetText() const
{
	return _text;
}
void TextMeshBehaviour::SetText(std::string_view text)
{
	_text = text;

	RecreateMesh();
}

UINT TextMeshBehaviour::GetFontAtlasID() const
{
	return _fontAtlasID;
}
void TextMeshBehaviour::SetFontAtlasID(UINT id)
{
	_fontAtlasID = id;

	RecreateMesh();
}

UINT TextMeshBehaviour::GetMeshID() const
{
	return _meshID;
}

MeshBehaviour *TextMeshBehaviour::GetMeshBehaviour() const
{
	return _meshBehaviour.Get();
}
