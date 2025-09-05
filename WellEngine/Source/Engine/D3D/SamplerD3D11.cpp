#include "stdafx.h"
#include "SamplerD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


SamplerD3D11::SamplerD3D11(ID3D11Device *device, D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter)
{
	if (!Initialize(device, adressMode, filter))
		ErrMsg("Failed to initialize sampler in constructor!");
}
SamplerD3D11::SamplerD3D11(ID3D11Device *device, const D3D11_SAMPLER_DESC &desc)
{
	if (!Initialize(device, desc))
		ErrMsg("Failed to initialize sampler in constructor!");
}

bool SamplerD3D11::Initialize(ID3D11Device *device, D3D11_TEXTURE_ADDRESS_MODE adressMode, D3D11_FILTER filter, D3D11_COMPARISON_FUNC comparisonFunc)
{
	D3D11_SAMPLER_DESC samplerDesc = { };
	samplerDesc.Filter = filter;
	samplerDesc.AddressU = adressMode;
	samplerDesc.AddressV = adressMode;
	samplerDesc.AddressW = adressMode;
	samplerDesc.MipLODBias = 0;
	samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.MaxAnisotropy = 2;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	if (FAILED(device->CreateSamplerState(&samplerDesc, _sampler.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create sampler state!");
		return false;
	}

	return true;
}
bool SamplerD3D11::Initialize(ID3D11Device *device, const D3D11_SAMPLER_DESC &desc)
{
	HRESULT hr = device->CreateSamplerState(&desc, _sampler.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		ErrMsgF("Failed to create sampler state! hr: {}, {}", hr, StringUtils::HResultToString(hr));
		return false;
	}

	return true;
}


ID3D11SamplerState *SamplerD3D11::GetSamplerState() const
{
	return _sampler.Get();
}
