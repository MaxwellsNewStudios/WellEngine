#pragma once

#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

#include "Engine/D3D/StructuredBufferD3D11.h"
#include "Engine/D3D/DepthBufferD3D11.h"
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightSpot.h"
#include "Game/Behaviours/Rendering/Lighting/B_LightSpotSimple.h"

namespace WellEngine
{
	class LightSpotCollection : public IRefTarget<LightSpotCollection>
	{
	private:
		struct SimpleSpotLightData
		{
			B_LightSpotSimple *lightBehaviour = nullptr;
			bool isEnabled = true;
		};
		struct SpotLightData
		{
			B_LightSpot *lightBehaviour = nullptr;
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
		LightSpotCollection() = default;
		~LightSpotCollection() = default;
		LightSpotCollection(const LightSpotCollection &other) = delete;
		LightSpotCollection &operator=(const LightSpotCollection &other) = delete;
		LightSpotCollection(LightSpotCollection &&other) = delete;
		LightSpotCollection &operator=(LightSpotCollection &&other) = delete;

		[[nodiscard]] bool Initialize(ID3D11Device *device, UINT resolution);

		[[nodiscard]] bool UpdateBuffers(ID3D11Device *device, ID3D11DeviceContext *context);
		[[nodiscard]] bool BindCSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool BindPSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool UnbindCSBuffers(ID3D11DeviceContext *context) const;
		[[nodiscard]] bool UnbindPSBuffers(ID3D11DeviceContext *context) const;

		[[nodiscard]] UINT GetNrOfLights() const;
		[[nodiscard]] UINT GetNrOfSimpleLights() const;
		[[nodiscard]] B_LightSpot *GetLightBehaviour(UINT lightIndex) const;
		[[nodiscard]] B_LightSpotSimple *GetSimpleLightBehaviour(UINT lightIndex) const;
		[[nodiscard]] ID3D11DepthStencilView *GetShadowMapDSV(UINT lightIndex) const;
		[[nodiscard]] ID3D11ShaderResourceView *GetShadowMapsSRV() const;
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

		[[nodiscard]] bool RegisterLight(B_LightSpot *light);
		[[nodiscard]] bool UnregisterLight(B_LightSpot *light);
		[[nodiscard]] bool UnregisterLight(UINT lightIndex);
		[[nodiscard]] bool RegisterSimpleLight(B_LightSpotSimple *light);
		[[nodiscard]] bool UnregisterSimpleLight(B_LightSpotSimple *light);
		[[nodiscard]] bool UnregisterSimpleLight(UINT lightIndex);

		TESTABLE
	};
}
