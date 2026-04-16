#include "stdafx.h"
#include "MipRenderTargetD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


bool MipRenderTargetD3D11::Initialize(
	ID3D11Device *device, UINT width, UINT height, UINT mipLevels, 
	DXGI_FORMAT format, bool hasUAV)
{
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = mipLevels;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	return Initialize(device, desc, hasUAV);
}

bool MipRenderTargetD3D11::Initialize(ID3D11Device *device, D3D11_TEXTURE2D_DESC desc, bool hasUAV)
{
	if (hasUAV)
		desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

	if (FAILED(device->CreateTexture2D(&desc, nullptr, _texture.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create render target texture!");
		return false;
	}

	D3D11_TEXTURE2D_DESC mipDesc{};
	_texture->GetDesc(&mipDesc);
	UINT mipLevels = mipDesc.MipLevels;

	_mipLevels.resize(mipLevels);

	for (int i = 0; i < mipLevels; i++)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = desc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = i;
		if (FAILED(device->CreateRenderTargetView(_texture.Get(), &rtvDesc, _mipLevels[i].rtv.ReleaseAndGetAddressOf())))
		{
			ErrMsgF("Failed to create render target view for mip level {}!", i);
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = i;
		if (FAILED(device->CreateShaderResourceView(_texture.Get(), &srvDesc, _mipLevels[i].srv.ReleaseAndGetAddressOf())))
		{
			ErrMsgF("Failed to create shader resource view for mip level {}!", i);
			return false;
		}

		if (hasUAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = desc.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = i;
			if (FAILED(device->CreateUnorderedAccessView(_texture.Get(), &uavDesc, _mipLevels[i].uav.ReleaseAndGetAddressOf())))
			{
				ErrMsgF("Failed to create unordered access view for mip level {}!", i);
				return false;
			}
		}
	}

	return true;
}

void MipRenderTargetD3D11::Reset()
{
	if (_texture)	{ _texture.ReleaseAndGetAddressOf();	_texture = nullptr; }

	for (auto &mipLevel : _mipLevels)
	{
		if (mipLevel.rtv) { mipLevel.rtv.ReleaseAndGetAddressOf(); mipLevel.rtv = nullptr; }
		if (mipLevel.srv) { mipLevel.srv.ReleaseAndGetAddressOf(); mipLevel.srv = nullptr; }
		if (mipLevel.uav) { mipLevel.uav.ReleaseAndGetAddressOf(); mipLevel.uav = nullptr; }
	}
	_mipLevels.clear();
}


ID3D11Texture2D *MipRenderTargetD3D11::GetTexture() const
{
	return _texture.Get();
}

ID3D11RenderTargetView *MipRenderTargetD3D11::GetRTV(UINT mipLevel) const
{
	if (mipLevel >= _mipLevels.size())
		return nullptr;
	return _mipLevels[mipLevel].rtv.Get();
}

ID3D11ShaderResourceView *MipRenderTargetD3D11::GetSRV(UINT mipLevel) const
{
	if (mipLevel >= _mipLevels.size())
		return nullptr;
	return _mipLevels[mipLevel].srv.Get();
}

ID3D11UnorderedAccessView *MipRenderTargetD3D11::GetUAV(UINT mipLevel) const
{
	if (mipLevel >= _mipLevels.size())
		return nullptr;
	return _mipLevels[mipLevel].uav.Get();
}
