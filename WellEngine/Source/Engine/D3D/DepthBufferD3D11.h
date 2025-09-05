#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

class DepthBufferD3D11
{
private:
	ComPtr<ID3D11Texture2D> _texture = nullptr;
	ComPtr<ID3D11ShaderResourceView> _srv = nullptr;
	std::vector<ComPtr<ID3D11DepthStencilView>> _dsvs;
	bool _isCube = false;

public:
	DepthBufferD3D11() = default;
	DepthBufferD3D11(ID3D11Device *device, UINT width, UINT height, DXGI_FORMAT format, bool hasSRV = false, bool isCube = false);
	~DepthBufferD3D11() = default;
	DepthBufferD3D11(const DepthBufferD3D11 &other) = delete;
	DepthBufferD3D11 &operator=(const DepthBufferD3D11 &other) = delete;
	DepthBufferD3D11(DepthBufferD3D11 &&other) = delete;
	DepthBufferD3D11 &operator=(DepthBufferD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, UINT width, UINT height, DXGI_FORMAT format,
		bool hasSRV = false, UINT arraySize = 1, bool isCube = false);
	void Reset();

	[[nodiscard]] ID3D11DepthStencilView *GetDSV(UINT lightIndex) const;
	[[nodiscard]] ID3D11ShaderResourceView *GetSRV() const;

	TESTABLE()
};