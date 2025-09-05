#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <array>

class SamplerD3D11
{
private:
	Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampler = nullptr;

public:
	SamplerD3D11() = default;
	SamplerD3D11(ID3D11Device *device, D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter);
	SamplerD3D11(ID3D11Device *device, const D3D11_SAMPLER_DESC &desc);
	~SamplerD3D11() = default;
	SamplerD3D11(const SamplerD3D11 &other) = delete;
	SamplerD3D11 &operator=(const SamplerD3D11 &other) = delete;
	SamplerD3D11(SamplerD3D11 &&other) = delete;
	SamplerD3D11 &operator=(SamplerD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, 
		D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter, 
		D3D11_COMPARISON_FUNC comparisonFunc = D3D11_COMPARISON_NEVER);
	[[nodiscard]] bool Initialize(ID3D11Device *device, const D3D11_SAMPLER_DESC &desc);

	[[nodiscard]] ID3D11SamplerState *GetSamplerState() const;

	TESTABLE()
};