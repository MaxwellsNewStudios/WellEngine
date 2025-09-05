#pragma once

#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
#include "D3D/StructuredBufferD3D11.h"
#include "D3D/DepthBufferD3D11.h"
#include "Behaviours/CameraBehaviour.h"
#include "Behaviours/PointLightBehaviour.h"
#include "Behaviours/SimplePointLightBehaviour.h"

class PointLightCollection : public IRefTarget<PointLightCollection>
{
private:
	struct SimplePointLightData
	{
		SimplePointLightBehaviour *lightBehaviour = nullptr;
		bool isEnabled = true;
	};
	struct PointLightData
	{
		PointLightBehaviour *lightBehaviour = nullptr;
		bool isEnabled = true;
	};
	
	std::vector<SimplePointLightData> _simpleLights;
	std::vector<PointLightData> _lights;
	UINT _texRes = 0;
	bool _isDirty = true;

	StructuredBufferD3D11 _simpleLightBufferCollection;
	StructuredBufferD3D11 _lightBufferCollection;
	DepthBufferD3D11 _shadowCollection;
	D3D11_VIEWPORT _shadowViewport = { };

public:
	PointLightCollection() = default;
	~PointLightCollection() = default;
	PointLightCollection(const PointLightCollection &other) = delete;
	PointLightCollection &operator=(const PointLightCollection &other) = delete;
	PointLightCollection(PointLightCollection &&other) = delete;
	PointLightCollection &operator=(PointLightCollection &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, UINT resolution);

	[[nodiscard]] bool UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context);

	[[nodiscard]] bool BindCSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool BindPSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool UnbindCSBuffers(ID3D11DeviceContext *context) const;
	[[nodiscard]] bool UnbindPSBuffers(ID3D11DeviceContext *context) const;

	[[nodiscard]] UINT GetNrOfLights() const;
	[[nodiscard]] UINT GetNrOfSimpleLights() const;
	[[nodiscard]] PointLightBehaviour *GetLightBehaviour(UINT lightIndex) const;
	[[nodiscard]] SimplePointLightBehaviour *GetSimpleLightBehaviour(UINT lightIndex) const;
	[[nodiscard]] ID3D11DepthStencilView *GetShadowMapDSV(UINT lightIndex) const;
	[[nodiscard]] ID3D11ShaderResourceView *GetShadowCubemapsSRV() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetLightBufferSRV() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetSimpleLightBufferSRV() const;
	[[nodiscard]] const D3D11_VIEWPORT &GetViewport() const;

	[[nodiscard]] UINT GetShadowResolution() const;
	void SetShadowResolution(UINT resolution);

	[[nodiscard]] bool GetLightEnabled(UINT lightIndex) const;
	[[nodiscard]] bool GetSimpleLightEnabled(UINT lightIndex) const;
	void SetLightEnabled(UINT lightIndex, bool state);
	void SetSimpleLightEnabled(UINT lightIndex, bool state);

	[[nodiscard]] bool RegisterLight(PointLightBehaviour *light);
	[[nodiscard]] bool UnregisterLight(PointLightBehaviour *light);
	[[nodiscard]] bool UnregisterLight(UINT lightIndex);

	[[nodiscard]] bool RegisterSimpleLight(SimplePointLightBehaviour *light);
	[[nodiscard]] bool UnregisterSimpleLight(SimplePointLightBehaviour *light);
	[[nodiscard]] bool UnregisterSimpleLight(UINT lightIndex);

	TESTABLE()
};