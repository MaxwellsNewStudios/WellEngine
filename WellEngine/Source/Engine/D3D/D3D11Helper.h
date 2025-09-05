#pragma once

#include <Windows.h>
#include <d3d11.h>


[[nodiscard]] bool SetupD3D11(
	bool fullscreen,
	const UINT width, const UINT height, const HWND window,
	ID3D11Device *&device, 
	ID3D11DeviceContext *&immediateContext, 
	ID3D11DeviceContext **deferredContexts,
	IDXGISwapChain *&swapChain, 
	ID3D11RenderTargetView *&rtv,
	ID3D11Texture2D *&dsTexture, 
	ID3D11DepthStencilView *&dsView,
	ID3D11UnorderedAccessView *&uav,
	ID3D11DepthStencilState *&normalDepthStencilState,
	ID3D11DepthStencilState *&reverseZDepthStencilState,
	ID3D11DepthStencilState *&transparentDepthStencilState,
	ID3D11DepthStencilState *&nullDepthStencilState,
	D3D11_VIEWPORT &viewport);

[[nodiscard]] bool ResizeD3D11(
	bool fullscreen,
	const UINT width, const UINT height,
	ID3D11Device *&device,
	ID3D11DeviceContext *&immediateContext,
	ID3D11DeviceContext **deferredContexts,
	IDXGISwapChain *&swapChain,
	ID3D11RenderTargetView *&rtv,
	ID3D11Texture2D *&dsTexture,
	ID3D11DepthStencilView *&dsView,
	ID3D11UnorderedAccessView *&uav,
	D3D11_VIEWPORT &viewport);




void ReportLiveDeviceObjects(ID3D11Device *&device);