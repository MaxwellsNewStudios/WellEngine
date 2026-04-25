#pragma once

#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

#include "Engine/D3D/StructuredBufferD3D11.h"
#include "Engine/D3D/DepthBufferD3D11.h"
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightPoint.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightPointSimple.h"

namespace WellEngine
{
	class LightPointCollection : public IRefTarget<LightPointCollection>
	{
	private:
		struct SimplePointLightData
		{
			B_LightPointSimple *lightBehaviour = nullptr;
			bool isEnabled = true;
		};
		struct PointLightData
		{
			B_LightPoint *lightBehaviour = nullptr;
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
		LightPointCollection() = default;
		~LightPointCollection() = default;
		LightPointCollection(const LightPointCollection &other) = delete;
		LightPointCollection &operator=(const LightPointCollection &other) = delete;
		LightPointCollection(LightPointCollection &&other) = delete;
		LightPointCollection &operator=(LightPointCollection &&other) = delete;

		[[nodiscard]] bool Initialize(ID3D11Device *device, UINT resolution);

		[[nodiscard]] bool UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context);

		[[nodiscard]] bool BindCSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool BindPSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool UnbindCSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool UnbindPSBuffers(ID3D11DeviceContext *context) const;

		[[nodiscard]] UINT GetNrOfLights() const;
		[[nodiscard]] UINT GetNrOfSimpleLights() const;
		[[nodiscard]] B_LightPoint *GetLightBehaviour(UINT lightIndex) const;
		[[nodiscard]] B_LightPointSimple *GetSimpleLightBehaviour(UINT lightIndex) const;
		[[nodiscard]] ID3D11DepthStencilView *GetShadowMapDSV(UINT lightIndex) const;
		[[nodiscard]] ID3D11ShaderResourceView *GetShadowCubemapsSRV() const;
		[[nodiscard]] ID3D11ShaderResourceView *GetLightBufferSRV() const;
		[[nodiscard]] ID3D11ShaderResourceView *GetSimpleLightBufferSRV() const;
		[[nodiscard]] const D3D11_VIEWPORT &GetViewport() const;

		[[nodiscard]] UINT GetShadowResolution() const;
		void SetShadowResolution(UINT resolution);

		[[nodiscard]] bool DoUpdate() const;

		[[nodiscard]] bool GetLightEnabled(UINT lightIndex) const;
		[[nodiscard]] bool GetSimpleLightEnabled(UINT lightIndex) const;
		void SetLightEnabled(UINT lightIndex, bool state);
		void SetSimpleLightEnabled(UINT lightIndex, bool state);

		[[nodiscard]] bool RegisterLight(B_LightPoint *light);
		[[nodiscard]] bool UnregisterLight(B_LightPoint *light);
		[[nodiscard]] bool UnregisterLight(UINT lightIndex);

		[[nodiscard]] bool RegisterSimpleLight(B_LightPointSimple *light);
		[[nodiscard]] bool UnregisterSimpleLight(B_LightPointSimple *light);
		[[nodiscard]] bool UnregisterSimpleLight(UINT lightIndex);

		TESTABLE
	};
}
