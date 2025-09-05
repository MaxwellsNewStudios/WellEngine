#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class RenderTargetD3D11
{
private:
	ComPtr<ID3D11Texture2D>				_texture = nullptr;
	ComPtr<ID3D11RenderTargetView>		_rtv = nullptr;
	ComPtr<ID3D11ShaderResourceView>	_srv = nullptr;
	ComPtr<ID3D11UnorderedAccessView>	_uav = nullptr;

public:
	RenderTargetD3D11() = default;
	~RenderTargetD3D11() = default;
	RenderTargetD3D11(const RenderTargetD3D11 &other) = delete;
	RenderTargetD3D11 &operator=(const RenderTargetD3D11 &other) = delete;
	RenderTargetD3D11(RenderTargetD3D11 &&other) = delete;
	RenderTargetD3D11 &operator=(RenderTargetD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, UINT width, UINT height,
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, bool hasSRV = false, bool hasUAV = false);
	[[nodiscard]] bool Initialize(ID3D11Device *device, D3D11_TEXTURE2D_DESC desc, 
		bool hasSRV = false, bool hasUAV = false);

	void Reset();

	[[nodiscard]] ID3D11Texture2D *GetTexture() const;
	[[nodiscard]] ID3D11RenderTargetView *GetRTV() const;
	[[nodiscard]] ID3D11ShaderResourceView *GetSRV() const;
	[[nodiscard]] ID3D11UnorderedAccessView *GetUAV() const;

	TESTABLE()
};