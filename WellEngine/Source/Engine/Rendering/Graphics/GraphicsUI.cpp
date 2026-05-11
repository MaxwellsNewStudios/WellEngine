#include "stdafx.h"
#include "Graphics.h"
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Engine/Debug/DebugDrawer.h"
#include "Engine/Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


#ifdef USE_IMGUI
bool Graphics::BeginUIRender()
{
	ZoneScopedXC(RandomUniqueColor());

	_context->OMSetRenderTargets(1, _rtv.GetAddressOf(), _dsView.Get());

	ImGuiIO &io = ImGui::GetIO();
	io.NavActive = false;

	auto &input = Input::Instance();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	_backgroundDockID = ImGui::DockSpaceOverViewport();

	if (input.IsCursorLocked())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX); // Hide mouse cursor when locked
	}

#ifdef USE_IMGUIZMO
	ImGuizmo::BeginFrame();
	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	ImGuizmo::SetGizmoSizeClipSpace(DebugData::Get().transformScale);

	auto windowPos = input.GetWindowPos();
	auto scenePos = input.GetSceneViewPos();
	auto sceneSize = input.GetSceneViewSize();
	ImGuizmo::SetRect(windowPos.x + scenePos.x, windowPos.y + scenePos.y, sceneSize.x, sceneSize.y);
#endif

	return true;
}
bool Graphics::EndUIRender() const
{
	ZoneScopedXC(RandomUniqueColor());

	//_context->OMSetRenderTargets(1, _imGuiRtv.GetAddressOf(), nullptr);
	_context->OMSetRenderTargets(1, _rtv.GetAddressOf(), nullptr);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO &io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	return true;
}

bool Graphics::RenderUI(TimeUtils &time)
{
	ZoneScopedXC(RandomUniqueColor());

	ImGui::SeparatorText("Info");
	ImGui::Text(std::format("Window Resolution: {}x{}", (int)_viewport.Width, (int)_viewport.Height).c_str());
	ImGui::Text(std::format("Scene Resolution: {}x{}", (int)_viewportSceneView.Width, (int)_viewportSceneView.Height).c_str());

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Settings");

	// Render type dropdown
	{
		const char *renderTypeNames[(int)RenderType::COUNT] = {
			"Default",
			"Position",
			"Normal",
			"Ambient",
			"Diffuse",
			"Depth",
			"Shadow",
			"Reflection",
			"Reflectivity",
			"Specular",
			"Specular Strength",
			"UV Coordinates",
			"Occlusion",
			"Transparency",
			"Light Tiles",
			"Overdraw"
		};

		ImGui::Text("Render Type:");
		ImGui::SameLine();
		if (ImGui::BeginCombo("##RenderTypeCombo", renderTypeNames[(int)_renderOutput], ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < (int)RenderType::COUNT; i++)
			{
				const bool isSelected = (i == (int)_renderOutput);
				if (ImGui::Selectable(renderTypeNames[i], isSelected))
					_renderOutput = (RenderType)i;

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (_renderOutput == RenderType::OVERDRAW)
		{
			ImGui::DragFloat4("##OverdrawBlendFactor", &_overdrawBlendFactor.x, 0.01f);

			ImGui::Checkbox("Include Discarded", &_overdrawIncludeDiscards);
			ImGui::SetItemTooltip(
				"Include triangles that would be discarded by the GPU.\n"
				"Easier to read & prevents flickering when moving the camera,\n"
				"but gives a less accurate representation of the overdraw."
			);

			ImGui::Dummy(ImVec2(0.0f, 8.0f));
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Skybox Shader dropdown
	{
		std::vector<std::string> shaderNames;
		_content->GetShaderNames(&shaderNames);

		ImGui::Text("Skybox:");
		ImGui::SameLine();
		if (ImGui::BeginCombo("##SkyboxShaderCombo", _skyboxPsID == CONTENT_NULL ? "None" : (shaderNames[_skyboxPsID].c_str())))
		{
			// Add "None" option
			{
				const bool isNoneSelected = (_skyboxPsID == CONTENT_NULL);
				if (ImGui::Selectable("None", isNoneSelected))
					SetSkyboxShaderID(CONTENT_NULL);

				if (isNoneSelected)
					ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < shaderNames.size(); i++)
			{
				std::string &shaderName = shaderNames[i];

				if (!shaderName.starts_with("PS_Skybox"))
					continue; // Only show pixel shaders

				const bool isSelected = (_skyboxPsID == i);
				if (ImGui::Selectable(shaderName.c_str(), isSelected))
					SetSkyboxShaderID(i);

				if (isSelected)
					ImGui::SetItemDefaultFocus();

			}
			ImGui::EndCombo();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Environment Cubemap dropdown
	{
		std::vector<std::string> cubemapNames;
		_content->GetCubemapNames(&cubemapNames);

		ImGui::Text("Cubemap:");
		ImGui::SameLine();
		if (ImGui::BeginCombo("##EnvironmentCubemapCombo", _envCubemapID == CONTENT_NULL ? "None" : (cubemapNames[_envCubemapID].c_str()), ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < cubemapNames.size(); i++)
			{
				std::string &cubemapName = cubemapNames[i];

				const bool isSelected = (_envCubemapID == i);
				if (ImGui::Selectable(cubemapName.c_str(), isSelected))
					SetEnvironmentCubemapID(i);

				if (isSelected)
					ImGui::SetItemDefaultFocus();

			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::CUBEMAP)))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
				ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

				SetEnvironmentCubemapID(contentPayload.id);
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Color LUT drag & drop target
	{
		std::vector<std::string> lutNames;
		_content->GetTextureNames(&lutNames);

		ImGui::BeginGroup();
		ImGui::Text("Color LUT:");
		ImGui::SameLine();

		ImGui::SameLine();
		if (ImGui::BeginCombo("##LUTTextureCombo", _colorLutID == CONTENT_NULL ? "None" : (lutNames[_colorLutID].c_str())))
		{
			// Add "None" option
			{
				const bool isNoneSelected = (_colorLutID == CONTENT_NULL);
				if (ImGui::Selectable("None", isNoneSelected))
					_colorLutID = CONTENT_NULL;

				if (isNoneSelected)
					ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < lutNames.size(); i++)
			{
				std::string &lutName = lutNames[i];

				if (_content->GetTexture(i)->GetDim() != TexDim::Tex3D)
					continue; // Only show LUT textures

				const bool isSelected = (_colorLutID == i);
				if (ImGui::Selectable(lutName.c_str(), isSelected))
					_colorLutID = i;

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::EndGroup();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::TEXTURE)))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
				ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

				_colorLutID = contentPayload.id;
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Set ambient color
	ImGui::ColorEdit3("Ambient Color", &(_currAmbientColor.x), ImGuiColorEditFlags_NoInputs);

	if (_skyboxBuffer)
	{
		ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR;
		ImGui::ColorEdit4("Skybox Color", &(_currSkyboxColor.x), flags);
	}

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Toggles");

	bool useVsync = (_vSync != 0);
	if (ImGui::Checkbox("V-Sync", &useVsync))
	{
		_vSync = useVsync ? 1 : 0;
	}

	if (useVsync)
	{
		ImGui::SameLine();
		int interval = (int)_vSync;
		if (ImGui::InputInt("##VSyncInterval", &interval, 1))
		{
			_vSync = (uint8_t)std::clamp(interval, 1, 4);
		}
	}

	std::string wireframeModes[] = { "Geometry", "Geometry + Wireframe", "Wireframe"};
	if (ImGui::Button((wireframeModes[_wireframeMode] + "##WireframeModeButton").c_str()))
	{
		_wireframeMode++;
		if (_wireframeMode > 2)
			_wireframeMode = 0;
	}

	if (_wireframeMode > 0)
	{
		ImGui::SameLine();
		if (ImGui::ColorEdit4("Color##WireframeColor", &_wireframeColor.x, ImGuiColorEditFlags_NoInputs))
		{
			if (!_wireframeColorBuffer.UpdateBuffer(_context, &_wireframeColor))
			{
				ErrMsg("Failed to update wireframe color buffer!");
				return false;
			}
		}
	}

	ImGui::Checkbox("Transparency", &_renderTransparency);

	ImGui::Checkbox("Overlay", &_renderOverlay);

	ImGui::Checkbox("Debug Drawing", &_renderDebugDraw);

	if (ImGui::Checkbox("Post Processing", &_renderPostFX))
	{
		if (!_renderPostFX)
		{
			// Clear post processing resources
			constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			_context->ClearRenderTargetView(_blurRT.GetRTV(0), clearColor);
			_context->ClearRenderTargetView(_fogRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_outlineRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_cocRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_dofSharpRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_dofHalfBlur1RT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_dofHalfBlur2RT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_dofFullBlurRT.GetRTV(), clearColor);
		}
	}

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Properties");

	if (_renderPostFX)
	{
		if (ImGui::TreeNode("Post Processing Settings"))
		{
			if (ImGui::TreeNode("Volumetric Fog"))
			{
				ImGui::Text(std::format("Resolution: {}x{}", (int)_viewportFog.Width, (int)_viewportFog.Height).c_str());
				if (ImGui::DragFloat("Scale##FogScale", &_fogResolutionScale, 0.005f, 0.01f, 1.0f))
				{
					_fogResolutionScale = std::clamp(_fogResolutionScale, 0.01f, 1.0f);
					DebugData::Get().graphicsFogScale = _fogResolutionScale;

					if (!RefreshFogBuffers())
					{
						ErrMsg("Failed to refresh fog buffers!");
						return false;
					}
				}

				if (ImGui::Checkbox("Render Fog", &_renderFogFX))
				{
					if (!_renderFogFX)
					{
						// Clear fog resources
						constexpr float clearFog[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_fogRT.GetRTV(), clearFog);
					}

					DebugData::Get().graphicsFogEnabled = _renderFogFX;
				}

				if (_renderFogFX)
				{
					ImGui::PushID("FogSettings");

					ImGui::DragFloat("Thickness", &_currFogSettings.thickness, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Sample Bias", &_currFogSettings.sampleBias, 0.01f, 0.01f, 100.0f);
					ImGuiUtils::LockMouseOnActive();

					int maxSteps = _currFogSettings.maxSteps;
					if (ImGui::DragInt("Max Samples", &maxSteps, 1))
						_currFogSettings.maxSteps = MAX(maxSteps, 0);
					ImGuiUtils::LockMouseOnActive();

					float farPlane = _currViewCamera->GetPlanes().farZ;

					ImGui::DragFloat("Depth Fade Begin", &_currFogSettings.depthFadeBegin, 0.001f, 0.001f, 1.0f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Depth Fade End", &_currFogSettings.depthFadeEnd, 0.001f, 0.001f, 1.0f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Depth Fade Exponent", &_currFogSettings.depthFadeExp, 0.001f, 0.001f);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::DragInt("Blur Iterations", &_fogBlurIterations, 0.01f, 0, 16))
					{
						constexpr float clearFog[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_fogRT.GetRTV(), clearFog);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _fogGaussWeights;
						bool modified = false;

						static float sigma = 0.0f;
						if (ImGui::Button("Apply Gaussian"))
						{
							CalcGaussianWeights(gaussWeights.data(), gaussWeights.size(), sigma);
							modified = true;
						}

						ImGui::SameLine();
						ImGui::Text("Sigma:");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::DragFloat("##Sigma", &sigma, 0.01f);
						ImGuiUtils::LockMouseOnActive();

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = MAX(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = MAX(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j] * (j == 0 ? 1.0f : 2.0f);
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetFogGaussianWeightsBuffer(gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Emission"))
			{
				ImGui::Text(std::format("Resolution: {}x{}", (int)_viewportBlur.Width, (int)_viewportBlur.Height).c_str());
				if (ImGui::DragFloat("Scale##EmissionScale", &_emissionResolutionScale, 0.005f, 0.01f, 1.0f))
				{
					_emissionResolutionScale = std::clamp(_emissionResolutionScale, 0.01f, 1.0f);
					DebugData::Get().graphicsEmissionScale = _emissionResolutionScale;

					if (!RefreshEmissionBuffers())
					{
						ErrMsg("Failed to refresh emission buffers!");
						return false;
					}
				}

				if (ImGui::Checkbox("Render Emission", &_renderEmissionFX))
				{
					if (!_renderEmissionFX)
					{
						// Clear emission resources
						constexpr float clearBlur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_blurRT.GetRTV(0), clearBlur);
					}

					DebugData::Get().graphicsEmissionEnabled = _renderEmissionFX;
				}

				if (_renderEmissionFX)
				{
					ImGui::PushID("EmissionSettings");

					ImGui::DragFloat("Strength", &_currEmissionSettings.strength, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Exponent", &_currEmissionSettings.exponent, 0.005f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Threshold", &_currEmissionSettings.threshold, 0.005f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("White Bias", &_currEmissionSettings.whiteBias, 0.005f);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::DragInt("Blur Iterations", &_emissionBlurIterations, 0.1f, 0, 16))
					{
						constexpr float clearBlur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_blurRT.GetRTV(0), clearBlur);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _emissionGaussWeights;
						bool modified = false;

						static float sigma = 0.0f;
						if (ImGui::Button("Apply Gaussian"))
						{
							CalcGaussianWeights(gaussWeights.data(), gaussWeights.size(), sigma);
							modified = true;
						}

						ImGui::SameLine();
						ImGui::Text("Sigma:");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::DragFloat("##Sigma", &sigma, 0.01f);
						ImGuiUtils::LockMouseOnActive();

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = MAX(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = MAX(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j] * (j == 0 ? 1.0f : 2.0f);
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetEmissionGaussianWeightsBuffer(gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Depth of Field"))
			{
				ImGui::Text(std::format("Resolution: {}x{}", (int)_viewportDof.Width, (int)_viewportDof.Height).c_str());
				if (ImGui::DragFloat("Scale##DoFScale", &_dofResolutionScale, 0.005f, 0.01f, 1.0f))
				{
					_dofResolutionScale = std::clamp(_dofResolutionScale, 0.01f, 1.0f);
					DebugData::Get().graphicsDofScale = _dofResolutionScale;

					if (!RefreshDofBuffers())
					{
						ErrMsg("Failed to refresh DoF buffers!");
						return false;
					}
				}

				if (ImGui::Checkbox("Render Depth of Field", &_renderDepthOfFieldFX))
				{
					if (!_renderDepthOfFieldFX)
					{
						// Clear dof resources
						constexpr float clearBlur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_cocRT.GetRTV(), clearBlur);
						_context->ClearRenderTargetView(_dofSharpRT.GetRTV(), clearBlur);
						_context->ClearRenderTargetView(_dofHalfBlur1RT.GetRTV(), clearBlur);
						_context->ClearRenderTargetView(_dofHalfBlur2RT.GetRTV(), clearBlur);
						_context->ClearRenderTargetView(_dofFullBlurRT.GetRTV(), clearBlur);
					}

					DebugData::Get().graphicsDofEnabled = _renderDepthOfFieldFX;
				}

				if (_renderDepthOfFieldFX)
				{
					ImGui::PushID("Depth of Field Settings");

					ImGui::DragFloat("Focal Plane", &_currDepthOfFieldSettings.focalPlane, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Aperture", &_currDepthOfFieldSettings.aperture, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("imageDistance", &_currDepthOfFieldSettings.imageDistance, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _dofGaussWeights;
						bool modified = false;

						static float sigma = 0.0f;
						if (ImGui::Button("Apply Gaussian"))
						{
							CalcGaussianWeights(gaussWeights.data(), gaussWeights.size(), sigma);
							modified = true;
						}

						ImGui::SameLine();
						ImGui::Text("Sigma:");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::DragFloat("##Sigma", &sigma, 0.01f);
						ImGuiUtils::LockMouseOnActive();

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = MAX(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = MAX(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j] * (j == 0 ? 1.0f : 2.0f);
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetDofGaussianWeightsBuffer(gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}


					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Outline"))
			{
				ImGui::Text(std::format("Resolution: {}x{}", (int)_viewportOutline.Width, (int)_viewportOutline.Height).c_str());
				if (ImGui::DragFloat("Scale##OutlineScale", &_outlineResolutionScale, 0.005f, 0.01f, 1.0f))
				{
					_outlineResolutionScale = std::clamp(_outlineResolutionScale, 0.01f, 1.0f);
					DebugData::Get().graphicsOutlineScale = _outlineResolutionScale;

					if (!RefreshOutlineBuffers())
					{
						ErrMsg("Failed to refresh outline buffers!");
						return false;
					}
				}

				if (ImGui::Checkbox("Render Outline", &_renderOutlineFX))
				{
					if (!_renderOutlineFX)
					{
						// Clear outline resources
						constexpr float clearOutline = 0.0f;
						_context->ClearRenderTargetView(_outlineRT.GetRTV(), &clearOutline);
					}

					DebugData::Get().graphicsOutlineEnabled = _renderOutlineFX;
				}

				if (_renderOutlineFX)
				{
					ImGui::PushID("OutlineSettings");

					ImGui::ColorEdit4("Color", &_outlineSettings.color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);

					ImGui::DragFloat("Strength", &_outlineSettings.strength, 0.001f, 0.0001f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Exponent", &_outlineSettings.exponent, 0.001f, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::SliderFloat("Smoothing", &_outlineSettings.smoothing, 0.0f, 1.0f);

					if (ImGui::DragInt("Blur Iterations", &_outlineBlurIterations, 0.1f, 0, 16))
					{
						constexpr float clearOutline[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_outlineRT.GetRTV(), clearOutline);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _outlineGaussWeights;
						bool modified = false;

						static float sigma = 0.0f;
						if (ImGui::Button("Apply Gaussian"))
						{
							CalcGaussianWeights(gaussWeights.data(), gaussWeights.size(), sigma);
							modified = true;
						}

						ImGui::SameLine();
						ImGui::Text("Sigma:");
						ImGui::SameLine();
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						ImGui::DragFloat("##Sigma", &sigma, 0.01f);
						ImGuiUtils::LockMouseOnActive();

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = MAX(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = MAX(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j] * (j == 0 ? 1.0f : 2.0f);
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (int i = 0; i < gaussWeights.size(); i++)
									sum += gaussWeights[i] * (i == 0 ? 1.0f : 2.0f);

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetGaussianWeightsBuffer(&_outlineGaussianWeightsBuffer, gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}

	if (ImGui::TreeNode("Render Distance Fog"))
	{
		ImGui::ColorEdit4("##RenderDistanceFogColor", &_generalDataSettings.fadeoutColor.x, ImGuiColorEditFlags_AlphaBar);

		ImGui::SliderFloat("Begin Depth##RenderDistanceFogBeginDepth", &_generalDataSettings.fadeoutDepthBegin, 0.0f, 1.0f);

		if (ImGui::DragFloat("Exponent##RenderDistanceFogExponent", &_generalDataSettings.fadeoutExponent, 0.01f))
			_generalDataSettings.fadeoutExponent = MAX(_generalDataSettings.fadeoutExponent, 0.01f);
		ImGuiUtils::LockMouseOnActive();

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Shadow Rasterization"))
	{
		bool hasChanged = false;

		int fillMode = (int)_shadowRasterizerDesc.FillMode - 2;
		int cullMode = (int)_shadowRasterizerDesc.CullMode - 1;
		bool frontCounterClockwise = _shadowRasterizerDesc.FrontCounterClockwise;
		int depthBias = _shadowRasterizerDesc.DepthBias;
		float depthBiasClamp = _shadowRasterizerDesc.DepthBiasClamp;
		float slopeScaledDepthBias = _shadowRasterizerDesc.SlopeScaledDepthBias;
		bool depthClipEnable = _shadowRasterizerDesc.DepthClipEnable;
		bool scissorEnable = _shadowRasterizerDesc.ScissorEnable;
		bool multisampleEnable = _shadowRasterizerDesc.MultisampleEnable;
		bool antialiasedLineEnable = _shadowRasterizerDesc.AntialiasedLineEnable;

		if (ImGui::Combo("Fill Mode", &fillMode, "Wireframe\0Solid\0"))
		{
			_shadowRasterizerDesc.FillMode = (D3D11_FILL_MODE)(fillMode + 2);
			hasChanged = true;
		}

		if (ImGui::Combo("Cull Mode", &cullMode, "None\0Front\0Back\0"))
		{
			_shadowRasterizerDesc.CullMode = (D3D11_CULL_MODE)(cullMode + 1);
			hasChanged = true;
		}

		if (ImGui::Checkbox("Front Counter Clockwise", &frontCounterClockwise))
		{
			_shadowRasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
			hasChanged = true;
		}

		if (ImGui::DragInt("Depth Bias", &depthBias, 0.01f))
		{
			_shadowRasterizerDesc.DepthBias = depthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Depth Bias Clamp", &depthBiasClamp, 0.001f))
		{
			_shadowRasterizerDesc.DepthBiasClamp = depthBiasClamp;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Slope Scaled Depth Bias", &slopeScaledDepthBias, 0.01f))
		{
			_shadowRasterizerDesc.SlopeScaledDepthBias = slopeScaledDepthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Checkbox("Depth Clip", &depthClipEnable))
		{
			_shadowRasterizerDesc.DepthClipEnable = depthClipEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Scissor", &scissorEnable))
		{
			_shadowRasterizerDesc.ScissorEnable = scissorEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Multisample", &multisampleEnable))
		{
			_shadowRasterizerDesc.MultisampleEnable = multisampleEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Antialiased Line", &antialiasedLineEnable))
		{
			_shadowRasterizerDesc.AntialiasedLineEnable = antialiasedLineEnable;
			hasChanged = true;
		}

		ImGui::Dummy({ 0.0f, 6.0f });

		static bool applyContinuously = false;
		ImGui::Checkbox("Apply Continuously", &applyContinuously);

		bool applyPreset = false;
		if (!applyContinuously)
		{
			if (ImGui::Button("Apply Preset"))
				applyPreset = true;
		}

		static bool invalidPreset = false;
		if ((applyContinuously && hasChanged) || applyPreset)
		{
			ID3D11RasterizerState *tempRasterizer;

			invalidPreset = FAILED(_device->CreateRasterizerState(&_shadowRasterizerDesc, &tempRasterizer));

			if (!invalidPreset)
			{
				_shadowRasterizer.Reset();
				_shadowRasterizer = tempRasterizer;
			}
		}

		if (invalidPreset)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Invalid Preset!");
			ImGui::PopStyleColor();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Wireframe Rasterization"))
	{
		bool hasChanged = false;

		int fillMode = (int)_wireframeRasterizerDesc.FillMode - 2;
		int cullMode = (int)_wireframeRasterizerDesc.CullMode - 1;
		bool frontCounterClockwise = _wireframeRasterizerDesc.FrontCounterClockwise;
		int depthBias = _wireframeRasterizerDesc.DepthBias;
		float depthBiasClamp = _wireframeRasterizerDesc.DepthBiasClamp;
		float slopeScaledDepthBias = _wireframeRasterizerDesc.SlopeScaledDepthBias;
		bool depthClipEnable = _wireframeRasterizerDesc.DepthClipEnable;
		bool scissorEnable = _wireframeRasterizerDesc.ScissorEnable;
		bool multisampleEnable = _wireframeRasterizerDesc.MultisampleEnable;
		bool antialiasedLineEnable = _wireframeRasterizerDesc.AntialiasedLineEnable;

		if (ImGui::Combo("Fill Mode", &fillMode, "Wireframe\0Solid\0"))
		{
			_wireframeRasterizerDesc.FillMode = (D3D11_FILL_MODE)(fillMode + 2);
			hasChanged = true;
		}

		if (ImGui::Combo("Cull Mode", &cullMode, "None\0Front\0Back\0"))
		{
			_wireframeRasterizerDesc.CullMode = (D3D11_CULL_MODE)(cullMode + 1);
			hasChanged = true;
		}

		if (ImGui::Checkbox("Front Counter Clockwise", &frontCounterClockwise))
		{
			_wireframeRasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
			hasChanged = true;
		}

		if (ImGui::DragInt("Depth Bias", &depthBias, 0.01f))
		{
			_wireframeRasterizerDesc.DepthBias = depthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Depth Bias Clamp", &depthBiasClamp, 0.001f))
		{
			_wireframeRasterizerDesc.DepthBiasClamp = depthBiasClamp;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Slope Scaled Depth Bias", &slopeScaledDepthBias, 0.01f))
		{
			_wireframeRasterizerDesc.SlopeScaledDepthBias = slopeScaledDepthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Checkbox("Depth Clip", &depthClipEnable))
		{
			_wireframeRasterizerDesc.DepthClipEnable = depthClipEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Scissor", &scissorEnable))
		{
			_wireframeRasterizerDesc.ScissorEnable = scissorEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Multisample", &multisampleEnable))
		{
			_wireframeRasterizerDesc.MultisampleEnable = multisampleEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Antialiased Line", &antialiasedLineEnable))
		{
			_wireframeRasterizerDesc.AntialiasedLineEnable = antialiasedLineEnable;
			hasChanged = true;
		}

		ImGui::Dummy({ 0.0f, 6.0f });

		static bool applyContinuously = false;
		ImGui::Checkbox("Apply Continuously", &applyContinuously);

		bool applyPreset = false;
		if (!applyContinuously)
		{
			if (ImGui::Button("Apply Preset"))
				applyPreset = true;
		}

		static bool invalidPreset = false;
		if ((applyContinuously && hasChanged) || applyPreset)
		{
			ID3D11RasterizerState *tempRasterizer;

			invalidPreset = FAILED(_device->CreateRasterizerState(&_wireframeRasterizerDesc, &tempRasterizer));

			if (!invalidPreset)
			{
				_wireframeOverlayRasterizer.Reset();
				_wireframeOverlayRasterizer = tempRasterizer;
			}
		}

		if (invalidPreset)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Invalid Preset!");
			ImGui::PopStyleColor();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Transparency Blending"))
	{
		static std::string blendStateName = "";
		ImGui::InputText("Blend State Name", &blendStateName);

		bool hasChanged = false;

		static bool alphaToCoverageEnable = _transparentBlendDesc.AlphaToCoverageEnable;
		static bool independentBlendEnable = _transparentBlendDesc.IndependentBlendEnable;
		static bool blendEnable = _transparentBlendDesc.RenderTarget[0].BlendEnable;

		static int srcBlend = 4;
		static int destBlend = 1;
		static int blendOp = 0;

		static int srcBlendAlpha = 4;
		static int destBlendAlpha = 5;
		static int blendOpAlpha = 4;

		static int renderTargetWriteMask = _transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask;

		constexpr D3D11_BLEND blendValues[] = {
			D3D11_BLEND_ZERO,
			D3D11_BLEND_ONE,
			D3D11_BLEND_SRC_COLOR,
			D3D11_BLEND_INV_SRC_COLOR,
			D3D11_BLEND_SRC_ALPHA,
			D3D11_BLEND_INV_SRC_ALPHA,
			D3D11_BLEND_DEST_ALPHA,
			D3D11_BLEND_INV_DEST_ALPHA,
			D3D11_BLEND_DEST_COLOR,
			D3D11_BLEND_INV_DEST_COLOR,
			D3D11_BLEND_SRC_ALPHA_SAT,
			D3D11_BLEND_BLEND_FACTOR,
			D3D11_BLEND_INV_BLEND_FACTOR,
			D3D11_BLEND_SRC1_COLOR,
			D3D11_BLEND_INV_SRC1_COLOR,
			D3D11_BLEND_SRC1_ALPHA,
			D3D11_BLEND_INV_SRC1_ALPHA
		};
		constexpr char blendNames[] = "ZERO\0ONE\0SRC_COLOR\0INV_SRC_COLOR\0SRC_ALPHA\0INV_SRC_ALPHA\0DEST_ALPHA\0INV_DEST_ALPHA\0DEST_COLOR\0INV_DEST_COLOR\0SRC_ALPHA_SAT\0BLEND_FACTOR\0INV_BLEND_FACTOR\0SRC1_COLOR\0INV_SRC1_COLOR\0SRC1_ALPHA\0INV_SRC1_ALPHA\0";

		constexpr D3D11_BLEND_OP blendOpValues[] = {
			D3D11_BLEND_OP_ADD,
			D3D11_BLEND_OP_SUBTRACT,
			D3D11_BLEND_OP_REV_SUBTRACT,
			D3D11_BLEND_OP_MIN,
			D3D11_BLEND_OP_MAX
		};
		constexpr char blendOpNames[] = "ADD\0SUBTRACT\0REV_SUBTRACT\0MIN\0MAX\0";

		if (ImGui::Checkbox("Alpha to Coverage", &alphaToCoverageEnable))
		{
			_transparentBlendDesc.AlphaToCoverageEnable = alphaToCoverageEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Independent Blend", &independentBlendEnable))
		{
			_transparentBlendDesc.IndependentBlendEnable = independentBlendEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Blend", &blendEnable))
		{
			_transparentBlendDesc.RenderTarget[0].BlendEnable = blendEnable ? 1 : 0;
			hasChanged = true;
		}

		if (ImGui::Combo("Source Blend", &srcBlend, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].SrcBlend = blendValues[srcBlend];
			hasChanged = true;
		}

		if (ImGui::Combo("Destination Blend", &destBlend, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].DestBlend = blendValues[destBlend];
			hasChanged = true;
		}

		if (ImGui::Combo("Blend Operation", &blendOp, blendOpNames))
		{
			_transparentBlendDesc.RenderTarget[0].BlendOp = blendOpValues[blendOp];
			hasChanged = true;
		}

		if (ImGui::Combo("Source Blend Alpha", &srcBlendAlpha, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].SrcBlendAlpha = blendValues[srcBlendAlpha];
			hasChanged = true;
		}

		if (ImGui::Combo("Destination Blend Alpha", &destBlendAlpha, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].DestBlendAlpha = blendValues[destBlendAlpha];
			hasChanged = true;
		}

		if (ImGui::Combo("Blend Operation Alpha", &blendOpAlpha, blendOpNames))
		{
			_transparentBlendDesc.RenderTarget[0].BlendOpAlpha = blendOpValues[blendOpAlpha];
			hasChanged = true;
		}

		bool renderTargetWriteMaskR = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_RED;
		if (ImGui::Checkbox("Write Red", &renderTargetWriteMaskR))
		{
			renderTargetWriteMask = renderTargetWriteMaskR ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_RED :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_RED;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskG = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_GREEN;
		if (ImGui::Checkbox("Write Green", &renderTargetWriteMaskG))
		{
			renderTargetWriteMask = renderTargetWriteMaskG ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_GREEN :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_GREEN;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskB = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_BLUE;
		if (ImGui::Checkbox("Write Blue", &renderTargetWriteMaskB))
		{
			renderTargetWriteMask = renderTargetWriteMaskB ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_BLUE :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_BLUE;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskA = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_ALPHA;
		if (ImGui::Checkbox("Write Alpha", &renderTargetWriteMaskA))
		{
			renderTargetWriteMask = renderTargetWriteMaskA ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_ALPHA :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_ALPHA;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		ImGui::Dummy({ 0.0f, 6.0f });

		bool applyPreset = false;
		if (ImGui::Button("Apply Preset"))
			applyPreset = true;

		static bool invalidPreset = false;
		if (applyPreset)
		{
			auto *blendStatePtr = _content->GetBlendStateAddress(std::format("{}", blendStateName));
			if (blendStatePtr)
			{
				// Replace existing blend state
				if (FAILED(_device->CreateBlendState(&_transparentBlendDesc, blendStatePtr->ReleaseAndGetAddressOf())))
				{
					invalidPreset = true;
				}
				else
				{
					invalidPreset = false;
				}
			}
			else
			{
				// Create new blend state
				UINT ret = _content->AddBlendState(_device, std::format("{}", blendStateName), _transparentBlendDesc);
				invalidPreset = ret == 0;
			}
		}

		if (invalidPreset)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Invalid Preset!");
			ImGui::PopStyleColor();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Light Draw Info"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;
		childFlags |= ImGuiChildFlags_ResizeY;

		ImGui::BeginChild("Entity Hierarchy", ImVec2(0, 300), childFlags);

		ImGui::Text(std::format("Main Draws: {}", _currViewCamera->GetCullCount()).c_str());
		for (UINT i = 0; i < _currSpotLightCollection.Get()->GetNrOfLights(); i++)
		{
			const B_Camera *spotlightCamera = _currSpotLightCollection.Get()->GetLightBehaviour(i)->GetShadowCamera();
			ImGui::Text(std::format("Spotlight #{} Draws: {}", i, spotlightCamera->GetCullCount()).c_str());
		}

		for (UINT i = 0; i < _currLightPointCollection.Get()->GetNrOfLights(); i++)
		{
			const B_CameraCube *pointlightCamera = _currLightPointCollection.Get()->GetLightBehaviour(i)->GetShadowCameraCube();
			ImGui::Text(std::format("Pointlight #{} Draws: {}", i, pointlightCamera->GetCullCount()).c_str());
		}

		ImGui::EndChild();
		ImGui::TreePop();
	}

	return true;
}
bool Graphics::RenderSceneView()
{
	ZoneScopedXC(RandomUniqueColor());

	const float padding = 0.0f;

	Input &input = Input::Instance();
	DebugDrawer &debugDrawer = DebugDrawer::Instance();

	dx::XMINT2 wPos = input.GetWindowPos();
	dx::XMUINT2 wSize = input.GetWindowSize();
	dx::XMUINT2 wRealSize = input.GetRealWindowSize();

	dx::XMINT2 sPos = input.GetScreenPos();
	dx::XMUINT2 sSize = input.GetScreenSize();

	MouseState mState = input.GetMouse();
	dx::XMFLOAT2 localMousePos = input.GetLocalMousePos();

	ImVec2 mPos = ImGui::GetMousePos() - ImVec2(wPos.x, wPos.y);
	mPos /= ImVec2((float)sSize.x, (float)sSize.y);
	mPos *= ImVec2((float)wSize.x, (float)wSize.y);

	ImVec2 displayPos = { (float)sPos.x, (float)sPos.y };
	ImVec2 displaySize = { (float)sSize.x, (float)sSize.y };

	ImVec2 appWindowPos = { (float)wPos.x, (float)wPos.y };
	ImVec2 appWindowSize = { (float)wRealSize.x, (float)wRealSize.y };

	ImVec2 sceneWindowPos = ImGui::GetWindowPos();
	ImVec2 sceneWindowSize = ImGui::GetWindowSize();
	ImVec2 sceneWindowLocalPos = sceneWindowPos - appWindowPos;

	ImVec2 viewRegionMin = ImGui::GetWindowContentRegionMin();
	ImVec2 viewRegionMax = ImGui::GetWindowContentRegionMax();
	ImVec2 viewRegionSize = viewRegionMax - viewRegionMin;

	ImVec2 sceneViewPos, sceneViewSize;

	auto &debugData = DebugData::Get();
	if (debugData.stretchToFitView)
	{
		dx::XMFLOAT2 sceneViewLocalPos = {
			sceneWindowLocalPos.x + viewRegionMin.x + padding,
			sceneWindowLocalPos.y + viewRegionMin.y + padding
		};
		dx::XMFLOAT2 sceneViewLocalSize = {
			sceneWindowLocalPos.x + viewRegionMax.x - padding - sceneViewLocalPos.x,
			sceneWindowLocalPos.y + viewRegionMax.y - padding - sceneViewLocalPos.y
		};

		sceneViewPos = ImGui::GetCursorPos();
		sceneViewSize = viewRegionSize;

		debugData.sceneViewSizeX = sceneViewSize.x;
		debugData.sceneViewSizeY = sceneViewSize.y;

		input.SetSceneViewPos(dx::XMINT2((int)sceneViewLocalPos.x - padding, (int)sceneViewLocalPos.y - padding));
		input.SetSceneViewSize(dx::XMUINT2((uint32_t)(sceneViewLocalSize.x + 2.0f * padding), (uint32_t)(sceneViewLocalSize.y + 2.0f * padding)));
	}
	else
	{
		dx::XMUINT2 srSize = input.GetSceneRenderSize();

		float renderAspect = (float)srSize.x / (float)srSize.y;
		float viewAspect = viewRegionSize.x / viewRegionSize.y;

		ImVec2 scaledBounds = ImVec2(0, 0);
		if (renderAspect > viewAspect)
		{
			scaledBounds.x = viewRegionSize.x;
			scaledBounds.y = viewRegionSize.x / renderAspect;
		}
		else
		{
			scaledBounds.x = viewRegionSize.y * renderAspect;
			scaledBounds.y = viewRegionSize.y;
		}

		ImVec2 viewRegionCenter = (viewRegionMin + viewRegionMax) * 0.5f;
		ImVec2 renderTextureCorner = viewRegionCenter - (scaledBounds * 0.5f);

		sceneViewPos = renderTextureCorner;
		sceneViewSize = scaledBounds;

		input.SetSceneViewPos(dx::XMINT2((int)(sceneWindowLocalPos.x + renderTextureCorner.x), (int)(sceneWindowLocalPos.y + renderTextureCorner.y)));
		input.SetSceneViewSize(dx::XMUINT2((uint32_t)scaledBounds.x, (uint32_t)scaledBounds.y));
	}

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration;
	windowFlags |= ImGuiWindowFlags_NoMove;
	windowFlags |= ImGuiWindowFlags_NoSavedSettings;
	windowFlags |= ImGuiWindowFlags_NoBackground;
	windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::SetCursorPos(sceneViewPos);
	ImGui::BeginChild("SceneViewChild", sceneViewSize, ImGuiChildFlags_None, windowFlags);
	ImGui::SetCursorPos({ 0, 0 });

	if (_sceneSampler)
		ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ImplDX11_SetSampler, _sceneSampler.Get());

	ImGui::Image((ImTextureID)_intermediateRT.GetSRV(), sceneViewSize);

	if (_sceneSampler)
		ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ImplDX11_SetSampler, NULL);

	if (!notifications.empty())
	{
		float dTime = ImGui::GetIO().DeltaTime;
		const float offset = 8.0f;

		ImGui::SetCursorPosY(offset);
		for (int i = 0; i < notifications.size(); i++)
		{
			NotificationMessage &notification = notifications[i];

			ImVec4 severityColor;
			switch (notification.severity)
			{
			default:
			case NotificationMessage::SeverityColor::White:		severityColor = { 1.0f, 1.0f, 1.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Green:		severityColor = { 0.0f, 1.0f, 0.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Yellow:	severityColor = { 1.0f, 1.0f, 0.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Orange:	severityColor = { 1.0f, 0.5f, 0.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Red:		severityColor = { 1.0f, 0.0f, 0.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Blue:		severityColor = { 0.0f, 0.0f, 1.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Magenta:	severityColor = { 1.0f, 0.0f, 1.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Cyan:		severityColor = { 0.0f, 1.0f, 1.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Black:		severityColor = { 0.0f, 0.0f, 0.0f, 1 }; break;
			case NotificationMessage::SeverityColor::Gray:		severityColor = { 0.5f, 0.5f, 0.5f, 1 }; break;

			case NotificationMessage::SeverityColor::Rainbow:
			{
				ImGui::ColorConvertHSVtoRGB(
					std::fmodf(ImGui::GetTime() * 0.3f, 1.0f),	// hue
					0.9f,										// saturation
					0.9f,										// value
					severityColor.x,
					severityColor.y,
					severityColor.z
				);
				severityColor.w = 1.0f;
				break;
			}
			}

			if (notification.blinkFrequency > 0.0f)
			{
				// binary blink
				float phase = fmodf(notification.blinkFrequency * ImGui::GetTime(), 1.0f);
				if (phase > 0.6f)
					severityColor.w = 0.0f;
			}

			ImGuiUtils::BeginFont("DroidSans", notification.fontSize);

			ImGui::SetCursorPosX(offset);

			if (notification.bgAlpha > 0.0f)
			{
				ImVec2 bgPadding = ImVec2(4.0f, 4.0f);

				ImVec2 textSize = ImGui::CalcTextSize(notification.message.c_str());
				ImVec2 bgMin = ImGui::GetCursorScreenPos() - bgPadding;
				ImVec2 bgMax = bgMin + textSize + bgPadding * 2.0f;

				ImGui::GetWindowDrawList()->AddRectFilled(bgMin, bgMax, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, notification.bgAlpha)));
			}

			ImGui::TextColored(severityColor, notification.message.c_str());

			ImGuiUtils::EndFont();

			if (notification.duration >= 0.0f)
			{
				notification.duration -= dTime;
				if (notification.duration <= 0.0f)
				{
					notifications.erase(notifications.begin() + i);
					i--;
				}
			}
		}
	}

#ifdef USE_IMGUIZMO
	ImGuizmo::SetDrawlist(ImGui::GetCurrentWindow()->DrawList);
#endif

	if (ImGui::IsWindowFocused(ImGuiHoveredFlags_None))
		input.SetKeyboardAbsorbed(false);
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None))
		input.SetMouseAbsorbed(false);

	ImGui::EndChild();

	return true;
}

ImGuiID Graphics::GetBackgroundDockID() const
{
	return _backgroundDockID;
}
#endif