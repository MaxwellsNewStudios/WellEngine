#include "stdafx.h"
#include "ShaderResourceTextureD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

static void FormatCubemapTexture(std::vector<unsigned char> cubeFaces[6], const void *textureData, const UINT width, const UINT height, const UINT channels, const UINT bytesPerChannel)
{
	UINT cubeFaceSize = max(1, width / 4);
	UINT texMid = height / 2;

	const dx::XMUINT4 faceRects[6] = {
		{ cubeFaceSize * 2,	texMid - cubeFaceSize / 2,	   cubeFaceSize * 3, texMid + cubeFaceSize / 2	   }, // +X
		{ 0,				texMid - cubeFaceSize / 2,	   cubeFaceSize,	 texMid + cubeFaceSize / 2	   }, // -X
		{ cubeFaceSize,		texMid - cubeFaceSize * 3 / 2, cubeFaceSize * 2, texMid - cubeFaceSize / 2	   }, // +Y
		{ cubeFaceSize,		texMid + cubeFaceSize / 2,	   cubeFaceSize * 2, texMid + cubeFaceSize * 3 / 2 }, // -Y
		{ cubeFaceSize,		texMid - cubeFaceSize / 2,	   cubeFaceSize * 2, texMid + cubeFaceSize / 2	   }, // +Z
		{ cubeFaceSize * 3,	texMid - cubeFaceSize / 2,	   cubeFaceSize * 4, texMid + cubeFaceSize / 2	   }  // -Z
	};

	for (UINT i = 0; i < 6; i++)
	{
		// Copy the texture data for each face of the cubemap
		cubeFaces[i].resize((size_t)cubeFaceSize * cubeFaceSize * channels * bytesPerChannel);
		const dx::XMUINT4 &faceRect = faceRects[i];

		for (UINT y = 0; y < cubeFaceSize; y++)
		{
			UINT srcY = faceRect.y + y;

			for (UINT x = 0; x < cubeFaceSize; x++)
			{
				UINT srcX = faceRect.x + x;

				for (UINT z = 0; z < channels; z++)
				{
					UINT srcDataIndex = ((srcY * width + srcX) * channels + z) * bytesPerChannel;
					UINT cubeDataIndex = ((y * cubeFaceSize + x) * channels + z) * bytesPerChannel;

					for (UINT w = 0; w < bytesPerChannel; w++)
						cubeFaces[i][(size_t)cubeDataIndex + w] = ((const unsigned char *)textureData)[(size_t)srcDataIndex + w];
				}
			}
		}
	}
}

bool ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
	const UINT width, const UINT height, const void* textureData, bool autoMipmaps, bool cubemap)
{
	if (context == nullptr)
		autoMipmaps = false;

	_size = { width, height };
	_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	_mipmapped = autoMipmaps;
	if (cubemap)
		_dim = TexDim::Cubemap;

	D3D11_TEXTURE2D_DESC textureDesc = { };
	textureDesc.Width = cubemap ? max(1, width / 4) : width;
	textureDesc.Height = cubemap ? max(1, width / 4) : height;
	textureDesc.ArraySize = cubemap ? 6 : 1;
	textureDesc.MipLevels = (_mipmapped) ? 0 : 1;
	textureDesc.Format = _format;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;

	if (_mipmapped)
	{
		textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	if (cubemap)
	{
		textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	}

	if (cubemap && !_mipmapped)
	{
		std::vector<unsigned char> cubeFaces[6]{};
		FormatCubemapTexture(cubeFaces, textureData, width, height, 4, 1);
		
		UINT cubeFaceSize = max(1, width / 4);
		D3D11_SUBRESOURCE_DATA srData[6]{};
		for (UINT i = 0; i < 6; i++)
		{
			srData[i].pSysMem = cubeFaces[i].data();
			srData[i].SysMemPitch = cubeFaceSize * sizeof(unsigned char) * 4;
			srData[i].SysMemSlicePitch = 0;
		}

		if (FAILED(device->CreateTexture2D(&textureDesc, srData, _texture.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create texture2D!");
			return false;
		}
	}
	else
	{ 
		D3D11_SUBRESOURCE_DATA srData = { };
		srData.pSysMem = textureData;
		srData.SysMemPitch = width * sizeof(unsigned char) * 4;
		srData.SysMemSlicePitch = 0;

		if (FAILED(device->CreateTexture2D(&textureDesc, (_mipmapped) ? nullptr : &srData, _texture.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create texture2D!");
			return false;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	srvDesc.Format = _format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = (_mipmapped) ? -1 : 1;

	if (cubemap)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = (_mipmapped) ? -1 : 1;
	}

	if (FAILED(device->CreateShaderResourceView(_texture.Get(), (_mipmapped) ? &srvDesc : nullptr, _srv.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create shader resource view!");
		return false;
	}

	if (_mipmapped)
	{
		if (cubemap) 
		{
			D3D11_TEXTURE2D_DESC pDesc;
			_texture->GetDesc(&pDesc);
			UINT mipLevels = pDesc.MipLevels;

			std::vector<unsigned char> cubeFaces[6]{};
			FormatCubemapTexture(cubeFaces, textureData, width, height, 4, 1);

			UINT cubeFaceSize = max(1, width / 4);

			for (UINT i = 0; i < 6; i++)
			{
				UINT subresource = D3D11CalcSubresource(0, i, mipLevels);
				context->UpdateSubresource(_texture.Get(), subresource, nullptr, cubeFaces[i].data(), cubeFaceSize * sizeof(unsigned char) * 4, 0);
			}
		}
		else 
		{
			context->UpdateSubresource(_texture.Get(), 0, nullptr, textureData, width * sizeof(unsigned char) * 4, 0);
		}

		context->GenerateMips(_srv.Get());
	}

	return true;
}

bool ShaderResourceTextureD3D11::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
	const UINT width, const UINT height, const void* textureData, DXGI_FORMAT format, bool autoMipmaps, bool cubemap)
{
	if (context == nullptr)
		autoMipmaps = false;

	_size = { width, height };
	_format = format;
	_mipmapped = autoMipmaps;
	if (cubemap)
		_dim = TexDim::Cubemap;

	UINT channelCount = (UINT)D3D11FormatData::GetChannelCount(_format);
	UINT bitsPerPixel = (UINT)D3D11FormatData::GetBitsPerPixel(_format);
	UINT bytesPerChannel = bitsPerPixel / (channelCount * 8);
	UINT bytesPerPixel = bytesPerChannel * channelCount;

	D3D11_TEXTURE2D_DESC textureDesc = { };
	textureDesc.Width = cubemap ? max(1, width / 4) : width;
	textureDesc.Height = cubemap ? max(1, width / 4) : height;
	textureDesc.ArraySize = cubemap ? 6 : 1;
	textureDesc.MipLevels = (_mipmapped) ? 0 : 1;
	textureDesc.Format = _format;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;

	if (_mipmapped)
	{
		textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		textureDesc.Usage = D3D11_USAGE_DEFAULT;
		textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	if (cubemap)
	{
		textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
	}

	if (cubemap && !_mipmapped)
	{
		std::vector<unsigned char> cubeFaces[6]{};
		FormatCubemapTexture(cubeFaces, textureData, width, height, channelCount, bytesPerChannel);
		
		UINT cubeFaceSize = max(1, width / 4);
		D3D11_SUBRESOURCE_DATA srData[6]{};
		for (UINT i = 0; i < 6; i++)
		{
			srData[i].pSysMem = cubeFaces[i].data();
			srData[i].SysMemPitch = cubeFaceSize * bytesPerPixel;
			srData[i].SysMemSlicePitch = 0;
		}

		if (FAILED(device->CreateTexture2D(&textureDesc, srData, _texture.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create texture2D!");
			return false;
		}
	}
	else
	{ 
		D3D11_SUBRESOURCE_DATA srData = { };
		srData.pSysMem = textureData;
		srData.SysMemPitch = width * bytesPerPixel;
		srData.SysMemSlicePitch = 0;

		if (FAILED(device->CreateTexture2D(&textureDesc, (_mipmapped) ? nullptr : &srData, _texture.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create texture2D!");
			return false;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	srvDesc.Format = _format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = (_mipmapped) ? -1 : 1;

	if (cubemap)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = (_mipmapped) ? -1 : 1;
	}

	if (FAILED(device->CreateShaderResourceView(_texture.Get(), (_mipmapped) ? &srvDesc : nullptr, _srv.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create shader resource view!");
		return false;
	}

	if (_mipmapped)
	{
		if (cubemap) 
		{
			D3D11_TEXTURE2D_DESC pDesc;
			_texture->GetDesc(&pDesc);
			UINT mipLevels = pDesc.MipLevels;

			std::vector<unsigned char> cubeFaces[6]{};
			FormatCubemapTexture(cubeFaces, textureData, width, height, channelCount, bytesPerChannel);

			UINT cubeFaceSize = max(1, width / 4);

			for (UINT i = 0; i < 6; i++)
			{
				UINT subresource = D3D11CalcSubresource(0, i, mipLevels);
				context->UpdateSubresource(_texture.Get(), subresource, nullptr, cubeFaces[i].data(), cubeFaceSize * bytesPerPixel, 0);
			}
		}
		else 
		{
			context->UpdateSubresource(_texture.Get(), 0, nullptr, textureData, width * bytesPerPixel, 0);
		}

		context->GenerateMips(_srv.Get());
	}

	return true;
}

bool ShaderResourceTextureD3D11::Initialize(ID3D11Device *device, ID3D11DeviceContext* context, 
	const D3D11_TEXTURE2D_DESC &textureDesc, const D3D11_SUBRESOURCE_DATA *srData, bool autoMipmaps)
{
	if (context == nullptr)
		autoMipmaps = false;

	_size = { textureDesc.Width, textureDesc.Height };
	_format = textureDesc.Format;
	_mipmapped = autoMipmaps;
	if ((textureDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0)
		_dim = TexDim::Cubemap;

	UINT channelCount = (UINT)D3D11FormatData::GetChannelCount(_format);
	UINT bitsPerPixel = (UINT)D3D11FormatData::GetBitsPerPixel(_format);
	UINT bytesPerChannel = bitsPerPixel / (channelCount * 8);
	UINT bytesPerPixel = bytesPerChannel * channelCount;

	if (FAILED(device->CreateTexture2D(&textureDesc, (_mipmapped) ? nullptr : srData, _texture.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create texture2D!");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = (autoMipmaps) ? -1 : 1;

	if (_dim == TexDim::Cubemap)
	{
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = (_mipmapped) ? -1 : 1;
	}

	if (FAILED(device->CreateShaderResourceView(_texture.Get(), (_mipmapped) ? &srvDesc : nullptr, _srv.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create shader resource view!");
		return false;
	}

	if (_mipmapped)
	{
		if (_dim == TexDim::Cubemap)
		{
			D3D11_TEXTURE2D_DESC pDesc;
			_texture->GetDesc(&pDesc);
			UINT mipLevels = pDesc.MipLevels;

			std::vector<unsigned char> cubeFaces[6]{};
			FormatCubemapTexture(cubeFaces, srData->pSysMem, _size.x, _size.y, channelCount, bytesPerChannel);

			UINT cubeFaceSize = max(1, _size.x / 4);

			for (UINT i = 0; i < 6; i++)
			{
				UINT subresource = D3D11CalcSubresource(0, i, mipLevels);
				context->UpdateSubresource(_texture.Get(), subresource, nullptr, cubeFaces[i].data(), cubeFaceSize * bytesPerPixel, 0);
			}
		}
		else
		{
			context->UpdateSubresource(_texture.Get(), 0, nullptr, srData->pSysMem, srData->SysMemPitch, 0);
		}

		context->GenerateMips(_srv.Get());
	}

	return true;
}

bool ShaderResourceTextureD3D11::Initialize(ComPtr<ID3D11Texture2D> &&texture, ComPtr<ID3D11ShaderResourceView> &&srv)
{
	if (texture == nullptr)
	{
		ErrMsg("Texture is null!");
		return false;
	}

	if (srv == nullptr)
	{
		ErrMsg("SRV is null!");
		return false;
	}

	_texture = std::move(texture);
	_srv = std::move(srv);

	D3D11_TEXTURE2D_DESC desc;
	_texture->GetDesc(&desc);

	_size = { desc.Width, desc.Height };
	_format = desc.Format;
	_mipmapped = desc.MipLevels > 1;

	if ((desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0)
		_dim = TexDim::Cubemap;

	return true;
}

ID3D11Texture2D *ShaderResourceTextureD3D11::GetTexture() const
{
	return _texture.Get();
}
ID3D11ShaderResourceView *ShaderResourceTextureD3D11::GetSRV() const
{
	return _srv.Get();
}

const dx::XMUINT2 &ShaderResourceTextureD3D11::GetSize() const
{
	return _size;
}
DXGI_FORMAT ShaderResourceTextureD3D11::GetFormat() const
{
	return _format;
}
bool ShaderResourceTextureD3D11::IsMipmapped() const
{
	return _mipmapped;
}
bool ShaderResourceTextureD3D11::IsCubemap() const
{
	return _dim == TexDim::Cubemap;
}
