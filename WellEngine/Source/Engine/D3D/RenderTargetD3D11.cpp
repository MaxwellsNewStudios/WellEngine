#include "stdafx.h"
#include "RenderTargetD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


bool RenderTargetD3D11::Initialize(
	ID3D11Device *device, const UINT width, const UINT height, 
	const DXGI_FORMAT format, const bool hasSRV, const bool hasUAV)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	return Initialize(device, desc, hasSRV, hasUAV);
}

bool RenderTargetD3D11::Initialize(ID3D11Device *device, D3D11_TEXTURE2D_DESC desc, bool hasSRV, bool hasUAV)
{
	if (hasUAV)
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(device->CreateTexture2D(&desc, nullptr, _texture.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create render target texture!");
		return false;
	}

	if (FAILED(device->CreateShaderResourceView(_texture.Get(), nullptr, _srv.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create shader resource view!");
		return false;
	}

	if (FAILED(device->CreateRenderTargetView(_texture.Get(), nullptr, _rtv.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create render target view!");
		return false;
	}

	if (hasUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		if (FAILED(device->CreateUnorderedAccessView(_texture.Get(), &uavDesc, _uav.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create unordered access view!");
			return false;
		}
	}

	return true;
}

void RenderTargetD3D11::Reset()
{
	if (_texture)	{ _texture.ReleaseAndGetAddressOf();	_texture = nullptr; }
	if (_rtv)		{ _rtv.ReleaseAndGetAddressOf();		_rtv = nullptr; }
	if (_srv)		{ _srv.ReleaseAndGetAddressOf();		_srv = nullptr; }
	if (_uav)		{ _uav.ReleaseAndGetAddressOf();		_uav = nullptr; }
}


ID3D11Texture2D *RenderTargetD3D11::GetTexture() const
{
	return _texture.Get();
}

ID3D11RenderTargetView *RenderTargetD3D11::GetRTV() const
{
	return _rtv.Get();
}

ID3D11ShaderResourceView *RenderTargetD3D11::GetSRV() const
{
	return _srv.Get();
}

ID3D11UnorderedAccessView *RenderTargetD3D11::GetUAV() const
{
	return _uav.Get();
}
