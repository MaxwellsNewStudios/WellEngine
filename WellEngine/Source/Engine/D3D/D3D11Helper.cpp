#include "stdafx.h"
#include "D3D/D3D11Helper.h"
#include "Rendering/Graphics.h"
#include <wrl/client.h>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;

static bool CreateInterfaces(
	bool fullscreen,
	const UINT width, const UINT height, const HWND window,
	ID3D11Device *&device, 
	ID3D11DeviceContext *&immediateContext,
	ID3D11DeviceContext **deferredContexts,
	IDXGISwapChain *&swapChain
	)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = SWAPCHAIN_BUFFER_FORMAT;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	swapChainDesc.OutputWindow = window;
	swapChainDesc.Windowed = !fullscreen;
	swapChainDesc.BufferCount = SWAPCHAIN_BUFFER_COUNT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DXGI_SWAP_EFFECT_FLIP_DISCARD DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
	swapChainDesc.Flags = 0;
	swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

#ifdef DEBUG_D3D11_DEVICE
	std::ifstream file("DebugInfoFlag");
	UINT fileFlag;
	file >> fileFlag;

	UINT flags = fileFlag; // 0 or D3D11_CREATE_DEVICE_DEBUG 
#else
	UINT flags = 0;
#endif

	constexpr D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

#if(1)
	if (FAILED(D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE,
			nullptr, flags, featureLevels,
			1, D3D11_SDK_VERSION, &swapChainDesc,
			&swapChain, &device, nullptr, &immediateContext)))
	{
		ErrMsg("Failed to create device and swap chain!");
		return false;
	}
#else
	// Create device
	if (FAILED(D3D11CreateDevice(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, 
		nullptr, flags, featureLevels, 
		1, D3D11_SDK_VERSION, &device, 
		nullptr, &immediateContext)))
	{
		ErrMsg("Failed to create device!");
		return false;
	}

	// Create swap chain
	ComPtr<IDXGIDevice> dxgiDevice;
	if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()))))
	{
		ErrMsg("Failed to get IDXGIDevice!");
		return false;
	}

	ComPtr<IDXGIAdapter> dxgiAdapter;
	if (FAILED(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf())))
	{
		ErrMsg("Failed to get IDXGIAdapter!");
		return false;
	}

	ComPtr<IDXGIFactory> dxgiFactory;
	if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()))))
	{
		ErrMsg("Failed to get IDXGIFactory!");
		return false;
	}

	if (FAILED(dxgiFactory->CreateSwapChain(device, &swapChainDesc, &swapChain)))
	{
		ErrMsg("Failed to create swap chain!");
		return false;
	}
#endif

	if (deferredContexts)
	{
		for (UINT i = 0; i < PARALLEL_THREADS; i++)
		{
			if (FAILED(device->CreateDeferredContext(0, &(deferredContexts[i]))))
			{
				ErrMsg("Failed to create deferred context!");
				return false;
			}
		}
	}

	return true;
}

static bool CreateRenderTargetView(
	ID3D11Device *device, 
	IDXGISwapChain *swapChain, 
	ID3D11RenderTargetView *&rtv,
	ID3D11UnorderedAccessView *&uav)
{
	// get the address of the back buffer
	ID3D11Texture2D *backBuffer = nullptr;
	if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&backBuffer))))
	{
		ErrMsg("Failed to get back buffer!");
		return false;
	}

	// use the back buffer address to create the render target
	// null as description to base it on the backbuffers values
	if (FAILED(device->CreateRenderTargetView(backBuffer, nullptr, &rtv)))
	{
		ErrMsg("Failed to create render target view!");
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = { };
	uavDesc.Format = SWAPCHAIN_BUFFER_FORMAT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	if (FAILED(device->CreateUnorderedAccessView(backBuffer, &uavDesc, &uav)))
	{
		ErrMsg("Failed to create unordered access view!");
		return false;
	}

	backBuffer->Release();
	return true;
}

static bool CreateDepthStencil(
	ID3D11Device *device, const UINT width, const UINT height, 
	ID3D11Texture2D *&dsTexture, 
	ID3D11DepthStencilView *&dsView)
{
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
#ifdef DEBUG_BUILD
	textureDesc.Format = DXGI_FORMAT_D16_UNORM; // DXGI_FORMAT_D16_UNORM DXGI_FORMAT_D24_UNORM_S8_UINT DXGI_FORMAT_D32_FLOAT
#else
	textureDesc.Format = VIEW_DEPTH_BUFFER_FORMAT;
#endif
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	if (FAILED(device->CreateTexture2D(&textureDesc, nullptr, &dsTexture)))
	{
		ErrMsg("Failed to create depth stencil texture!");
		return false;
	}
	
	if (FAILED(device->CreateDepthStencilView(dsTexture, nullptr, &dsView)))
	{
		ErrMsg("Failed to create depth stencil view!");
		return false;
	}

	return true;
}

static bool CreateDepthStencilStates(ID3D11Device *device,
	ID3D11DepthStencilState *&normalDepthStencilState, 
	ID3D11DepthStencilState *&reverseZDepthStencilState, 
	ID3D11DepthStencilState *&transparentDepthStencilState,
	ID3D11DepthStencilState *&nullDepthStencilState)
{
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = { };
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	if (FAILED(device->CreateDepthStencilState(&depthStencilDesc, &normalDepthStencilState)))
	{
		ErrMsg("Failed to create normal depth stencil state!");
		return false;
	}

	depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;

	if (FAILED(device->CreateDepthStencilState(&depthStencilDesc, &reverseZDepthStencilState)))
	{
		ErrMsg("Failed to create reverse depth stencil state!");
		return false;
	}

	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	if (FAILED(device->CreateDepthStencilState(&depthStencilDesc, &transparentDepthStencilState)))
	{
		ErrMsg("Failed to create transparent depth stencil state!");
		return false;
	}

	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	if (FAILED(device->CreateDepthStencilState(&depthStencilDesc, &nullDepthStencilState)))
	{
		ErrMsg("Failed to create null depth stencil state!");
		return false;
	}

	return true;
}

static void SetViewport(D3D11_VIEWPORT &viewport, const UINT width, const UINT height)
{
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;
}


bool SetupD3D11(
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
	D3D11_VIEWPORT &viewport)
{
	if (!CreateInterfaces(fullscreen, width, height, window, device, immediateContext, deferredContexts, swapChain))
	{
		ErrMsg("Error creating interfaces!");
		return false;
	}

	if (!CreateRenderTargetView(device, swapChain, rtv, uav))
	{
		ErrMsg("Error creating rtv!");
		return false;
	}

	if (!CreateDepthStencil(device, width, height, dsTexture, dsView))
	{
		ErrMsg("Error creating depth stencil view!");
		return false;
	}

	if (!CreateDepthStencilStates(device, normalDepthStencilState, reverseZDepthStencilState, transparentDepthStencilState, nullDepthStencilState))
	{
		ErrMsg("Error creating depth stencil state!");
		return false;
	}

	SetViewport(viewport, width, height);
	
	return true;
}


bool ResizeD3D11(
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
	D3D11_VIEWPORT &viewport)
{
	if (swapChain)
	{
		immediateContext->OMSetRenderTargets(0, 0, 0);

		if (FAILED(swapChain->SetFullscreenState(fullscreen, nullptr)))
		{
			ErrMsg("Failed to set fullscreen state!");
			return false;
		}

		if (FAILED(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)))
		{
			ErrMsg("Failed to resize swap chain buffers!");
			return false;
		}

		if (!CreateRenderTargetView(device, swapChain, rtv, uav))
		{
			ErrMsg("Error creating rtv!");
			return false;
		}

		if (!CreateDepthStencil(device, width, height, dsTexture, dsView))
		{
			ErrMsg("Error creating depth stencil view!");
			return false;
		}

		// Set up the viewport.
		SetViewport(viewport, width, height);
	}

	return true;
}






#ifdef _DEBUG
void ReportLiveDeviceObjects(ID3D11Device *&device)
{
	OutputDebugString(L"-------------------------------------------------------------------------------------------------------------\n");
	OutputDebugString(L"--------------------| Live Device Object Report |------------------------------------------------------------\n");
	OutputDebugString(L"-------------------------------------------------------------------------------------------------------------\n");

	ComPtr<ID3D11Debug> debug;
	device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void **>(debug.GetAddressOf()));
	debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);

	ComPtr<ID3D11InfoQueue> infoQueue;
	if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void **>(debug.GetAddressOf()))))
	{
		if (SUCCEEDED(debug.Get()->QueryInterface(__uuidof(ID3D11InfoQueue), reinterpret_cast<void **>(infoQueue.GetAddressOf()))))
		{
			D3D11_MESSAGE_ID filterListIds[] = {
				D3D11_MESSAGE_ID_UPDATESUBRESOURCE_PREFERUPDATESUBRESOURCE1
			};
			int numFilterListIds = sizeof(filterListIds) / sizeof(D3D11_MESSAGE_ID);

			D3D11_MESSAGE_ID *deniedIds = true ? NULL : filterListIds;
			int numDeniedIds = true ? 0 : numFilterListIds;

			static D3D11_INFO_QUEUE_FILTER filter;
			memset(&filter, 0, sizeof(filter));

			filter.DenyList.NumIDs = numDeniedIds;
			filter.DenyList.pIDList = deniedIds;

			// setup both storage and retrieval filters. The storage filter doesn't seem to entirely work.
			infoQueue.Get()->AddStorageFilterEntries(&filter);
			infoQueue.Get()->AddRetrievalFilterEntries(&filter);

			// Only enable debug breaks in debug builds
			if (IsDebuggerPresent())
			{
				infoQueue.Get()->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_INFO, FALSE);

				for (int i = D3D11_MESSAGE_ID_UNKNOWN + 1; i < D3D11_MESSAGE_ID_D3D10_MESSAGES_END; i++)
				{
					infoQueue.Get()->SetBreakOnID((D3D11_MESSAGE_ID)i, TRUE);
				}
				for (int i = D3D11_MESSAGE_ID_D3D10L9_MESSAGES_START + 1; i < D3D11_MESSAGE_ID_D3D10L9_MESSAGES_END; i++)
				{
					infoQueue.Get()->SetBreakOnID((D3D11_MESSAGE_ID)i, TRUE);
				}
				for (int i = D3D11_MESSAGE_ID_D3D11_MESSAGES_START + 1; i < D3D11_MESSAGE_ID_D3D11_MESSAGES_END; i++)
				{
					infoQueue.Get()->SetBreakOnID((D3D11_MESSAGE_ID)i, TRUE);
				}
				for (int i = D3D11_MESSAGE_ID_D3D11_1_MESSAGES_START + 1; i < D3D11_MESSAGE_ID_D3D11_1_MESSAGES_END; i++)
				{
					infoQueue.Get()->SetBreakOnID((D3D11_MESSAGE_ID)i, TRUE);
				}
				for (int i = 0; i < numDeniedIds; i++)
				{
					infoQueue.Get()->SetBreakOnID(deniedIds[i], FALSE);
				}
			}
		}
	}

	OutputDebugString(L"-------------------------------------------------------------------------------------------------------------\n");
	OutputDebugString(L"--------------------| Live Device Object Report |------------------------------------------------------------\n");
	OutputDebugString(L"-------------------------------------------------------------------------------------------------------------\n");
}
#endif