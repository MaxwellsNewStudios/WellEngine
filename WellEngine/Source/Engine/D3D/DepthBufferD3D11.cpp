#include "stdafx.h"
#include "DepthBufferD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


DepthBufferD3D11::DepthBufferD3D11(ID3D11Device *device, const UINT width, const UINT height, 
	const DXGI_FORMAT format, bool hasSRV, bool isCube) : _isCube(isCube)
{
	if (!Initialize(device, width, height, format, hasSRV, isCube))
		ErrMsg("Failed to initialize depth buffer in constructor!");
}

bool DepthBufferD3D11::Initialize(ID3D11Device *device, const UINT width, const UINT height, 
	const DXGI_FORMAT format, const bool hasSRV, const UINT arraySize, bool isCube)
{
	_isCube = isCube;

	DXGI_FORMAT formatTyped = format;
	DXGI_FORMAT formatTypeless{};

	switch (format)
	{
	case DXGI_FORMAT_D32_FLOAT:
		formatTypeless = DXGI_FORMAT_R32_TYPELESS;
		break;

	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		formatTypeless = DXGI_FORMAT_R24G8_TYPELESS;
		break;

	default: // Default to 16-bit
		DbgMsg(std::format("Warning: Unrecognized DXGI_FORMAT '{}' for depth buffer!", (size_t)format));
		formatTyped = DXGI_FORMAT_D16_UNORM;
		[[fallthrough]];
	case DXGI_FORMAT_D16_UNORM:
		formatTypeless = DXGI_FORMAT_R16_TYPELESS;
		break;
	}

	UINT clampedArraySize = arraySize;
	if (clampedArraySize < 1)
		clampedArraySize = 1;

	// Increase size by six in desired places only if this is a cubemap
	UINT isCubeMult = isCube ? 6 : 1;

	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = clampedArraySize * isCubeMult;
	textureDesc.Format = formatTyped;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

	if (hasSRV)
	{
		textureDesc.Format = formatTypeless;
		textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	}

	if (FAILED(device->CreateTexture2D(&textureDesc, nullptr, _texture.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create depth buffer texture!");
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	const D3D11_DEPTH_STENCIL_VIEW_DESC *dsvDescPtr = nullptr;

	if (hasSRV)
	{
		dsvDesc.Format = formatTyped;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.ArraySize = 1 * isCubeMult;

		dsvDescPtr = &dsvDesc;
	}

	_dsvs.reserve(clampedArraySize);
	for (UINT i = 0; i < clampedArraySize; i++)
	{
		if (isCube || hasSRV)
			dsvDesc.Texture2DArray.FirstArraySlice = i * isCubeMult;

		ComPtr<ID3D11DepthStencilView> dsView;
		if (FAILED(device->CreateDepthStencilView(_texture.Get(), dsvDescPtr, dsView.ReleaseAndGetAddressOf())))
		{
			ErrMsgF("Failed to create depth stencil view #{}!", i);
			return false;
		}
		_dsvs.emplace_back(dsView);
	}

	if (hasSRV)
	{
		DXGI_FORMAT srvType = formatTyped;

		switch (formatTyped)
		{
		case DXGI_FORMAT_D32_FLOAT:
			srvType = DXGI_FORMAT_R32_FLOAT;
			break;

		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			srvType = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			break;

		case DXGI_FORMAT_D16_UNORM:
			srvType = DXGI_FORMAT_R16_UNORM;
			break;

		default:
			break;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
		srvDesc.Format = srvType;
		if (isCube)
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
			srvDesc.TextureCubeArray.MostDetailedMip = 0;
			srvDesc.TextureCubeArray.MipLevels = 1;
			srvDesc.TextureCubeArray.First2DArrayFace = 0;
			srvDesc.TextureCubeArray.NumCubes = clampedArraySize;
		}
		else
		{
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.MipLevels = 1;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.ArraySize = clampedArraySize;
		}

		if (FAILED(device->CreateShaderResourceView(_texture.Get(), &srvDesc, _srv.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create depth buffer shader resource view!");
			return false;
		}
	}

	return true;
}

void DepthBufferD3D11::Reset()
{
	if (_srv != nullptr)
		_srv.Reset();

	for (auto &dsv : _dsvs)
	{
		if (dsv != nullptr)
			dsv.Reset();
	}

	if (_texture != nullptr)
		_texture.Reset();

	_texture = nullptr;
	_dsvs.clear();
	_srv = nullptr;
}

ID3D11DepthStencilView *DepthBufferD3D11::GetDSV(UINT lightIndex) const
{
	if (_dsvs.size() <= lightIndex)
		return nullptr;
	return _dsvs[lightIndex].Get();
}

ID3D11ShaderResourceView *DepthBufferD3D11::GetSRV() const
{
	return _srv.Get();
}
