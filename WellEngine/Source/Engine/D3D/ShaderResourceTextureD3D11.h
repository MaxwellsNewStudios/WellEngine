#pragma once

#include <d3d11.h>
#include <wrl/client.h>

enum class TexDim
{
	Tex2D,
	Tex2DArray,
	Tex3D,
	Tex3DArray,
	Cubemap,
	CubemapArray,

	COUNT
};

class ShaderResourceTextureD3D11
{
private:
	ComPtr<ID3D11Texture2D> _texture = nullptr;
	ComPtr<ID3D11ShaderResourceView> _srv = nullptr;

	dx::XMUINT2 _size = {0, 0};
	DXGI_FORMAT _format = DXGI_FORMAT_UNKNOWN;
	TexDim _dim = TexDim::Tex2D;
	bool _mipmapped = false;

public:
	ShaderResourceTextureD3D11() = default;
	~ShaderResourceTextureD3D11() = default;

	ShaderResourceTextureD3D11(const ShaderResourceTextureD3D11 &other) = delete;
	ShaderResourceTextureD3D11 &operator=(const ShaderResourceTextureD3D11 &other) = delete;
	ShaderResourceTextureD3D11(ShaderResourceTextureD3D11 &&other) = delete;
	ShaderResourceTextureD3D11 &operator=(ShaderResourceTextureD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, ID3D11DeviceContext *context, UINT width, UINT height, const void *textureData, bool autoMipmaps, bool cubemap = false);
	[[nodiscard]] bool Initialize(ID3D11Device *device, ID3D11DeviceContext *context, UINT width, UINT height, const void *textureData, DXGI_FORMAT format, bool autoMipmaps, bool cubemap = false);
	[[nodiscard]] bool Initialize(ID3D11Device *device, ID3D11DeviceContext *context, const D3D11_TEXTURE2D_DESC &textureDesc, const D3D11_SUBRESOURCE_DATA *srData, bool autoMipmaps);
	[[nodiscard]] bool Initialize(ComPtr<ID3D11Texture2D> &&texture, ComPtr<ID3D11ShaderResourceView>  &&srv);

	[[nodiscard]] ID3D11Texture2D *GetTexture() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetSRV() const;

	[[nodiscard]] const dx::XMUINT2 &GetSize() const;
	[[nodiscard]] DXGI_FORMAT GetFormat() const;
	[[nodiscard]] bool IsMipmapped() const;
	[[nodiscard]] bool IsCubemap() const;

	TESTABLE()
};