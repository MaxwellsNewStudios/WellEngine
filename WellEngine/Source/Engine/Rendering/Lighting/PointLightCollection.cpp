#include "stdafx.h"
#include "Rendering/Lighting/PointLightCollection.h"
#include <DirectXCollision.h>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool PointLightCollection::Initialize(ID3D11Device *device, UINT resolution)
{
	_texRes = resolution;
	return true;
}

bool PointLightCollection::UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context)
{
	const UINT lightCount = GetNrOfLights();
	std::vector<PointLightBufferData> lightBufferVec;
	lightBufferVec.reserve(lightCount);

	for (UINT i = 0; i < lightCount; i++)
	{
		PointLightBehaviour *lightBehaviour = _lights[i].lightBehaviour;
		const PointLightBufferData lightBufferData = lightBehaviour->GetLightBufferData();
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
			if (!_shadowCollection.Initialize(device, 
				1, 1, SHADOWCUBE_DEPTH_BUFFER_FORMAT, true, 1, true))
			{
				ErrMsg("Failed to initialize shadow collection!");
				return false;
			}

			PointLightBufferData emptyLightBuffer;
			emptyLightBuffer = { };
			emptyLightBuffer.falloff = 1.0f;
			emptyLightBuffer.fogStrength = 1.0f;

			if (!_lightBufferCollection.Initialize(
				device, sizeof(PointLightBufferData), 1,
				true, false, true, &emptyLightBuffer))
			{
				ErrMsg("Failed to initialize empty pointlight buffer collection!");
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
			if (!_shadowCollection.Initialize(
				device, _texRes, _texRes, 
				SHADOWCUBE_DEPTH_BUFFER_FORMAT,
				true, lightCount, true))
			{
				ErrMsg("Failed to initialize shadow collection!");
				return false;
			}

			if (!_lightBufferCollection.Initialize(device, 
				sizeof(PointLightBufferData), lightCount,
				true, false, true, lightBufferVec.data()))
			{
				ErrMsg("Failed to initialize pointlight buffer collection!");
				return false;
			}

			_shadowViewport = { };
			_shadowViewport.TopLeftX = 0;
			_shadowViewport.TopLeftY = 0;
			_shadowViewport.Width = (float)_texRes;
			_shadowViewport.Height = (float)_texRes;
			_shadowViewport.MinDepth = 0.0f;
			_shadowViewport.MaxDepth = 1.0f;
		}
	}

	for (UINT i = 0; i < lightCount; i++)
	{
		if (!GetLightEnabled(i))
			continue;

		PointLightBehaviour *lightBehaviour = _lights[i].lightBehaviour;

		if (hasResized)
			lightBehaviour->SetUpdateTimer(0);
		else if (!lightBehaviour->DoUpdate())
			continue;

		if (!lightBehaviour->UpdateBuffers())
		{
			ErrMsgF("Failed to update pointlight #{} buffers!", i);
			return false;
		}
	}

	if (lightCount > 0)
	{
		if (!_lightBufferCollection.UpdateBuffer(context, lightBufferVec.data()))
		{
			ErrMsg("Failed to update light buffer!");
			return false;
		}
	}
	
	const UINT simpleLightCount = GetNrOfSimpleLights();
	std::vector<SimplePointLightBufferData> simpleLightBufferVec;
	simpleLightBufferVec.reserve(simpleLightCount);

	for (UINT i = 0; i < simpleLightCount; i++)
	{
		const SimplePointLightBehaviour *lightBehaviour = _simpleLights[i].lightBehaviour;
		const SimplePointLightBufferData lightBufferData = lightBehaviour->GetLightBufferData();
		simpleLightBufferVec.emplace_back(lightBufferData);
	}

	if (_isDirty)
	{
		_simpleLightBufferCollection.Reset();

		if (simpleLightCount <= 0) // Create empty simple light
		{
			SimplePointLightBufferData emptyLightBuffer;
			emptyLightBuffer = { };
			emptyLightBuffer.falloff = 1.0f;
			emptyLightBuffer.fogStrength = 1.0f;

			if (!_simpleLightBufferCollection.Initialize(device, sizeof(SimplePointLightBufferData), 1,
				true, false, true, &emptyLightBuffer))
			{
				ErrMsg("Failed to initialize empty simple pointlight buffer collection!");
				return false;
			}
		}
		else
		{
			if (!_simpleLightBufferCollection.Initialize(device, sizeof(SimplePointLightBufferData), simpleLightCount,
				true, false, true, simpleLightBufferVec.data()))
			{
				ErrMsg("Failed to initialize simple pointlight buffer collection!");
				return false;
			}
		}
	}

	if (simpleLightCount > 0)
	{
		if (!_simpleLightBufferCollection.UpdateBuffer(context, simpleLightBufferVec.data()))
		{
			ErrMsg("Failed to update simple light buffer!");
			return false;
		}
	}

	_isDirty = false;
	return true;
}

bool PointLightCollection::BindCSBuffers(ID3D11DeviceContext *context) const
{
	ID3D11ShaderResourceView *const lightBufferSRV = _lightBufferCollection.GetSRV();
	context->CSSetShaderResources(6, 1, &lightBufferSRV);

	ID3D11ShaderResourceView *const shadowMapSRV = _shadowCollection.GetSRV();
	context->CSSetShaderResources(7, 1, &shadowMapSRV);
	
	ID3D11ShaderResourceView *const simpleLightBufferSRV = _simpleLightBufferCollection.GetSRV();
	context->CSSetShaderResources(13, 1, &simpleLightBufferSRV);

	return true;
}
bool PointLightCollection::BindPSBuffers(ID3D11DeviceContext *context) const
{
	ID3D11ShaderResourceView *const lightBufferSRV = _lightBufferCollection.GetSRV();
	context->PSSetShaderResources(6, 1, &lightBufferSRV);

	ID3D11ShaderResourceView *const shadowMapSRV = _shadowCollection.GetSRV();
	context->PSSetShaderResources(7, 1, &shadowMapSRV);

	ID3D11ShaderResourceView *const simpleLightBufferSRV = _simpleLightBufferCollection.GetSRV();
	context->PSSetShaderResources(13, 1, &simpleLightBufferSRV);

	return true;
}
bool PointLightCollection::UnbindCSBuffers(ID3D11DeviceContext *context) const
{
	constexpr ID3D11ShaderResourceView *const nullSRV[2] = { nullptr, nullptr };
	context->CSSetShaderResources(6, 2, nullSRV);
	context->CSSetShaderResources(13, 1, nullSRV);

	return true;
}
bool PointLightCollection::UnbindPSBuffers(ID3D11DeviceContext *context) const
{
	constexpr ID3D11ShaderResourceView *const nullSRV[2] = { nullptr, nullptr };
	context->PSSetShaderResources(6, 2, nullSRV);
	context->PSSetShaderResources(13, 1, nullSRV);

	return true;
}

UINT PointLightCollection::GetNrOfLights() const
{
	return static_cast<UINT>(_lights.size());
}
UINT PointLightCollection::GetNrOfSimpleLights() const
{
	return static_cast<UINT>(_simpleLights.size());
}
PointLightBehaviour *PointLightCollection::GetLightBehaviour(UINT lightIndex) const
{
	if (lightIndex >= _lights.size())
	{
		ErrMsg("Failed to get pointlight behaviour, index out of bounds!");
		return nullptr;
	}

	return _lights[lightIndex].lightBehaviour;
}
SimplePointLightBehaviour *PointLightCollection::GetSimpleLightBehaviour(UINT lightIndex) const
{
	if (lightIndex >= _simpleLights.size())
	{
		ErrMsg("Failed to get simple pointlight behaviour, index out of bounds!");
		return nullptr;
	}

	return _simpleLights[lightIndex].lightBehaviour;
}
ID3D11DepthStencilView *PointLightCollection::GetShadowMapDSV(UINT lightIndex) const
{
	return _shadowCollection.GetDSV(lightIndex);
}
ID3D11ShaderResourceView *PointLightCollection::GetShadowCubemapsSRV() const
{
	return _shadowCollection.GetSRV();
}
ID3D11ShaderResourceView *PointLightCollection::GetLightBufferSRV() const
{
	return _lightBufferCollection.GetSRV();
}
ID3D11ShaderResourceView *PointLightCollection::GetSimpleLightBufferSRV() const
{
	return _simpleLightBufferCollection.GetSRV();
}
const D3D11_VIEWPORT &PointLightCollection::GetViewport() const
{
	return _shadowViewport;
}

UINT PointLightCollection::GetShadowResolution() const
{
	return _texRes;
}
void PointLightCollection::SetShadowResolution(UINT resolution)
{
	_texRes = resolution;
	_isDirty = true; // Mark as dirty to recreate buffers
}

bool PointLightCollection::GetLightEnabled(UINT lightIndex) const
{
	if (lightIndex < 0)
		return false;

	if (lightIndex >= _lights.size())
		return false;

	return _lights[lightIndex].isEnabled;
}
bool PointLightCollection::GetSimpleLightEnabled(UINT lightIndex) const
{
	if (lightIndex < 0)
		return false;
	
	if (lightIndex >= _simpleLights.size())
		return false;

	return _simpleLights[lightIndex].isEnabled;
}
void PointLightCollection::SetLightEnabled(UINT lightIndex, bool state)
{
	if (lightIndex >= _lights.size())
		return;

	if (GetLightEnabled(lightIndex) == state)
		return;

	_lights[lightIndex].isEnabled = state;
}
void PointLightCollection::SetSimpleLightEnabled(UINT lightIndex, bool state)
{
	if (lightIndex >= _simpleLights.size())
		return;

	if (GetSimpleLightEnabled(lightIndex) == state)
		return;

	_simpleLights[lightIndex].isEnabled = state;
}

bool PointLightCollection::RegisterLight(PointLightBehaviour *light)
{
	if (!light)
	{
		ErrMsg("Failed to register pointlight, light is null!");
		return false;
	}

	for (const PointLightData &lightData : _lights)
	{
		if (lightData.lightBehaviour == light)
		{
			ErrMsg("Failed to register pointlight, light already registered!");
			return false;
		}
	}

	_lights.emplace_back(light, true);

	_isDirty = true;
	return true;
}
bool PointLightCollection::UnregisterLight(PointLightBehaviour *light)
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
		ErrMsg("Failed to unregister pointlight, light not found!");
		return false;
	}

	return UnregisterLight(lightIndex);
}
bool PointLightCollection::UnregisterLight(UINT lightIndex)
{
	if (_lights.size() <= 0)
		return true;

	if (lightIndex >= _lights.size())
	{
		ErrMsg("Failed to unregister pointlight, index out of bounds!");
		return false;
	}

	_lights.erase(_lights.begin() + lightIndex);

	_isDirty = true;
	return true;
}
bool PointLightCollection::RegisterSimpleLight(SimplePointLightBehaviour *light)
{
	if (!light)
	{
		ErrMsg("Failed to register simple pointlight, light is null!");
		return false;
	}

	for (const SimplePointLightData &lightData : _simpleLights)
	{
		if (lightData.lightBehaviour == light)
		{
			ErrMsg("Failed to register simple pointlight, light already registered!");
			return false;
		}
	}

	_simpleLights.emplace_back(light, true);

	_isDirty = true;
	return true;
}
bool PointLightCollection::UnregisterSimpleLight(SimplePointLightBehaviour *light)
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
		ErrMsg("Failed to unregister simple pointlight, light not found!");
		return false;
	}

	return UnregisterSimpleLight(lightIndex);
}
bool PointLightCollection::UnregisterSimpleLight(UINT lightIndex)
{
	if (_simpleLights.size() <= 0)
		return true;

	if (lightIndex >= _simpleLights.size())
	{
		ErrMsg("Failed to unregister simple pointlight, index out of bounds!");
		return false;
	}

	_simpleLights.erase(_simpleLights.begin() + lightIndex);

	_isDirty = true;
	return true;
}
