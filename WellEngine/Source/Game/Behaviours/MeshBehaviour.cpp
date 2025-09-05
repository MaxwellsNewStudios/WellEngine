#include "stdafx.h"
#include "Behaviours/MeshBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool MeshBehaviour::Start()
{
	if (_name.empty())
		_name = "MeshBehaviour"; // For categorization in ImGui.

	bool invalidMesh = false;
	bool invalidMaterial = false;

	if (_meshID == CONTENT_NULL)
		invalidMesh = true;

	if (_material)
	{
		if (!ValidateMaterial(&_material))
			invalidMaterial = true;
	}
	else
	{
		invalidMaterial = true;
	}

	auto device = GetScene()->GetDevice();
	auto content = GetScene()->GetContent();

	if (invalidMesh && invalidMaterial)
	{
		_meshID = content->GetMeshID("Error");
		_material = content->GetErrorMaterial();
	}
	else if (invalidMesh)
	{
		_meshID = content->GetMeshID("Fallback");
	}
	else if (invalidMaterial)
	{
		_material = content->GetDefaultMaterial();
	}

	MaterialProperties materialProperties = { };
	materialProperties.sampleNormal = _material->normalID != CONTENT_NULL;
	materialProperties.sampleSpecular = _material->specularID != CONTENT_NULL;
	materialProperties.sampleGlossiness = _material->glossinessID != CONTENT_NULL;
	materialProperties.sampleReflective = _material->reflectiveID != CONTENT_NULL;
	materialProperties.sampleAmbient = _material->ambientID != CONTENT_NULL;
	materialProperties.sampleOcclusion = _material->occlusionID != CONTENT_NULL;

	materialProperties.alphaCutoff = _alphaCutoff;
	materialProperties.specularFactor = _specularFactor;
	materialProperties.baseColor = _baseColor;
	materialProperties.metallic = _metallic;
	materialProperties.reflectivity = _reflectivity;

	if (!_materialBuffer.Initialize(device, sizeof(MaterialProperties), &materialProperties))
	{
		ErrMsg("Failed to initialize material buffer!");
		return false;
	}

	XMFLOAT4A pos = To4(GetTransform()->GetPosition());
	if (!_posBuffer.Initialize(device, sizeof(dx::XMFLOAT4A), &pos))
	{
		ErrMsg("Failed to initialize position buffer!");
		return false;
	}

	QueueUpdate();

	return true;
}

bool MeshBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (_updatePosBuffer)
	{
		BoundingOrientedBox worldSpaceBounds;
		StoreBounds(worldSpaceBounds);
		const XMFLOAT4A center = { worldSpaceBounds.Center.x, worldSpaceBounds.Center.y, worldSpaceBounds.Center.z, 0.0f };

		if (!_posBuffer.UpdateBuffer(GetScene()->GetContext(), &center))
		{
			ErrMsg("Failed to update position buffer!");
			return false;
		}

		_updatePosBuffer = false;
	}

	if (_updateMatBuffer)
	{
		MaterialProperties materialProperties = { };
		materialProperties.sampleNormal = _material->normalID != CONTENT_NULL;
		materialProperties.sampleSpecular = _material->specularID != CONTENT_NULL;
		materialProperties.sampleGlossiness = _material->glossinessID != CONTENT_NULL;
		materialProperties.sampleReflective = _material->reflectiveID != CONTENT_NULL;
		materialProperties.sampleAmbient = _material->ambientID != CONTENT_NULL;
		materialProperties.sampleOcclusion = _material->occlusionID != CONTENT_NULL;

		materialProperties.alphaCutoff = _alphaCutoff;
		materialProperties.specularFactor = _specularFactor;
		materialProperties.baseColor = _baseColor;
		materialProperties.metallic = _metallic;
		materialProperties.reflectivity = _reflectivity;

		if (!_materialBuffer.UpdateBuffer(GetScene()->GetContext(), &materialProperties))
		{
			ErrMsg("Failed to update material buffer!");
			return false;
		}

		_updateMatBuffer = false;
	}
	
	if (_psSettings.data && _psSettings.dirty)
	{
		_psSettings.buffer = std::make_unique<ConstantBufferD3D11>();

		if (!_psSettings.buffer.get()->Initialize(GetScene()->GetDevice(), _psSettings.size, _psSettings.data.get()))
		{
			ErrMsg("Failed to initialize pixel shader settings buffer!");
			return false;
		}

		_psSettings.dirty = false;
	}

	return true;
}

bool MeshBehaviour::Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
	if (rendererInfo.shadowCamera && !_castShadows)
		return true;

	const RenderQueueEntry entry = {
		ResourceGroup(_meshID, _material, _castShadows, _isOverlay, _shadowsOnly, _blendStateID),
		RenderInstance(dynamic_cast<Behaviour *>(this), sizeof(MeshBehaviour))
	};

	if (_isTransparent)
		queuer.QueueTransparent(entry);
	else
		queuer.QueueGeometry(entry);

	return true;
}

#ifdef USE_IMGUI
bool MeshBehaviour::RenderUI()
{
	Content *content = GetScene()->GetContent();

	ImGui::SeparatorText("Settings");

	if (ImGui::TreeNode("Material"))
	{
		int
			inputMeshID = (int)_meshID,
			inputBlendID = (int)_blendStateID,
			inputTexID = (int)_material->textureID,
			inputNormID = (int)_material->normalID,
			inputSpecID = (int)_material->specularID,
			inputGlossID = (int)_material->glossinessID,
			inputAmbID = (int)_material->ambientID,
			inputReflectID = (int)_material->reflectiveID,
			inputOcclusionID = (int)_material->occlusionID,
			inputSampID = (int)_material->samplerID,
			inputVSID = (int)_material->vsID,
			inputPSID = (int)_material->psID;

		static int previewSize = 128;
		ImGui::Text("Preview Size: ");
		ImGui::SameLine();
		ImGui::InputInt("##PreviewSize", &previewSize);
		ImGui::Dummy(ImVec2(0.0f, 5.0f));
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		float absPreviewSize = abs(static_cast<float>(previewSize));
		ImVec2 previewVec = ImVec2(absPreviewSize, absPreviewSize);

		std::vector<std::string> meshNames, textureNames, shaderNames, samplerNames, blendStateNames;
		content->GetMeshNames(&meshNames);
		content->GetTextureNames(&textureNames);
		content->GetShaderNames(&shaderNames);
		content->GetSamplerNames(&samplerNames);
		content->GetBlendStateNames(&blendStateNames);

		ImGui::PushID("Mats");
		bool isChanged = false;
		int id = 1;

		ImGuiComboFlags comboFlags = ImGuiComboFlags_None;
		comboFlags |= ImGuiComboFlags_HeightLarge;

		// Mesh
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());

			ImGui::Text("Mesh:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::BeginCombo("", content->GetMeshName((UINT)inputMeshID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				for (UINT i = 0; i < meshNames.size(); i++)
				{
					if (!filter.empty())
					{
						std::string lower = meshNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputMeshID == i);
					if (ImGui::Selectable(meshNames[i].c_str(), isSelected))
					{
						inputMeshID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::MESH)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputMeshID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Texture map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());

			ImGui::BeginGroup();
			ImGui::Text("Texture:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputTexID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputTexID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputTexID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
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
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (inputTexID != -1)
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputTexID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

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

			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Normal map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Normal:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputNormID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputNormID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputNormID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < textureNames.size(); i++)
				{
					if (textureNames[i].find("_Normal") == std::string::npos)
						continue;

					if (!filter.empty())
					{
						std::string lower = textureNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputNormID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputNormID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputNormID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			std::string name = content->GetTextureName((UINT)inputNormID);
			if (content->GetTexture((UINT)inputNormID) && name != "Uninitialized")
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputNormID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));
			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputNormID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Specular map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Specular:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputSpecID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();
				{
					bool isSelected = (inputSpecID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputSpecID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < textureNames.size(); i++)
				{
					if (textureNames[i].find("_Specular") == std::string::npos)
						continue;

					if (!filter.empty())
					{
						std::string lower = textureNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputSpecID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputSpecID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputSpecID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			std::string name = content->GetTextureName((UINT)inputSpecID);
			if (content->GetTexture((UINT)inputSpecID) && name != "Uninitialized")
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputSpecID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputSpecID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Glossiness map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Glossiness:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputGlossID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputGlossID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputGlossID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < textureNames.size(); i++)
				{
					if (textureNames[i].find("_Glossiness") == std::string::npos)
						continue;
					
					if (!filter.empty())
					{
						std::string lower = textureNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputGlossID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputGlossID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputGlossID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			std::string name = content->GetTextureName((UINT)inputGlossID);
			if (content->GetTexture((UINT)inputGlossID) && name != "Uninitialized")
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputGlossID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputGlossID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Ambient map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Ambient:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputAmbID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputAmbID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputAmbID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
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

					bool isSelected = (inputAmbID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputAmbID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputAmbID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			if (inputAmbID != -1)
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputAmbID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputAmbID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Reflection map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Reflection:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputReflectID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputReflectID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputReflectID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
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

					bool isSelected = (inputReflectID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputReflectID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputReflectID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			if (inputReflectID != -1)
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputReflectID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputReflectID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Occlusion map
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Ambient Occlusion:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetTextureName((UINT)inputOcclusionID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputOcclusionID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputOcclusionID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < textureNames.size(); i++)
				{
					if (textureNames[i].find("_Occlusion") == std::string::npos)
						continue;

					if (!filter.empty())
					{
						std::string lower = textureNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputOcclusionID == i);
					if (ImGui::Selectable(textureNames[i].c_str(), isSelected))
					{
						inputOcclusionID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputOcclusionID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			std::string name = content->GetTextureName((UINT)inputOcclusionID);
			if (content->GetTexture((UINT)inputOcclusionID) && name != "Uninitialized")
				ImGui::Image((ImTextureID)content->GetTexture((UINT)inputOcclusionID)->GetSRV(), previewVec, ImVec2(1, 1), ImVec2(0, 0));

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputOcclusionID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Sampler
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());

			ImGui::BeginGroup();
			ImGui::Text("Sampler:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetSamplerName((UINT)inputSampID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputSampID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputSampID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < samplerNames.size(); i++)
				{
					if (!filter.empty())
					{
						std::string lower = samplerNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputSampID == i);
					if (ImGui::Selectable(samplerNames[i].c_str(), isSelected))
					{
						inputSampID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputSampID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::SAMPLER)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputSampID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Blend State
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Blend State:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetBlendStateName((UINT)inputBlendID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputBlendID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputBlendID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < blendStateNames.size(); i++)
				{
					if (!filter.empty())
					{
						std::string lower = blendStateNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					bool isSelected = (inputBlendID == i);
					if (ImGui::Selectable(blendStateNames[i].c_str(), isSelected))
					{
						inputBlendID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputBlendID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::BLENDSTATE)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputBlendID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Vertex Shader
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Vertex Shader:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetShaderName((UINT)inputVSID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputVSID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputVSID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < shaderNames.size(); i++)
				{
					if (!filter.empty())
					{
						std::string lower = shaderNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					if (!shaderNames[i].starts_with("VS_"))
						continue;

					bool isSelected = (inputVSID == i);
					if (ImGui::Selectable(shaderNames[i].c_str(), isSelected))
					{
						inputVSID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputVSID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::SHADER_VERTEX)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputVSID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();
		ImGui::Dummy(ImVec2(0.0f, 10.0f));

		// Pixel Shader
		{
			ImGui::PushID(("Param " + std::to_string(id++)).c_str());
			ImGui::BeginGroup();

			ImGui::Text("Pixel Shader:"); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 6.0f);
			if (ImGui::BeginCombo("", content->GetShaderName((UINT)inputPSID).c_str(), comboFlags))
			{
				static std::string filter = "";
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(8.0f);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				{
					bool isSelected = (inputPSID == -1);
					if (ImGui::Selectable("None", isSelected))
					{
						inputPSID = -1;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				for (UINT i = 0; i < shaderNames.size(); i++)
				{
					if (!filter.empty())
					{
						std::string lower = shaderNames[i];
						std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

						if (lower.find(filter) == std::string::npos)
							continue;
					}

					if (!shaderNames[i].starts_with("PS_"))
						continue;

					bool isSelected = (inputPSID == i);
					if (ImGui::Selectable(shaderNames[i].c_str(), isSelected))
					{
						inputPSID = i;
						isChanged = true;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
			if (ImGui::Button("X"))
			{
				inputPSID = -1;
				isChanged = true;
			}
			ImGuiUtils::EndButtonStyle();

			ImGui::EndGroup();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::SHADER_PIXEL)))
				{
					IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
					ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

					inputPSID = contentPayload.id;
					isChanged = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		}
		ImGui::Separator();

		ImGui::PopID();

		if (isChanged)
		{
			int texCount = (int)content->GetTextureCount();

			inputMeshID += (int)content->GetMeshCount();
			inputMeshID %= (int)content->GetMeshCount();

			inputTexID += texCount;
			inputTexID %= texCount;

			if (inputNormID != -1)
			{
				if (inputNormID < 0)
					inputNormID++;

				inputNormID += texCount;
				inputNormID %= texCount;
			}

			if (inputSpecID != -1)
			{
				if (inputSpecID < 0)
					inputSpecID++;

				inputSpecID += texCount;
				inputSpecID %= texCount;
			}

			if (inputGlossID != -1)
			{
				if (inputGlossID < 0)
					inputGlossID++;

				inputGlossID += texCount;
				inputGlossID %= texCount;
			}

			if (inputAmbID != -1)
			{
				if (inputAmbID < 0)
					inputAmbID++;

				inputAmbID += texCount;
				inputAmbID %= texCount;
			}

			if (inputReflectID != -1)
			{
				if (inputReflectID < 0)
					inputReflectID++;

				inputReflectID += texCount;
				inputReflectID %= texCount;
			}

			if (inputOcclusionID != -1)
			{
				if (inputOcclusionID < 0)
					inputOcclusionID++;

				inputOcclusionID += texCount;
				inputOcclusionID %= texCount;
			}

			if (inputSampID != -1)
			{
				if (inputSampID < 0)
					inputSampID++;

				inputSampID += (int)content->GetSamplerCount();
				inputSampID %= (int)content->GetSamplerCount();
			}

			if (inputBlendID != -1)
			{
				if (inputBlendID < 0)
					inputBlendID++;

				inputBlendID += (int)content->GetBlendStateCount();
				inputBlendID %= (int)content->GetBlendStateCount();
			}

			if (inputMeshID != _meshID)
				SetMeshID((UINT)inputMeshID, true);

			if (inputMeshID != _meshID)
				SetMeshID((UINT)inputMeshID, true);

			Material newMat;
			newMat.textureID	= (UINT)inputTexID;
			newMat.normalID		= (UINT)inputNormID;
			newMat.specularID	= (UINT)inputSpecID;
			newMat.glossinessID	= (UINT)inputGlossID;
			newMat.ambientID	= (UINT)inputAmbID;
			newMat.reflectiveID	= (UINT)inputReflectID;
			newMat.occlusionID	= (UINT)inputOcclusionID;
			newMat.samplerID	= (UINT)inputSampID;
			newMat.vsID			= (UINT)inputVSID;
			newMat.psID			= (UINT)inputPSID;
			_blendStateID		= (UINT)inputBlendID;

			if (!SetMaterial(&newMat))
			{
				ErrMsg("Failed to set material!");
				ImGui::TreePop();
				return false;
			}

			_updateMatBuffer = true;
		}

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Renderer Parameters"))
	{
		if (ImGui::RadioButton("Overlay", _isOverlay))
			_isOverlay = !_isOverlay;

		if (ImGui::RadioButton("Transparent", _isTransparent))
			_isTransparent = !_isTransparent;

		if (ImGui::RadioButton("Cast Shadows", _castShadows))
			_castShadows = !_castShadows;

		if (ImGui::RadioButton("Shadows Only", _shadowsOnly))
			_shadowsOnly = !_shadowsOnly;

		if (ImGui::SliderFloat("Alpha Cutoff", &_alphaCutoff, 0, 1))
			_updateMatBuffer = true;

		if (ImGui::SliderFloat("Metallic", &_metallic, 0.0f, 1.0f))
			_updateMatBuffer = true;

		if (ImGui::SliderFloat("Reflectivity", &_reflectivity, 0.0f, 1.0f))
			_updateMatBuffer = true;

		if (ImGui::DragFloat("Specular Factor", &_specularFactor, 0.001f))
			_updateMatBuffer = true;
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::ColorEdit4("Base Color", &_baseColor.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs))
			_updateMatBuffer = true;

		if (ImGui::TreeNode("Shader Settings"))
		{
			// Get all pixel shaders with unique settings
			static const UINT triPlanarID = content->GetShaderID("PS_TriPlanar");

			if (_psSettings.data)
			{
				if (_material->psID == triPlanarID)
				{
					ImGui::SeparatorText("Triplanar Settings");

					TriplanarSettings *settings = _psSettings.GetData<TriplanarSettings>();

					ImGui::Text("Texture Scale:"); ImGui::SameLine();
					if (ImGui::DragFloat2("##TextureScale", &(settings->texSize.x), 0.01f))
						_psSettings.dirty = true;
					ImGuiUtils::LockMouseOnActive();

					ImGui::Text("Blend Sharpness:"); ImGui::SameLine();
					if (ImGui::DragFloat("##BlendSharpness", &(settings->blendSharpness), 0.01f))
					{
						settings->blendSharpness = max(settings->blendSharpness, 0.001f);
						_psSettings.dirty = true;
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::Checkbox("Correct Normals", &(settings->flipWithNormal)))
						_psSettings.dirty = true;
				}

				ImGui::Separator();

				if (ImGui::Button("Copy Settings To All Instances"))
				{
					SceneContents::SceneIterator entIter = GetScene()->GetSceneHolder()->GetEntities();
					while (Entity *ent = entIter.Step())
					{
						MeshBehaviour *mesh = nullptr;
						if (ent->GetBehaviourByType<MeshBehaviour>(mesh))
						{
							if (mesh->GetMaterial()->psID == _material->psID)
							{
								mesh->CopyPSSettings(_psSettings);
							}
						}
					}
				}
			}
			else
			{
				if (ImGui::Button("Create PS Settings Buffer"))
				{
					if (_material->psID == triPlanarID)
					{
						TriplanarSettings settings{};
						SetPSSettings<TriplanarSettings>(settings);
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	ImGui::SeparatorText("Tools");

	// Mesh collider visualization
	{
		static bool visualizeMeshCollider = false;

		ImGui::Checkbox("Visualize Mesh Collider", &visualizeMeshCollider);
		if (visualizeMeshCollider)
		{
			static int meshColliderDepth = 0;
			static dx::XMFLOAT4 meshColliderColor = { 1.0f, 0.0f, 1.0f, 0.5f };
			static bool compact = true;
			static bool drawTris = false;
			static bool overlay = false;
			static bool recursive = false;

			if (ImGui::TreeNode("Visualization Parameters"))
			{
				ImGui::ColorEdit4("Color##VisualizationColor", &meshColliderColor.x);

				if (ImGui::InputInt("Depth##MeshColliderDepthInt", &meshColliderDepth))
					meshColliderDepth = std::clamp(meshColliderDepth, 0, (int)MeshCollider::MAX_DEPTH);

				ImGui::Checkbox("Compact Bounds", &compact);

				ImGui::Checkbox("Draw Tris", &drawTris);

				ImGui::Checkbox("Overlay", &overlay);

				ImGui::Checkbox("Recursive", &recursive);

				ImGui::TreePop();
			}

			MeshD3D11 *mesh = content->GetMesh(_meshID);
			const MeshCollider &meshCollider = mesh->GetMeshCollider();

			meshCollider.VisualizeTreeDepth(
				GetTransform()->GetMatrix(World), meshColliderDepth,
				compact, meshColliderColor, drawTris, overlay, recursive
			);
		}
	}
	ImGui::Dummy({ 0, 4 });

	// Global actions for entities sharing this mesh
	{
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.35f, 0.55f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.35f, 0.65f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.35f, 0.75f, 0.7f));
		if (ImGui::Button("Set All Instances to Static"))
		{
			SceneContents::SceneIterator entIter = GetScene()->GetSceneHolder()->GetEntities();
			while (Entity *ent = entIter.Step())
			{
				MeshBehaviour *mesh = nullptr;
				if (ent->GetBehaviourByType<MeshBehaviour>(mesh))
				{
					if (mesh->GetMeshID() == _meshID)
						ent->SetStatic(true);
				}
			}
		}
		ImGui::PopStyleColor(3);

		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.05f, 0.55f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.05f, 0.65f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.05f, 0.75f, 0.7f));
		if (ImGui::Button("Set All Instances to Non-Static"))
		{
			SceneContents::SceneIterator entIter = GetScene()->GetSceneHolder()->GetEntities();
			while (Entity *ent = entIter.Step())
			{
				MeshBehaviour *mesh = nullptr;
				if (ent->GetBehaviourByType<MeshBehaviour>(mesh))
				{
					if (mesh->GetMeshID() == _meshID)
						ent->SetStatic(false);
				}
			}
		}
		ImGui::PopStyleColor(3);
	}

	ImGui::SeparatorText("Debug");

	// LOD data
	{
		UINT lodCount = GetLODCount();
		ImGui::Text("LOD: %d / %d", _lastUsedLODIndex + 1, lodCount);
		ImGui::Text(std::format("Normalized Distance: {}", 1.0f + _lastUsedLODDist * (lodCount - 1.0f)).c_str());

		CameraBehaviour *camera = GetScene()->GetViewCamera();
		if (camera)
		{
			dx::BoundingOrientedBox bounds;
			StoreBounds(bounds);
			auto meshPos = Load(bounds.Center);

			float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
			float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

			CameraPlanes camPlanes = camera->GetPlanes();
			float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
			float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

			ImGui::Text(std::format("Scale Factor: ({} - {})", lodDistMin, lodDistMax).c_str());
		}
	}

	return true;
}
#endif

bool MeshBehaviour::BindBuffers(ID3D11DeviceContext *context)
{
	ID3D11Buffer *const wmBuffer = GetTransform()->GetConstantBuffer();
	context->VSSetConstantBuffers(0, 1, &wmBuffer);

	ID3D11Buffer *const materialBuffer = _materialBuffer.GetBuffer();
	context->PSSetConstantBuffers(2, 1, &materialBuffer);

	if (_psSettings.buffer)
	{
		ID3D11Buffer *const psSettingsBuffer = _psSettings.buffer.get()->GetBuffer();
		context->PSSetConstantBuffers(4, 1, &psSettingsBuffer);
	}

	return true;
}

void MeshBehaviour::OnDirty()
{
	_updatePosBuffer = true;
	_recalculateBounds = true;
}

bool MeshBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) 
{
	Content *content = GetScene()->GetContent();

	json::Value meshStr(json::kStringType);
	meshStr.SetString(content->GetMeshName(_meshID).c_str(), docAlloc);
	obj.AddMember("Mesh", meshStr, docAlloc);

	obj.AddMember("Transparent", _isTransparent, docAlloc);
	obj.AddMember("Shadow Caster", _castShadows, docAlloc);
	obj.AddMember("Shadows Only", _shadowsOnly, docAlloc);
	obj.AddMember("Alpha Cutoff", _alphaCutoff, docAlloc);
	obj.AddMember("Specular Factor", _specularFactor, docAlloc);
	obj.AddMember("Base Color", SerializerUtils::SerializeVec(_baseColor, docAlloc), docAlloc);
	obj.AddMember("Metallic", _metallic, docAlloc);
	obj.AddMember("Reflectivity", _reflectivity, docAlloc);

	if (_psSettings.data)
	{
		json::Value psSettings(json::kObjectType);
		psSettings.AddMember("Size", _psSettings.size, docAlloc);

		json::Value dataArr(json::kArrayType);
		char *data = _psSettings.data.get();

		for (UINT i = 0; i < _psSettings.size; i++)
			dataArr.PushBack((UINT)data[i], docAlloc);

		psSettings.AddMember("Data", dataArr, docAlloc);
		obj.AddMember("PS Settings", psSettings, docAlloc);
	}

	std::string contentName;

	contentName = content->GetTextureName(_material->textureID);
	if (contentName != "Uninitialized") 
	{
		json::Value textureStr(json::kStringType);
		textureStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Tex", textureStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->normalID);
	if (contentName != "Uninitialized")
	{
		json::Value normalStr(json::kStringType);
		normalStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Normal", normalStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->specularID);
	if (contentName != "Uninitialized")
	{
		json::Value specularStr(json::kStringType);
		specularStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Specular", specularStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->glossinessID);
	if (contentName != "Uninitialized")
	{
		json::Value glossStr(json::kStringType);
		glossStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Gloss", glossStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->ambientID);
	if (contentName != "Uninitialized")
	{
		json::Value ambientStr(json::kStringType);
		ambientStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Ambient", ambientStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->reflectiveID);
	if (contentName != "Uninitialized")
	{
		json::Value reflectiveStr(json::kStringType);
		reflectiveStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Reflection", reflectiveStr, docAlloc);
	}

	contentName = content->GetTextureName(_material->occlusionID);
	if (contentName != "Uninitialized")
	{
		json::Value occlusionStr(json::kStringType);
		occlusionStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("AO", occlusionStr, docAlloc);
	}

	contentName = content->GetSamplerName(_material->samplerID);
	if (contentName != "Uninitialized")
	{
		json::Value samplerStr(json::kStringType);
		samplerStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Sampler", samplerStr, docAlloc);
	}

	contentName = content->GetBlendStateName(_blendStateID);
	if (contentName != "Uninitialized")
	{
		json::Value blendStateStr(json::kStringType);
		blendStateStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("Blend State", blendStateStr, docAlloc);
	}

	contentName = content->GetShaderName(_material->vsID);
	if (contentName != "Uninitialized")
	{
		json::Value vertexStr(json::kStringType);
		vertexStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("VS", vertexStr, docAlloc);
	}

	contentName = content->GetShaderName(_material->psID);
	if (contentName != "Uninitialized")
	{
		json::Value pixelStr(json::kStringType);
		pixelStr.SetString(contentName.c_str(), docAlloc);
		obj.AddMember("PS", pixelStr, docAlloc);
	}

	return true;
}
bool MeshBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Transparent"))
		_isTransparent = obj["Transparent"].GetBool();
	
	if (obj.HasMember("Shadow Caster"))
		_castShadows = obj["Shadow Caster"].GetBool();
	
	if (obj.HasMember("Shadows Only"))
		_shadowsOnly = obj["Shadows Only"].GetBool();

	if (obj.HasMember("Alpha Cutoff"))
		_alphaCutoff = obj["Alpha Cutoff"].GetFloat();

	if (obj.HasMember("Specular Factor"))
		_specularFactor = obj["Specular Factor"].GetFloat();

	if (obj.HasMember("Base Color"))
		SerializerUtils::DeserializeVec(_baseColor, obj["Base Color"]);

	if (obj.HasMember("Metallic"))
		_metallic = obj["Metallic"].GetFloat();

	if (obj.HasMember("Reflectivity"))
		_reflectivity = obj["Reflectivity"].GetFloat();

	if (obj.HasMember("PS Settings"))
	{
		const json::Value &psSettingsObj = obj["PS Settings"];
		const json::Value &dataArr = psSettingsObj["Data"];

		UINT psSettingsSize = psSettingsObj["Size"].GetUint();
		char *psSettingsData = new char[psSettingsSize];

		for (UINT i = 0; i < psSettingsSize; i++)
			psSettingsData[i] = (char)dataArr[i].GetUint();

		_psSettings = ShaderSettings(); // Reset previous settings
		_psSettings.size = psSettingsSize;
		_psSettings.data = std::make_unique<char[]>(psSettingsSize);
		_psSettings.dirty = true;

		memcpy(_psSettings.data.get(), psSettingsData, psSettingsSize);
		delete[] psSettingsData;
	}

	_deserializedMesh = std::make_unique<DeserializedMesh>();

	if (obj.HasMember("Mesh"))
		_deserializedMesh->mesh = obj["Mesh"].GetString();

	if (obj.HasMember("Tex")) 
		_deserializedMesh->texture = obj["Tex"].GetString();

	if (obj.HasMember("Normal"))
		_deserializedMesh->normal = obj["Normal"].GetString();

	if (obj.HasMember("Specular"))
		_deserializedMesh->specular = obj["Specular"].GetString();

	if (obj.HasMember("Gloss"))
		_deserializedMesh->glossiness = obj["Gloss"].GetString();

	if (obj.HasMember("Ambient"))
		_deserializedMesh->ambient = obj["Ambient"].GetString();

	if (obj.HasMember("Reflection"))
		_deserializedMesh->reflect = obj["Reflection"].GetString();

	if (obj.HasMember("AO"))
		_deserializedMesh->occlusion = obj["AO"].GetString();

	if (obj.HasMember("Sampler"))
		_deserializedMesh->sampler = obj["Sampler"].GetString();

	if (obj.HasMember("Blend State"))
		_deserializedMesh->blendState = obj["Blend State"].GetString();

	if (obj.HasMember("VS"))
		_deserializedMesh->vs = obj["VS"].GetString();

	if (obj.HasMember("PS"))
		_deserializedMesh->ps = obj["PS"].GetString();

	return true;
}
void MeshBehaviour::PostDeserialize()
{
	if (!_deserializedMesh)
	{
		ErrMsg("Deserialized mesh is nullptr!");
		return;
	}

	Content *content = GetScene()->GetContent();

	if (!_deserializedMesh->mesh.empty())
		_meshID = content->GetMeshID(_deserializedMesh->mesh);

	if (!_deserializedMesh->blendState.empty())
		_blendStateID = content->GetBlendStateID(_deserializedMesh->blendState);

	// Material section
	Material mat = Material();

	if (!_deserializedMesh->texture.empty())
		mat.textureID = content->GetTextureID(_deserializedMesh->texture);

	if (!_deserializedMesh->normal.empty())
		mat.normalID = content->GetTextureID(_deserializedMesh->normal);

	if (!_deserializedMesh->specular.empty())
		mat.specularID = content->GetTextureID(_deserializedMesh->specular);

	if (!_deserializedMesh->glossiness.empty())
		mat.glossinessID = content->GetTextureID(_deserializedMesh->glossiness);

	if (!_deserializedMesh->ambient.empty())
		mat.ambientID = content->GetTextureID(_deserializedMesh->ambient);

	if (!_deserializedMesh->reflect.empty())
		mat.reflectiveID = content->GetTextureID(_deserializedMesh->reflect);

	if (!_deserializedMesh->occlusion.empty())
		mat.occlusionID = content->GetTextureID(_deserializedMesh->occlusion);

	if (!_deserializedMesh->sampler.empty())
		mat.samplerID = content->GetSamplerID(_deserializedMesh->sampler);

	if (!_deserializedMesh->vs.empty())
		mat.vsID = content->GetShaderID(_deserializedMesh->vs);

	if (!_deserializedMesh->ps.empty())
		mat.psID = content->GetShaderID(_deserializedMesh->ps);

	SetMeshID(_meshID, true);
	if (!SetMaterial(&mat))
		ErrMsg("Failed to set material!");

	_deserializedMesh.release();
}


bool MeshBehaviour::ValidateMaterial(const Material **material)
{
	if (!material)
	{
		ErrMsg("**Material is nullptr!");
		return false;
	}

	if (!*material)
	{
		ErrMsg("*Material is nullptr!");
		return false;
	}

	(*material) = GetScene()->GetContent()->GetOrAddMaterial(**material);
	return true;
}

void MeshBehaviour::SetMeshID(UINT meshID, bool updateBounds)
{
	_meshID = meshID;
	_updateMatBuffer = true;

	if (updateBounds)
	{
		BoundingOrientedBox newBounds = GetScene()->GetContent()->GetMesh(meshID)->GetBoundingOrientedBox();
		SetBounds(newBounds);
		GetEntity()->SetEntityBounds(newBounds);
	}
}
void MeshBehaviour::SetBlendStateID(UINT blendStateID)
{
	_blendStateID = blendStateID;
}
bool MeshBehaviour::SetMaterial(const Material *material)
{
	if (IsInitialized())
	{
		if (!ValidateMaterial(&material))
		{
			ErrMsg("Failed to validate material!");
			return false;
		}
	}

	_material = material;
	_updateMatBuffer = true;
	return true;
}
void MeshBehaviour::SetTransparent(bool state)
{
	_isTransparent = state;
}
void MeshBehaviour::SetOverlay(bool state)
{
	_isOverlay = state;
}
void MeshBehaviour::SetCastShadows(bool state)
{
	_castShadows = state;
}
void MeshBehaviour::SetShadowsOnly(bool state)
{
	_shadowsOnly = state;
}
void MeshBehaviour::SetAlphaCutoff(float value)
{
	_alphaCutoff = value;
	_updateMatBuffer = true;
}
void MeshBehaviour::SetColor(const dx::XMFLOAT4 &color)
{
	_baseColor = color;
}
void MeshBehaviour::SetBounds(BoundingOrientedBox &newBounds)
{
	_bounds = newBounds;
	_recalculateBounds = true;
}

UINT MeshBehaviour::GetMeshID() const
{
	return _meshID;
}
UINT MeshBehaviour::GetBlendStateID() const
{
	return _blendStateID;
}
const Material *MeshBehaviour::GetMaterial() const
{
	return _material;
}

void MeshBehaviour::SetLastUsedLOD(UINT lodIndex, float normalizedDist)
{
	_lastUsedLODIndex = lodIndex;
	_lastUsedLODDist = normalizedDist;
}
UINT MeshBehaviour::GetLastUsedLODIndex() const
{
	return _lastUsedLODIndex;
}
float MeshBehaviour::GetLastUsedLODDist() const
{
	return _lastUsedLODDist;
}
UINT MeshBehaviour::GetLODCount() const
{
	return GetScene()->GetContent()->GetMesh(_meshID)->GetNrOfSubMeshes();
}

void MeshBehaviour::StoreBounds(BoundingOrientedBox &meshBounds)
{
	if (_recalculateBounds)
	{
		XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		
		_bounds.Transform(_transformedBounds, Load(&worldMatrix));
		_recalculateBounds = false;
	}

	meshBounds = _transformedBounds;
}
