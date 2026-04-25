#include "stdafx.h"
#include "LightSpotCollection.h"
//#include "Game/Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool LightSpotCollection::Initialize(ID3D11Device *device, UINT resolution)
{
	_texRes = resolution;
	return true;
}

bool LightSpotCollection::UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context)
{
	const UINT lightCount = GetNrOfLights();
	std::vector<SpotLightBufferData> lightBufferVec;
	lightBufferVec.reserve(lightCount);

	for (UINT i = 0; i < lightCount; i++)
	{
		B_LightSpot *lightBehaviour = _lights[i].lightBehaviour;
		const SpotLightBufferData lightBufferData = lightBehaviour->GetLightBufferData();
		lightBufferVec.emplace_back(lightBufferData);
	}

	bool hasResized = false;
	if (_isDirty)
	{
		hasResized = true;
		_lightBufferCollection.Reset();
		_shadowCollection.Reset();

		if (lightCount <= 0) // Create empty light
		{
			if (!_shadowCollection.Initialize(device, 1, 1, SHADOW_DEPTH_BUFFER_FORMAT, true, 1))
			{
				ErrMsg("Failed to initialize shadow collection!");
				return false;
			}

			SpotLightBufferData emptyLightBuffer = { };
			Store(emptyLightBuffer.vpMatrix, XMMatrixIdentity());
			emptyLightBuffer.falloff = 1.0f;
			emptyLightBuffer.fogStrength = 1.0f;

			if (!_lightBufferCollection.Initialize(device, sizeof(SpotLightBufferData), 1,
				true, false, true, &emptyLightBuffer))
			{
				ErrMsg("Failed to initialize empty spotlight buffer collection!");
				return false;
			}

			_shadowViewport = { };
			_shadowViewport.TopLeftX = 0;
			_shadowViewport.TopLeftY = 0;
			_shadowViewport.Width = 1.0f;
			_shadowViewport.Height = 1.0f;
			_shadowViewport.MinDepth = 0.0f;
			_shadowViewport.MaxDepth = 1.0f;
		}
		else
		{
			if (!_shadowCollection.Initialize(device, _texRes, _texRes, SHADOW_DEPTH_BUFFER_FORMAT, true, lightCount))
			{
				ErrMsg("Failed to initialize shadow collection!");
				return false;
			}

			if (!_lightBufferCollection.Initialize(device, sizeof(SpotLightBufferData), lightCount,
				true, false, true, lightBufferVec.data()))
			{
				ErrMsg("Failed to initialize spotlight buffer collection!");
				return false;
			}

			_shadowViewport = { };
			_shadowViewport.TopLeftX = 0;
			_shadowViewport.TopLeftY = 0;
			_shadowViewport.Width = static_cast<float>(_texRes);
			_shadowViewport.Height = static_cast<float>(_texRes);
			_shadowViewport.MinDepth = 0.0f;
			_shadowViewport.MaxDepth = 1.0f;
		}
	}

	for (UINT i = 0; i < lightCount; i++)
	{
		if (!_lights[i].isEnabled)
			continue;

		B_LightSpot *lightBehaviour = _lights[i].lightBehaviour;

		if (hasResized)
			lightBehaviour->ForceUpdate();
		else if (!lightBehaviour->DoUpdate())
			continue;

		if (!lightBehaviour->UpdateBuffers())
		{
			ErrMsgF("Failed to update spotlight #{} buffers!", i);
			return false;
		}
	}

	if (lightCount > 0)
	{
		if (!_lightBufferCollection.UpdateBuffer(context, lightBufferVec.data()))
		{
			ErrMsg("Failed to update spotlight buffer!");
			return false;
		}
	}
	
	const UINT simpleLightCount = GetNrOfSimpleLights();
	std::vector<SimpleSpotLightBufferData> simpleLightBufferVec;
	simpleLightBufferVec.reserve(simpleLightCount);

	for (UINT i = 0; i < simpleLightCount; i++)
	{
		const B_LightSpotSimple *lightBehaviour = _simpleLights[i].lightBehaviour;
		const SimpleSpotLightBufferData lightBufferData = lightBehaviour->GetLightBufferData();
		simpleLightBufferVec.emplace_back(lightBufferData);
	}

	if (_isDirty)
	{
		_simpleLightBufferCollection.Reset();

		if (simpleLightCount <= 0) // Create empty simple light
		{
			SimpleSpotLightBufferData emptyLightBuffer = { };
			emptyLightBuffer.position = { 0.0f, 0.0f, 0.0f };
			emptyLightBuffer.direction = { 0.0f, 0.0f, 1.0f };
			emptyLightBuffer.color = { 0.0f, 0.0f, 0.0f };
			emptyLightBuffer.angle = 0.0f;
			emptyLightBuffer.falloff = 1.0f;
			emptyLightBuffer.fogStrength = 1.0f;

			if (!_simpleLightBufferCollection.Initialize(device, sizeof(SimpleSpotLightBufferData), 1,
				true, false, true, &emptyLightBuffer))
			{
				ErrMsg("Failed to initialize empty simple spotlight buffer collection!");
				return false;
			}
		}
		else
		{
			if (!_simpleLightBufferCollection.Initialize(device, sizeof(SimpleSpotLightBufferData), simpleLightCount,
				true, false, true, simpleLightBufferVec.data()))
			{
				ErrMsg("Failed to initialize simple spotlight buffer collection!");
				return false;
			}
		}
	}

	if (simpleLightCount > 0)
	{
		if (!_simpleLightBufferCollection.UpdateBuffer(context, simpleLightBufferVec.data()))
		{
			ErrMsg("Failed to update simple spotlight buffer!");
			return false;
		}
	}

	_isDirty = false;
	return true;
}

bool LightSpotCollection::BindCSBuffers(ID3D11DeviceContext *context) const
{
	ID3D11ShaderResourceView *const lightBufferSRV = _lightBufferCollection.GetSRV();
	context->CSSetShaderResources(11, 1, &lightBufferSRV);

	ID3D11ShaderResourceView *const shadowMapSRV = _shadowCollection.GetSRV();
	context->CSSetShaderResources(5, 1, &shadowMapSRV);

	ID3D11ShaderResourceView *const simpleLightBufferSRV = _simpleLightBufferCollection.GetSRV();
	context->CSSetShaderResources(12, 1, &simpleLightBufferSRV);

	return true;
}
bool LightSpotCollection::BindPSBuffers(ID3D11DeviceContext *context) const
{
	ID3D11ShaderResourceView *const lightBufferSRV = _lightBufferCollection.GetSRV();
	context->PSSetShaderResources(11, 1, &lightBufferSRV);

	ID3D11ShaderResourceView *const shadowMapSRV = _shadowCollection.GetSRV();
	context->PSSetShaderResources(5, 1, &shadowMapSRV);

	ID3D11ShaderResourceView *const simpleLightBufferSRV = _simpleLightBufferCollection.GetSRV();
	context->PSSetShaderResources(12, 1, &simpleLightBufferSRV);

	return true;
}
bool LightSpotCollection::UnbindCSBuffers(ID3D11DeviceContext *context) const
{
	constexpr ID3D11ShaderResourceView *const nullSRV[2] = { nullptr, nullptr };
	context->CSSetShaderResources(5, 1, nullSRV);
	context->CSSetShaderResources(11, 2, nullSRV);

	return true;
}
bool LightSpotCollection::UnbindPSBuffers(ID3D11DeviceContext *context) const
{
	constexpr ID3D11ShaderResourceView *const nullSRV[2] = { nullptr, nullptr };
	context->PSSetShaderResources(5, 1, nullSRV);
	context->PSSetShaderResources(11, 2, nullSRV);

	return true;
}

UINT LightSpotCollection::GetNrOfLights() const
{
	return static_cast<UINT>(_lights.size());
}
UINT LightSpotCollection::GetNrOfSimpleLights() const
{
	return static_cast<UINT>(_simpleLights.size());
}
B_LightSpot *LightSpotCollection::GetLightBehaviour(UINT lightIndex) const
{
	if (lightIndex >= _lights.size())
	{
		ErrMsg("Failed to get spotlight behaviour, index out of bounds!");
		return nullptr;
	}

	return _lights[lightIndex].lightBehaviour;
}
B_LightSpotSimple *LightSpotCollection::GetSimpleLightBehaviour(UINT lightIndex) const
{
	if (lightIndex >= _simpleLights.size())
	{
		ErrMsg("Failed to get simple spotlight behaviour, index out of bounds!");
		return nullptr;
	}

	return _simpleLights[lightIndex].lightBehaviour;
}
ID3D11DepthStencilView *LightSpotCollection::GetShadowMapDSV(UINT lightIndex) const
{
	return _shadowCollection.GetDSV(lightIndex);
}
ID3D11ShaderResourceView *LightSpotCollection::GetShadowMapsSRV() const
{
	return _shadowCollection.GetSRV();
}
ID3D11ShaderResourceView *LightSpotCollection::GetLightBufferSRV() const
{
	return _lightBufferCollection.GetSRV();
}
ID3D11ShaderResourceView *LightSpotCollection::GetSimpleLightBufferSRV() const
{
	return _simpleLightBufferCollection.GetSRV();
}
const D3D11_VIEWPORT &LightSpotCollection::GetViewport() const
{
	return _shadowViewport;
}

UINT LightSpotCollection::GetShadowResolution() const
{
	return _texRes;
}
void LightSpotCollection::SetShadowResolution(UINT resolution)
{
	_texRes = resolution;
	_isDirty = true; // Mark as dirty to recreate buffers
}

bool LightSpotCollection::DoUpdate() const
{
	for (const SpotLightData &lightData : _lights)
	{
		if (lightData.lightBehaviour->DoUpdate())
			return true;
	}
	return false;
}

bool LightSpotCollection::GetLightEnabled(UINT lightIndex) const
{
	if (lightIndex < 0)
		return false;
	if (lightIndex >= _lights.size())
		return false;

	return _lights[lightIndex].isEnabled;
}
bool LightSpotCollection::GetSimpleLightEnabled(UINT lightIndex) const
{
	if (lightIndex < 0)
		return false;
	if (lightIndex >= _simpleLights.size())
		return false;

	return _simpleLights[lightIndex].isEnabled;
}
void LightSpotCollection::SetLightEnabled(UINT lightIndex, bool state)
{
	if (lightIndex < 0)
		return;
	if (lightIndex >= _lights.size())
		return;

	_lights[lightIndex].isEnabled = state;
}

void LightSpotCollection::SetSimpleLightEnabled(UINT lightIndex, bool state)
{
	if (lightIndex < 0)
		return;
	if (lightIndex >= _simpleLights.size())
		return;

	_simpleLights[lightIndex].isEnabled = state;
}

bool LightSpotCollection::RegisterLight(B_LightSpot *light)
{
	if (!light)
	{
		ErrMsg("Failed to register spotlight, light is null!");
		return false;
	}

	for (const SpotLightData &lightData : _lights)
	{
		if (lightData.lightBehaviour == light)
		{
			ErrMsg("Failed to register spotlight, light already registered!");
			return false;
		}
	}

	_lights.emplace_back(light, true);

	static int nextOffset = -1;
	nextOffset++;

	light->SetUpdateTimer(nextOffset % (int)(light->GetUpdateFrequency() + 1));
	light->ForceUpdate();

	_isDirty = true;
	return true;
}
bool LightSpotCollection::UnregisterLight(B_LightSpot *light)
{
	if (_lights.size() <= 0)
		return true;

	int lightIndex = -1;
	for (int i = 0; i < _lights.size(); i++)
	{
		if (_lights[i].lightBehaviour == light)
		{
			lightIndex = i;
			break;
		}
	}

	if (lightIndex < 0)
	{
		ErrMsg("Failed to unregister spotlight, light not found!");
		return false;
	}

	return UnregisterLight(lightIndex);
}
bool LightSpotCollection::UnregisterLight(UINT lightIndex)
{
	if (_lights.size() <= 0)
		return true;

	if (lightIndex >= _lights.size())
	{
		ErrMsg("Failed to unregister spotlight, index out of bounds!");
		return false;
	}

	_lights.erase(_lights.begin() + lightIndex);

	_isDirty = true;
	return true;
}
bool LightSpotCollection::RegisterSimpleLight(B_LightSpotSimple *light)
{
	if (!light)
	{
		ErrMsg("Failed to register simple spotlight, light is null!");
		return false;
	}

	for (const SimpleSpotLightData &lightData : _simpleLights)
	{
		if (lightData.lightBehaviour == light)
		{
			ErrMsg("Failed to register spotlight, light already registered!");
			return false;
		}
	}

	_simpleLights.emplace_back(light, true);

	_isDirty = true;
	return true;
}
bool LightSpotCollection::UnregisterSimpleLight(B_LightSpotSimple *light)
{
	if (_simpleLights.size() <= 0)
		return true;

	int lightIndex = -1;
	for (int i = 0; i < _simpleLights.size(); i++)
	{
		if (_simpleLights[i].lightBehaviour == light)
		{
			lightIndex = i;
			break;
		}
	}

	if (lightIndex < 0)
	{
		ErrMsg("Failed to unregister simple spotlight, light not found!");
		return false;
	}

	return UnregisterSimpleLight(lightIndex);
}
bool LightSpotCollection::UnregisterSimpleLight(UINT lightIndex)
{
	if (_simpleLights.size() <= 0)
		return true;

	if (lightIndex >= _simpleLights.size())
	{
		ErrMsg("Failed to unregister simple spotlight, index out of bounds!");
		return false;
	}

	_simpleLights.erase(_simpleLights.begin() + lightIndex);

	_isDirty = true;
	return true;
}
