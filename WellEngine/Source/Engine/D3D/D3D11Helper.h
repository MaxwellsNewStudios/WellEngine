#pragma once

#include <d3d11.h>

namespace WellEngine
{
	[[nodiscard]] bool SetupD3D11(
		bool fullscreen,
		const UINT width, const UINT height, const HWND window,
		ID3D11Device *&device, 
		ID3D11DeviceContext *&immediateContext, 
		IDXGISwapChain *&swapChain, 
		ID3D11RenderTargetView *&rtv,
		ID3D11Texture2D *&dsTexture, 
		ID3D11DepthStencilView *&dsView,
		ID3D11UnorderedAccessView *&uav,
		D3D11_VIEWPORT &viewport);

	[[nodiscard]] bool ResizeD3D11(
		bool fullscreen,
		const UINT width, const UINT height,
		ID3D11Device *&device,
		ID3D11DeviceContext *&immediateContext,
		IDXGISwapChain *&swapChain,
		ID3D11RenderTargetView *&rtv,
		ID3D11Texture2D *&dsTexture,
		ID3D11DepthStencilView *&dsView,
		ID3D11UnorderedAccessView *&uav,
		D3D11_VIEWPORT &viewport);

	void ReportLiveDeviceObjects(ID3D11Device *&device);
}
