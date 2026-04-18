#include "stdafx.h"
#include "Graphics.h"
#include "Game/Entity.h"
#include "Game/Behaviours/Rendering/Camera/CameraBehaviour.h"
#include "Game/Behaviours/Rendering/Mesh/MeshBehaviour.h"
#include "Engine/Debug/DebugData.h"
#include "Engine/UI/UILayout.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;

Graphics::~Graphics()
{
	delete[] _lightGrid;

	if (_isSetup)
		Shutdown();
}

bool Graphics::Setup(
	bool fullscreen, const UINT width, const UINT height, const Window &window,
	ID3D11Device *&device, ID3D11DeviceContext *&immediateContext,
	ID3D11DeviceContext **deferredContexts, Content *content)
{
	if (_isSetup)
	{
		ErrMsg("Failed to set up graphics, graphics has already been set up!");
		return false;
	}

	if (!SetupD3D11(fullscreen, width, height, window.GetHWND(),
		device, immediateContext, deferredContexts,
		*_swapChain.ReleaseAndGetAddressOf(),
		*_rtv.ReleaseAndGetAddressOf(),
		*_dsTexture.ReleaseAndGetAddressOf(),
		*_dsView.ReleaseAndGetAddressOf(),
		*_uav.ReleaseAndGetAddressOf(),
		*_ndss.ReleaseAndGetAddressOf(),
		*_rdss.ReleaseAndGetAddressOf(),
		*_tdss.ReleaseAndGetAddressOf(),
		*_nulldss.ReleaseAndGetAddressOf(),
		_viewport))
	{
		ErrMsg("Failed to setup d3d11!");
		return false;
	}

#if defined(TRACY_ENABLE) && defined(TRACY_GPU)
	_tracyD3D11Context = TracyD3D11Context(device, immediateContext);
#endif

	ZoneScopedC(RandomUniqueColor());

	// Store references
	_device = device;
	_context = immediateContext;
	_content = content;


#ifdef DEBUG_BUILD
	auto &debugData = DebugData::Get();

	_emissionResolutionScale = debugData.graphicsEmissionScale;
	_fogResolutionScale = debugData.graphicsFogScale;
	_dofResolutionScale = debugData.graphicsDofScale;
	_outlineResolutionScale = debugData.graphicsOutlineScale;
#endif

	dx::XMUINT2 screenSize, sceneSize;
	screenSize = sceneSize = { width, height };

	if (!ResizeWindowBuffers(fullscreen, width, height, true))
	{
		ErrMsg("Failed to resize window buffers!");
		return false;
	}

#ifdef USE_IMGUI
	UINT sceneWidth = debugData.sceneViewSizeX;
	UINT sceneHeight = debugData.sceneViewSizeY;
	sceneSize = { sceneWidth, sceneHeight };

	if (!ResizeSceneViewBuffers(sceneWidth, sceneHeight))
	{
		ErrMsg("Failed to resize scene view buffers!");
		return false;
	}

	_sceneDsTexturePtr = _sceneDsTexture.GetAddressOf();
	_sceneDsViewPtr = _sceneDsView.GetAddressOf();
#else
	_sceneDsTexturePtr = _dsTexture.GetAddressOf();
	_sceneDsViewPtr = _dsView.GetAddressOf();
#endif

	if (!DebugDrawer::Instance().Setup(screenSize, sceneSize, _device, _context, _content))
	{
		ErrMsg("Failed to setup debug drawer!");
		return false;
	}

	// Light Grid
	{
		_lightGrid = new LightTile[LIGHT_GRID_RES * LIGHT_GRID_RES];

		ResetLightGrid();
		if (!_lightGridBuffer.Initialize(device, sizeof(LightTile), static_cast<size_t>(LIGHT_GRID_RES) * LIGHT_GRID_RES,
			true, false, true, _lightGrid))
		{
			ErrMsg("Failed to initialize light tile buffer!");
			return false;
		}
	}

	// Constant Buffers
	{
		if (!_globalLightBuffer.Initialize(device, sizeof(dx::XMFLOAT4A), &_currAmbientColor))
		{
			ErrMsg("Failed to initialize global light buffer!");
			return false;
		}

		if (!_generalDataBuffer.Initialize(device, sizeof(GeneralDataBuffer), &_generalDataSettings))
		{
			ErrMsg("Failed to initialize general data buffer!");
			return false;
		}

		if (!_fogSettingsBuffer.Initialize(device, sizeof(FogSettingsBuffer), &_currFogSettings))
		{
			ErrMsg("Failed to initialize fog settings buffer!");
			return false;
		}

		if (!_emissionSettingsBuffer.Initialize(device, sizeof(EmissionSettingsBuffer), &_currEmissionSettings))
		{
			ErrMsg("Failed to initialize emission settings buffer!");
			return false;
		}

		if (!_distortionSettingsBuffer.Initialize(device, sizeof(DistortionSettingsBuffer), &_distortionSettings))
		{
			ErrMsg("Failed to initialize distortion settings buffer!");
			return false;
		}

		if (!_depthOfFieldSettingsBuffer.Initialize(device, sizeof(DepthOfFieldSettingsBuffer), &_currDepthOfFieldSettings))
		{
			ErrMsg("Failed to initialize depth of field settings buffer!");
			return false;
		}

#ifdef DEBUG_BUILD
		if (!_outlineSettingsBuffer.Initialize(device, sizeof(OutlineSettingsBuffer), &_outlineSettings))
		{
			ErrMsg("Failed to initialize outline settings buffer!");
			return false;
		}
#endif
	}

	// Rasterizer States
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = { };
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_BACK;
		rasterizerDesc.FrontCounterClockwise = false;
		rasterizerDesc.DepthBias = 0;
		rasterizerDesc.DepthBiasClamp = 0;
		rasterizerDesc.SlopeScaledDepthBias = 0;
		rasterizerDesc.DepthClipEnable = true;
		rasterizerDesc.ScissorEnable = false;
		rasterizerDesc.MultisampleEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false;

		if (FAILED(device->CreateRasterizerState(&rasterizerDesc, &_defaultRasterizer)))
		{
			ErrMsg("Failed to create default rasterizer state!");
			return false;
		}

		rasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		//rasterizerDesc.AntialiasedLineEnable = true;

		if (FAILED(device->CreateRasterizerState(&rasterizerDesc, &_wireframeRasterizer)))
		{
			ErrMsg("Failed to create wireframe rasterizer state!");
			return false;
		}

		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_BACK; // D3D11_CULL_NONE
		rasterizerDesc.DepthBias = -1;
		rasterizerDesc.DepthBiasClamp = -0.01f;
		rasterizerDesc.SlopeScaledDepthBias = -2.0f;
		rasterizerDesc.DepthClipEnable = false;
		rasterizerDesc.AntialiasedLineEnable = false;

		if (FAILED(device->CreateRasterizerState(&rasterizerDesc, &_shadowRasterizer)))
		{
			ErrMsg("Failed to create shadow rasterizer state!");
			return false;
		}

#ifdef USE_IMGUI
		std::memcpy(&_shadowRasterizerDesc, &rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));
#endif
	}

#ifdef USE_IMGUI
	_transparentBlendDesc = { };
	_transparentBlendDesc.AlphaToCoverageEnable = false;
	_transparentBlendDesc.IndependentBlendEnable = false;
	_transparentBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	_transparentBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	_transparentBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	_transparentBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	_transparentBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	_transparentBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	_transparentBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
	_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	io.ConfigFlags = 0;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#ifdef USE_IMGUI_VIEWPORTS
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#else
	io.MouseDrawCursor = true;
#endif
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

	ImGuiStyle &style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplSDL3_InitForD3D(window.GetWindow());
	ImGui_ImplDX11_Init(device, immediateContext);

	// Load fonts in font folder
	{
		std::vector<std::pair<std::string, std::string>> fontPaths;

		// Search for all .ttf files in ASSET_PATH_FONTS
		for (const auto &entry : std::filesystem::directory_iterator(ASSET_PATH_FONTS))
		{
			const auto &path = entry.path();
			std::string filename = path.filename().string();
			std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

			if (ext != ASSET_EXT_FONT)
				continue; // Skip non-font files

			filename = filename.substr(0, filename.find_last_of('.'));
			fontPaths.emplace_back(std::make_pair(filename, path.generic_string()));
		}

		for (const auto &[name, path] : fontPaths)
		{
			ImGuiIO &io = ImGui::GetIO();
			ImFont *font = io.Fonts->AddFontFromFileTTF(path.c_str()/*, 12.0f*/);

			if (!font)
			{
				WarnF("Failed to load font from path: {}", path.c_str());
				continue;
			}

			if (!ImGuiUtils::Utils::AddFont(name, font))
			{
				DbgMsgF("Font with name {} already exists. Skipping...", name.c_str());
			}
		}
	}

	ImGui::StyleColorsDark();

	if (!UILayout::LoadLayout(DebugData::Get().layoutName))
	{
		DbgMsgF("Failed to load layout: '{}'. Fallback to default.", DebugData::Get().layoutName);
		UILayout::LoadLayout("Default");
	}
#endif

	_content = content;
	_device = device;
	_context = immediateContext;

	SetFogGaussianWeightsBuffer(_fogGaussWeights.data(), _fogGaussWeights.size());
	SetEmissionGaussianWeightsBuffer(_emissionGaussWeights.data(), _emissionGaussWeights.size());
	SetDofGaussianWeightsBuffer(_dofGaussWeights.data(), _dofGaussWeights.size());
#ifdef DEBUG_BUILD
	SetGaussianWeightsBuffer(&_outlineGaussianWeightsBuffer, _outlineGaussWeights.data(), _outlineGaussWeights.size());
#endif

#ifdef DEBUG_BUILD
	_renderFogFX = debugData.graphicsFogEnabled;
	_renderEmissionFX = debugData.graphicsEmissionEnabled;
	_renderDepthOfFieldFX = debugData.graphicsDofEnabled;
	_renderOutlineFX = debugData.graphicsOutlineEnabled;

	if (!_sceneSampler)
		SetScenePointFiltering(debugData.graphicsScenePointFiltering);
#endif

	_isSetup = true;
	return true;
}

void Graphics::Shutdown()
{
	ZoneScopedC(RandomUniqueColor());

	_swapChain.Reset();
	_rtv.Reset();
	_dsTexture.Reset();
	_dsView.Reset();
	_uav.Reset();
	_ndss.Reset();
	_rdss.Reset();
	_tdss.Reset();
	_nulldss.Reset();
	_defaultRasterizer.Reset();
	_wireframeRasterizer.Reset();
	_shadowRasterizer.Reset();
	_sceneRT.Reset();
	_depthRT.Reset();
	_emissionRT.Reset();
	_blurRT.Reset();
	_intermediateBlurRT.Reset();
	_fogRT.Reset();
	_intermediateFogRT.Reset();
	_cocRT.Reset();
	_dofSharpRT.Reset();
	_dofHalfBlur1RT.Reset();
	_dofHalfBlur2RT.Reset();
	_dofFullBlurRT.Reset();
	_lightGridBuffer.Reset();
	_globalLightBuffer.Reset();
	_generalDataBuffer.Reset();
	_fogSettingsBuffer.Reset();
	_emissionSettingsBuffer.Reset();
	_distortionSettingsBuffer.Reset();
	_depthOfFieldSettingsBuffer.Reset();
	_fogGaussianWeightsBuffer.Reset();
	_emissionGaussianWeightsBuffer.Reset();
	_dofGaussianWeightsBuffer.Reset();

#ifdef DEBUG_BUILD
	_outlineSettingsBuffer.Reset();
	_outlineGaussianWeightsBuffer.Reset();
	_outlineRT.Reset();
	_intermediateOutlineRT.Reset();
#endif

#ifdef USE_IMGUI
	if (_isSetup)
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		ImPlot::DestroyContext();
	}
	_sceneDsTexture.Reset();
	_sceneDsView.Reset();
	_intermediateRT.Reset();
#endif

#ifdef TRACY_GPU
	TracyD3D11Destroy(_tracyD3D11Context)
		_tracyD3D11Context = nullptr;
#endif

#ifdef TRACY_SCREEN_CAPTURE
	_tracyCaptureRT.Reset();
#endif

	_isSetup = false;
}

bool Graphics::ResizeWindowBuffers(bool fullscreen, UINT newWidth, UINT newHeight, bool skipResizeD3D11)
{
	ZoneScopedC(RandomUniqueColor());

	newWidth = max(newWidth, DIM_FORCED_MULTIPLE);
	newHeight = max(newHeight, DIM_FORCED_MULTIPLE);

	if (newWidth % DIM_FORCED_MULTIPLE != 0)
		newWidth -= newWidth % DIM_FORCED_MULTIPLE;
	if (newHeight % DIM_FORCED_MULTIPLE != 0)
		newHeight -= newHeight % DIM_FORCED_MULTIPLE;

	if (!skipResizeD3D11)
	{
#ifdef TRACY_SCREEN_CAPTURE
		_tracyCaptureRT.Reset();
#endif

		if (!ResizeD3D11(fullscreen, newWidth, newHeight,
			_device, _context,
			nullptr,
			*_swapChain.GetAddressOf(),
			*_rtv.ReleaseAndGetAddressOf(),
			*_dsTexture.ReleaseAndGetAddressOf(),
			*_dsView.ReleaseAndGetAddressOf(),
			*_uav.ReleaseAndGetAddressOf(),
			_viewport))
		{
			ErrMsg("Failed to resize d3d11!");
			return false;
		}
	}

#ifdef TRACY_SCREEN_CAPTURE
	float screenAspect = static_cast<float>(newWidth) / static_cast<float>(newHeight);
	_tracyCaptureWidth = min((UINT)std::ceil(TRACY_CAPTURE_WIDTH), newWidth);
	_tracyCaptureHeight = (UINT)std::ceil(_tracyCaptureWidth / screenAspect);

	// Ensure resolution is multiple of 4 for tracy capture
	_tracyCaptureWidth = (_tracyCaptureWidth / 4u) * 4u;
	_tracyCaptureHeight = (_tracyCaptureHeight / 4u) * 4u;

	D3D11_TEXTURE2D_DESC tracyCaptureDesc{};
	tracyCaptureDesc.Width = _tracyCaptureWidth;
	tracyCaptureDesc.Height = _tracyCaptureHeight;
	tracyCaptureDesc.MipLevels = 1;
	tracyCaptureDesc.ArraySize = 1;
	tracyCaptureDesc.Format = SWAPCHAIN_BUFFER_FORMAT;
	tracyCaptureDesc.SampleDesc.Count = 1;
	tracyCaptureDesc.SampleDesc.Quality = 0;
	tracyCaptureDesc.Usage = D3D11_USAGE_DEFAULT;
	tracyCaptureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tracyCaptureDesc.CPUAccessFlags = 0;
	tracyCaptureDesc.MiscFlags = 0;

	if (!_tracyCaptureRT.Initialize(_device, tracyCaptureDesc, false, true))
	{
		ErrMsg("Failed to initialize tracy capture render target!");
		return false;
	}
#endif

#ifndef USE_IMGUI
	if (!ResizeSceneViewBuffers(newWidth, newHeight))
	{
		ErrMsg("Failed to resize scene view buffers!");
		return false;
	}
#endif

	return true;
}

bool Graphics::ResizeSceneViewBuffers(UINT newWidth, UINT newHeight)
{
	ZoneScopedC(RandomUniqueColor());

	newWidth = max(newWidth, DIM_FORCED_MULTIPLE);
	newHeight = max(newHeight, DIM_FORCED_MULTIPLE);

	if (newWidth % DIM_FORCED_MULTIPLE != 0)
		newWidth -= newWidth % DIM_FORCED_MULTIPLE;
	if (newHeight % DIM_FORCED_MULTIPLE != 0)
		newHeight -= newHeight % DIM_FORCED_MULTIPLE;

#ifdef USE_IMGUI
	_intermediateRT.Reset();
#endif
	_sceneRT.Reset();
	_depthRT.Reset();
	_emissionRT.Reset();
	_blurRT.Reset();
	_intermediateBlurRT.Reset();
	_fogRT.Reset();
	_intermediateFogRT.Reset();
	_cocRT.Reset();
	_dofSharpRT.Reset();
	_dofHalfBlur1RT.Reset();
	_dofHalfBlur2RT.Reset();
	_dofFullBlurRT.Reset();
#ifdef DEBUG_BUILD
	_outlineRT.Reset();
	_intermediateOutlineRT.Reset();
#endif

	_viewportSceneView = _viewport;
	_viewportSceneView.Width = (float)newWidth;
	_viewportSceneView.Height = (float)newHeight;

	// Render Targets
	{
#ifdef USE_IMGUI
		if (!_intermediateRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, SWAPCHAIN_BUFFER_FORMAT, true, true))
		{
			ErrMsg("Failed to initialize intermediate render target!");
			return false;
		}
#endif

		DXGI_FORMAT depthFormat{};
		switch (VIEW_DEPTH_BUFFER_FORMAT)
		{
		case DXGI_FORMAT_D16_UNORM:				depthFormat = DXGI_FORMAT_R16_UNORM; break;
		case DXGI_FORMAT_D32_FLOAT:				depthFormat = DXGI_FORMAT_R32_FLOAT; break;
		default: break;
		}

		if (!_sceneRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, VIEW_BUFFER_FORMAT, true))
		{
			ErrMsg("Failed to initialize scene render target!");
			return false;
		}

		if (!_depthRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, depthFormat, true))
		{
			ErrMsg("Failed to initialize depth render target!");
			return false;
		}

		if (!_emissionRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, DXGI_FORMAT_R11G11B10_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize emission render target!");
			return false;
		}

		if (!RefreshEmissionBuffers())
		{
			ErrMsg("Failed to refresh emission buffers!");
			return false;
		}

		if (!RefreshFogBuffers())
		{
			ErrMsg("Failed to refresh fog buffers!");
			return false;
		}

		if (!RefreshDofBuffers())
		{
			ErrMsg("Failed to refresh dof buffers!");
			return false;
		}

#ifdef DEBUG_BUILD
		if (!RefreshOutlineBuffers())
		{
			ErrMsg("Failed to refresh outline buffers!");
			return false;
		}
#endif
	}

#ifdef USE_IMGUI
	// Scene depth stencil texture & view
	{
		D3D11_TEXTURE2D_DESC depthTextureDesc{};
		depthTextureDesc.Width = newWidth;
		depthTextureDesc.Height = newHeight;
		depthTextureDesc.MipLevels = 1;
		depthTextureDesc.ArraySize = 1;
		depthTextureDesc.Format = VIEW_DEPTH_BUFFER_FORMAT;
		depthTextureDesc.SampleDesc.Count = 1;
		depthTextureDesc.SampleDesc.Quality = 0;
		depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthTextureDesc.CPUAccessFlags = 0;
		depthTextureDesc.MiscFlags = 0;

		if (FAILED(_device->CreateTexture2D(&depthTextureDesc, nullptr, _sceneDsTexture.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create depth stencil texture!");
			return false;
		}

		if (FAILED(_device->CreateDepthStencilView(_sceneDsTexture.Get(), nullptr, _sceneDsView.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create depth stencil view!");
			return false;
		}
	}

	if (_currViewCamera)
	{
		float screenAspect = _viewportSceneView.Width / _viewportSceneView.Height;
		_currViewCamera->SetAspectRatio(screenAspect);
	}
#endif

	return true;
}

bool Graphics::RefreshEmissionBuffers()
{
	_viewportBlur = _viewportSceneView;
	_viewportBlur.Width = std::ceil(_viewportBlur.Width * _emissionResolutionScale);
	_viewportBlur.Height = std::ceil(_viewportBlur.Height * _emissionResolutionScale);

	if (!_blurRT.Initialize(_device, (UINT)_viewportBlur.Width, (UINT)_viewportBlur.Height, 0, 4, DXGI_FORMAT_R11G11B10_FLOAT, true))
	{
		ErrMsg("Failed to initialize blur stage two render target!");
		return false;
	}

	if (!_intermediateBlurRT.Initialize(_device, (UINT)_viewportBlur.Width, (UINT)_viewportBlur.Height, 0, 4, DXGI_FORMAT_R11G11B10_FLOAT, true))
	{
		ErrMsg("Failed to initialize blur stage one render target!");
		return false;
	}

	return true;
}

bool Graphics::RefreshFogBuffers()
{
	_viewportFog = _viewportSceneView;
	_viewportFog.Width = std::ceil(_viewportFog.Width * _fogResolutionScale);
	_viewportFog.Height = std::ceil(_viewportFog.Height * _fogResolutionScale);

	if (!_fogRT.Initialize(_device, (UINT)_viewportFog.Width, (UINT)_viewportFog.Height, DXGI_FORMAT_R16G16B16A16_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize fog stage one render target!");
		return false;
	}

	if (!_intermediateFogRT.Initialize(_device, (UINT)_viewportFog.Width, (UINT)_viewportFog.Height, DXGI_FORMAT_R16G16B16A16_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize fog stage one render target!");
		return false;
	}

	return true;
}

bool Graphics::RefreshDofBuffers()
{
	_viewportDof = _viewportSceneView;
	_viewportDof.Width = std::ceil(_viewportDof.Width * _dofResolutionScale);
	_viewportDof.Height = std::ceil(_viewportDof.Height * _dofResolutionScale);

	if (!_cocRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, DXGI_FORMAT_R16_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize dof render target!");
		return false;
	}

	if (!_dofSharpRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, SWAPCHAIN_BUFFER_FORMAT, true, true))
	{
		ErrMsg("Failed to initialize dof render target!");
		return false;
	}

	if (!_dofHalfBlur1RT.Initialize(_device, (UINT)_viewportDof.Width, (UINT)_viewportDof.Height, DXGI_FORMAT_R11G11B10_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize dof render target!");
		return false;
	}

	if (!_dofHalfBlur2RT.Initialize(_device, (UINT)_viewportDof.Width, (UINT)_viewportDof.Height, DXGI_FORMAT_R11G11B10_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize dof render target!");
		return false;
	}

	if (!_dofFullBlurRT.Initialize(_device, (UINT)_viewportSceneView.Width, (UINT)_viewportSceneView.Height, DXGI_FORMAT_R16G16B16A16_FLOAT, true, true))
	{
		ErrMsg("Failed to initialize dof render target!");
		return false;
	}

	return true;
}

#ifdef DEBUG_BUILD
bool Graphics::RefreshOutlineBuffers()
{
	_viewportOutline = _viewportSceneView;
	_viewportOutline.Width = std::ceil(_viewportOutline.Width * _outlineResolutionScale);
	_viewportOutline.Height = std::ceil(_viewportOutline.Height * _outlineResolutionScale);


	if (!_outlineRT.Initialize(_device, (UINT)_viewportOutline.Width, (UINT)_viewportOutline.Height, DXGI_FORMAT_R8_UNORM, true, true))
	{
		ErrMsg("Failed to initialize outline stage one render target!");
		return false;
	}

	if (!_intermediateOutlineRT.Initialize(_device, (UINT)_viewportOutline.Width, (UINT)_viewportOutline.Height, DXGI_FORMAT_R8_UNORM, true, true))
	{
		ErrMsg("Failed to initialize outline stage two render target!");
		return false;
	}

	return true;
}
#endif
