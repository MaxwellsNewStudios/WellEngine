#include "stdafx.h"
#include "Graphics.h"
#include "Game/Entity.h"
#include "Game/Behaviours/Rendering/Camera/CameraBehaviour.h"
#include "Game/Behaviours/Rendering/Mesh/MeshBehaviour.h"
#include "Engine/Debug/DebugData.h"
#include "Engine/UI/UILayout.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


#pragma region Getters
float Graphics::GetScreenFadeAmount() const
{
	if (_currViewCamera)
		return _currViewCamera->GetScreenFadeAmount();
	return 0.0f;
}
float Graphics::GetScreenFadeRate() const
{
	if (_currViewCamera)
		return _currViewCamera->GetScreenFadeRate();
	return 0.0f;
}

bool Graphics::GetRenderTransparent() const
{
	return _renderTransparency;
}
bool Graphics::GetRenderOverlay() const
{
	return _renderOverlay;
}
bool Graphics::GetRenderPostFX() const
{
	return _renderPostFX;
}

ID3D11DepthStencilState *Graphics::GetCurrentDepthStencilState()
{
	return _currViewCamera->GetInverted() ? _rdss.Get() : _ndss.Get();
}

FogSettingsBuffer Graphics::GetFogSettings() const
{
	return _currFogSettings;
}
EmissionSettingsBuffer Graphics::GetEmissionSettings() const
{
	return _currEmissionSettings;
}
DepthOfFieldSettingsBuffer Graphics::GetDepthOfFieldSettings() const
{
	return _currDepthOfFieldSettings;
}
dx::XMFLOAT3 Graphics::GetAmbientColor() const
{
	return To3(_currAmbientColor);
}
dx::XMFLOAT4 Graphics::GetSkyboxColor() const
{
	return _currSkyboxColor;
}
UINT Graphics::GetSkyboxShaderID() const
{
	return _skyboxPsID;
}
UINT Graphics::GetEnvironmentCubemapID() const
{
	return _environmentCubemapID;
}
#pragma endregion


#pragma region Setters
bool Graphics::SetCamera(CameraBehaviour *viewCamera)
{
	if (viewCamera == nullptr)
	{
		ErrMsg("Failed to set camera, camera is nullptr!");
		return false;
	}

	_currViewCamera = viewCamera;
	return true;
}
bool Graphics::SetSpotlightCollection(SpotLightCollection *spotlights)
{
	if (spotlights == nullptr)
	{
		ErrMsg("Failed to set spotlight collection, collection is nullptr!");
		return false;
	}

	_currSpotLightCollection = spotlights;
	return true;
}
bool Graphics::SetPointlightCollection(PointLightCollection *pointlights)
{
	if (pointlights == nullptr)
	{
		ErrMsg("Failed to set pointlight collection, collection is nullptr!");
		return false;
	}

	_currPointLightCollection = pointlights;
	return true;
}

void Graphics::SetDistortionOrigin(const dx::XMFLOAT3A &origin)
{
	_distortionSettings.distortionOrigin = origin;
}
void Graphics::SetDistortionStrength(float strength)
{
	_distortionSettings.distortionStrength = strength;
}

void Graphics::SetGaussianWeightsBuffer(StructuredBufferD3D11 *buffer, float *const weights, UINT count)
{
	ZoneScopedC(RandomUniqueColor());

	if (buffer == nullptr || weights == nullptr || count == 0)
	{
		ErrMsg("Failed to set gaussian weights buffer, invalid parameters!");
		return;
	}

	if (!buffer->Initialize(_device, sizeof(float), count, true, false, false, weights))
	{
		ErrMsg("Failed to initialize gaussian weights buffer!");
		return;
	}
}
void Graphics::SetFogGaussianWeightsBuffer(float *const weights, UINT count)
{
	SetGaussianWeightsBuffer(&_fogGaussianWeightsBuffer, weights, count);
}
void Graphics::SetEmissionGaussianWeightsBuffer(float *const weights, UINT count)
{
	SetGaussianWeightsBuffer(&_emissionGaussianWeightsBuffer, weights, count);
}
void Graphics::SetDofGaussianWeightsBuffer(float *const weights, UINT count)
{
	SetGaussianWeightsBuffer(&_dofGaussianWeightsBuffer, weights, count);
}

void Graphics::SetFogSettings(const FogSettingsBuffer &fogSettings)
{
	_currFogSettings = fogSettings;
}
void Graphics::SetEmissionSettings(const EmissionSettingsBuffer &emissionSettings)
{
	_currEmissionSettings = emissionSettings;
}
void Graphics::SetDepthOfFieldSettings(const DepthOfFieldSettingsBuffer &dofSettings)
{
	//_currDepthOfFieldSettings = dofSettings; TODO: Uncomment this to give control to the scene
}
void Graphics::SetAmbientColor(const dx::XMFLOAT3 &color)
{
	_currAmbientColor.x = color.x;
	_currAmbientColor.y = color.y;
	_currAmbientColor.z = color.z;
}
void Graphics::SetSkyboxColor(const dx::XMFLOAT4 &color)
{
	_currSkyboxColor.x = color.x;
	_currSkyboxColor.y = color.y;
	_currSkyboxColor.z = color.z;
	_currSkyboxColor.w = color.w;
}
void Graphics::SetSkyboxShaderID(UINT shaderID)
{
	if (shaderID == CONTENT_NULL)
	{
		_skyboxPsID = CONTENT_NULL;
		return;
	}

	std::string shaderName = _content->GetShaderName(shaderID);
	if (shaderName == "Uninitialized")
	{
		_skyboxPsID = CONTENT_NULL;
		return;
	}

	if (!shaderName.starts_with("PS_Skybox"))
	{
		WarnF("Failed to set skybox shader ID, shader '{}' is not a skybox pixel shader!", shaderName.c_str());
		return;
	}

	_skyboxPsID = shaderID;

	if (shaderName == "PS_SkyboxSolidColor")
	{
		if (!_skyboxBuffer)
			_skyboxBuffer = std::make_unique<ConstantBufferD3D11>();

		if (!_skyboxBuffer->Initialize(_device, sizeof(dx::XMFLOAT4), &_currSkyboxColor))
		{
			ErrMsg("Failed to initialize skybox buffer!");
			return;
		}
	}
	else
	{
		_skyboxBuffer = nullptr;
	}
}
void Graphics::SetEnvironmentCubemapID(UINT cubemapID)
{
	if (cubemapID == CONTENT_NULL)
	{
		_environmentCubemapID = CONTENT_NULL;
		return;
	}

	std::string cubemapName = _content->GetCubemapName(cubemapID);
	if (cubemapName == "Uninitialized")
	{
		_environmentCubemapID = CONTENT_NULL;
		return;
	}

	ShaderResourceTextureD3D11 *cubemapSRT = _content->GetCubemap(cubemapID);
	if (!cubemapSRT->IsCubemap())
		return;

	_environmentCubemapID = cubemapID;
}

#ifdef USE_IMGUI
void Graphics::SetScenePointFiltering(bool state)
{
	D3D11_SAMPLER_DESC desc{};
	desc.Filter = state ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.MipLODBias = 0.f;
	desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	desc.MinLOD = 0.f;
	desc.MaxLOD = 0.f;

	_device->CreateSamplerState(&desc, _sceneSampler.ReleaseAndGetAddressOf());
}
#endif
#pragma endregion


#pragma region Other
void Graphics::BeginScreenFade(float duration)
{
	if (_currViewCamera)
		_currViewCamera->BeginScreenFade(duration);
}
void Graphics::SetScreenFadeManual(float amount)
{
	if (_currViewCamera)
		_currViewCamera->SetScreenFadeManual(amount);
}

void Graphics::ResetLightGrid()
{
	ZoneScoped;
	for (UINT i = 0; i < LIGHT_GRID_RES * LIGHT_GRID_RES; i++)
	{
		_lightGrid[i].spotlightCount = 0;
		_lightGrid[i].pointlightCount = 0;
		_lightGrid[i].simpleSpotlightCount = 0;
		_lightGrid[i].simplePointlightCount = 0;
	}
}
void Graphics::AddLightToTile(UINT tileIndex, UINT lightIndex, LightType type)
{
	ZoneScopedX;
	LightTile &tile = _lightGrid[tileIndex];

	switch (type)
	{
	case SPOTLIGHT:
		if (tile.spotlightCount >= MAX_LIGHTS)
			return;

		tile.spotlights[tile.spotlightCount++] = lightIndex;
		break;

	case POINTLIGHT:
		if (tile.pointlightCount >= MAX_LIGHTS)
			return;

		tile.pointlights[tile.pointlightCount++] = lightIndex;
		break;

	case SIMPLE_SPOTLIGHT:
		if (tile.simpleSpotlightCount >= MAX_LIGHTS)
			return;

		tile.simpleSpotlights[tile.simpleSpotlightCount++] = lightIndex;
		break;

	case SIMPLE_POINTLIGHT:
		if (tile.simplePointlightCount >= MAX_LIGHTS)
			return;

		tile.simplePointlights[tile.simplePointlightCount++] = lightIndex;
		break;
	}
}

#ifdef DEBUG_BUILD
void Graphics::AddOutlinedEntity(Entity *entity)
{
	if (entity == nullptr)
		return;

	std::vector<Entity *> entities;
	entity->GetChildrenRecursive(entities);
	entities.push_back(entity);

	for (int i = 0; i < entities.size(); i++)
	{
		Entity *ent = entities[i];
		// Check if entity is already in the list
		for (const auto &e : _outlinedEntities)
		{
			if (e.Get() != ent)
				continue;

			// Already added
			entities.erase(entities.begin() + i);
			i--;
		}
	}

	for (int i = 0; i < entities.size(); i++)
		_outlinedEntities.emplace_back(*entities[i]);
}
#endif
#pragma endregion
