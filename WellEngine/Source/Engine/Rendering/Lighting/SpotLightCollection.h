#pragma once

#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
#include "D3D/StructuredBufferD3D11.h"
#include "D3D/DepthBufferD3D11.h"
#include "Behaviours/CameraBehaviour.h"
#include "Behaviours/SpotLightBehaviour.h"
#include "Behaviours/SimpleSpotLightBehaviour.h"

class SpotLightCollection : public IRefTarget<SpotLightCollection>
{
private:
	struct SimpleSpotLightData
	{
		SimpleSpotLightBehaviour *lightBehaviour = nullptr;
		bool isEnabled = true;
	};
	struct SpotLightData
	{
		SpotLightBehaviour *lightBehaviour = nullptr;
		bool isEnabled = true;
	};

	std::vector<SpotLightData> _lights;
	std::vector<SimpleSpotLightData> _simpleLights;
	UINT _texRes = 0;
	bool _isDirty = true;

	StructuredBufferD3D11 _simpleLightBufferCollection;
	StructuredBufferD3D11 _lightBufferCollection;
	DepthBufferD3D11 _shadowCollection;
	D3D11_VIEWPORT _shadowViewport = { };

public:
	SpotLightCollection() = default;
	~SpotLightCollection() = default;
	SpotLightCollection(const SpotLightCollection &other) = delete;
	SpotLightCollection &operator=(const SpotLightCollection &other) = delete;
	SpotLightCollection(SpotLightCollection &&other) = delete;
	SpotLightCollection &operator=(SpotLightCollection &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, UINT resolution);

	[[nodiscard]] bool UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context);
	[[nodiscard]] bool BindCSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool BindPSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool UnbindCSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool UnbindPSBuffers(ID3D11DeviceContext *context) const;

	[[nodiscard]] UINT GetNrOfLights() const;
	[[nodiscard]] UINT GetNrOfSimpleLights() const;
	[[nodiscard]] SpotLightBehaviour *GetLightBehaviour(UINT lightIndex) const;
	[[nodiscard]] SimpleSpotLightBehaviour *GetSimpleLightBehaviour(UINT lightIndex) const;
	[[nodiscard]] ID3D11DepthStencilView *GetShadowMapDSV(UINT lightIndex) const;
	[[nodiscard]] ID3D11ShaderResourceView *GetShadowMapsSRV() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetLightBufferSRV() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetSimpleLightBufferSRV() const;
	[[nodiscard]] const D3D11_VIEWPORT &GetViewport() const;

	[[nodiscard]] UINT GetShadowResolution() const;
	void SetShadowResolution(UINT resolution);

	[[nodiscard]] bool GetLightEnabled(UINT lightIndex) const;
	[[nodiscard]] bool GetSimpleLightEnabled(UINT lightIndex) const;
	void SetLightEnabled(UINT lightIndex, bool state);
	void SetSimpleLightEnabled(UINT lightIndex, bool state);

	[[nodiscard]] bool RegisterLight(SpotLightBehaviour *light);
	[[nodiscard]] bool UnregisterLight(SpotLightBehaviour *light);
	[[nodiscard]] bool UnregisterLight(UINT lightIndex);
	[[nodiscard]] bool RegisterSimpleLight(SimpleSpotLightBehaviour *light);
	[[nodiscard]] bool UnregisterSimpleLight(SimpleSpotLightBehaviour *light);
	[[nodiscard]] bool UnregisterSimpleLight(UINT lightIndex);

	TESTABLE()
};