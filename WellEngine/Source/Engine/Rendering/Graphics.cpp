#include "stdafx.h"
#include "Rendering/Graphics.h"

#include "Entity.h"
#include "Behaviours/MeshBehaviour.h"
#include "D3D/D3D11Helper.h"
#include "Debug/DebugData.h"
#ifdef USE_IMGUI
#include "UI/UILayout.h"
#endif

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

bool Graphics::Setup(bool fullscreen, const UINT width, const UINT height, const Window &window, 
	ID3D11Device *&device, ID3D11DeviceContext *&immediateContext, 
	ID3D11DeviceContext **deferredContexts, Content *content)
{
	ZoneScopedC(RandomUniqueColor());

	if (_isSetup)
	{
		ErrMsg("Failed to set up graphics, graphics has already been set up!");
		return false;
	}
	
	if (!SetupD3D11(fullscreen, width, height, window.GetHWND(),
		device, immediateContext, deferredContexts,
		*_swapChain.ReleaseAndGetAddressOf(),
		*_rtv.ReleaseAndGetAddressOf(),
		* _dsTexture.ReleaseAndGetAddressOf(),
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

	// Store references
	_device = device;
	_context = immediateContext;
#ifdef DEFERRED_CONTEXTS
	_deferredContexts = deferredContexts;
#endif
	_content = content;

	dx::XMUINT2 screenSize, sceneSize;
	screenSize = sceneSize = { width, height };

	if (!ResizeWindowBuffers(fullscreen, width, height, true))
	{
		ErrMsg("Failed to resize window buffers!");
		return false;
	}

#ifdef USE_IMGUI
	UINT sceneWidth = DebugData::Get().sceneViewSizeX;
	UINT sceneHeight = DebugData::Get().sceneViewSizeY;
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

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplSDL3_InitForD3D(window.GetWindow());
	ImGui_ImplDX11_Init(device, immediateContext);

	ImGui::StyleColorsDark();

	UILayout::LoadLayout(DebugData::Get().layoutName);
#endif

	_content = content;
	_device = device;
	_context = immediateContext;
#ifdef DEFERRED_CONTEXTS
	_deferredContexts = deferredContexts;
#endif

	SetFogGaussianWeightsBuffer(_fogGaussWeights.data(), _fogGaussWeights.size());
	SetEmissionGaussianWeightsBuffer(_emissionGaussWeights.data(), _emissionGaussWeights.size());
#ifdef DEBUG_BUILD
	SetGaussianWeightsBuffer(&_outlineGaussianWeightsBuffer, _outlineGaussWeights.data(), _outlineGaussWeights.size());
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
	_lightGridBuffer.Reset();
	_globalLightBuffer.Reset();
	_generalDataBuffer.Reset();
	_fogSettingsBuffer.Reset();
	_emissionSettingsBuffer.Reset();
	_distortionSettingsBuffer.Reset();
	_fogGaussianWeightsBuffer.Reset();
	_emissionGaussianWeightsBuffer.Reset();

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

	if (!skipResizeD3D11)
	{
#ifdef TRACY_SCREEN_CAPTURE
		_tracyCaptureRT.Release();
#endif

		if (!ResizeD3D11(fullscreen, newWidth, newHeight,
			_device, _context,
#ifdef DEFERRED_CONTEXTS
			_deferredContexts,
#else
			nullptr,
#endif
			* _swapChain.GetAddressOf(),
			*_rtv.ReleaseAndGetAddressOf(),
			* _dsTexture.ReleaseAndGetAddressOf(),
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
	_tracyCaptureWidth = min(static_cast<UINT>(TRACY_CAPTURE_WIDTH), newWidth);
	_tracyCaptureHeight = static_cast<UINT>(_tracyCaptureWidth / screenAspect);

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
#ifdef DEBUG_BUILD
	_outlineRT.Reset();
	_intermediateOutlineRT.Reset();
#endif

	_viewportSceneView = _viewport;
	_viewportSceneView.Width = (float)newWidth;
	_viewportSceneView.Height = (float)newHeight;

	_viewportBlur = _viewportSceneView;
	_viewportBlur.Width *= 0.25f;
	_viewportBlur.Height *= 0.25f;

	_viewportFog = _viewportSceneView;
	_viewportFog.Width *= 0.25f;
	_viewportFog.Height *= 0.25f;

#ifdef DEBUG_BUILD
	_viewportOutline = _viewportSceneView;
	_viewportOutline.Width *= 0.5f;
	_viewportOutline.Height *= 0.5f;
#endif

	// Render Targets
	{
#ifdef USE_IMGUI
		if (!_intermediateRT.Initialize(_device, newWidth, newHeight, SWAPCHAIN_BUFFER_FORMAT, true, true))
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

		if (!_sceneRT.Initialize(_device, newWidth, newHeight, VIEW_BUFFER_FORMAT, true))
		{
			ErrMsg("Failed to initialize scene render target!");
			return false;
		}

		if (!_depthRT.Initialize(_device, newWidth, newHeight, depthFormat, true))
		{
			ErrMsg("Failed to initialize depth render target!");
			return false;
		}

		if (!_emissionRT.Initialize(_device, newWidth, newHeight, DXGI_FORMAT_R11G11B10_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize emission render target!");
			return false;
		}

		if (!_blurRT.Initialize(_device, static_cast<UINT>(_viewportBlur.Width), static_cast<UINT>(_viewportBlur.Height), DXGI_FORMAT_R11G11B10_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize blur stage two render target!");
			return false;
		}

		if (!_intermediateBlurRT.Initialize(_device, static_cast<UINT>(_viewportBlur.Width), static_cast<UINT>(_viewportBlur.Height), DXGI_FORMAT_R11G11B10_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize blur stage onw render target!");
			return false;
		}

		if (!_fogRT.Initialize(_device, static_cast<UINT>(_viewportFog.Width), static_cast<UINT>(_viewportFog.Height), DXGI_FORMAT_R16G16B16A16_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize fog stage one render target!");
			return false;
		}

		if (!_intermediateFogRT.Initialize(_device, static_cast<UINT>(_viewportFog.Width), static_cast<UINT>(_viewportFog.Height), DXGI_FORMAT_R16G16B16A16_FLOAT, true, true))
		{
			ErrMsg("Failed to initialize fog stage one render target!");
			return false;
		}

#ifdef DEBUG_BUILD
		if (!_outlineRT.Initialize(_device, static_cast<UINT>(_viewportOutline.Width), static_cast<UINT>(_viewportOutline.Height), DXGI_FORMAT_R8_UNORM, true, true))
		{
			ErrMsg("Failed to initialize outline stage one render target!");
			return false;
		}

		if (!_intermediateOutlineRT.Initialize(_device, static_cast<UINT>(_viewportOutline.Width), static_cast<UINT>(_viewportOutline.Height), DXGI_FORMAT_R8_UNORM, true, true))
		{
			ErrMsg("Failed to initialize outline stage two render target!");
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

bool Graphics::SetCamera(CameraBehaviour *viewCamera)
{
	if (viewCamera == nullptr)
	{
		ErrMsg("Failed to set camera, camera is nullptr!");
		return false;
	}

	_currViewCamera = viewCamera;
	return true;
}
bool Graphics::SetSpotlightCollection(SpotLightCollection *spotlights)
{
	if (spotlights == nullptr)
	{
		ErrMsg("Failed to set spotlight collection, collection is nullptr!");
		return false;
	}

	_currSpotLightCollection = spotlights;
	return true;
}
bool Graphics::SetPointlightCollection(PointLightCollection *pointlights)
{
	if (pointlights == nullptr)
	{
		ErrMsg("Failed to set pointlight collection, collection is nullptr!");
		return false;
	}

	_currPointLightCollection = pointlights;
	return true;
}

#ifdef DEBUG_BUILD
void Graphics::AddOutlinedEntity(Entity *entity)
{
	if (entity == nullptr)
		return;

	std::vector<Entity *> entities;
	entity->GetChildrenRecursive(entities);
	entities.push_back(entity);

	for (int i = 0; i < entities.size(); i++)
	{
		// Check if entity is already in the list
		for (const auto &e : _outlinedEntities)
		{
			if (e.Get() != entity)
				continue;

			// Already added
			entities.erase(entities.begin() + i);
			i--;
		}
	}

	for (int i = 0; i < entities.size(); i++)
		_outlinedEntities.emplace_back(*entities[i]);
}
#endif

void Graphics::ResetLightGrid()
{
	ZoneScoped;
	for (UINT i = 0; i < LIGHT_GRID_RES * LIGHT_GRID_RES; i++)
	{
		_lightGrid[i].spotlightCount = 0;
		_lightGrid[i].pointlightCount = 0;
		_lightGrid[i].simpleSpotlightCount = 0;
		_lightGrid[i].simplePointlightCount = 0;
	}
}
void Graphics::AddLightToTile(UINT tileIndex, UINT lightIndex, LightType type)
{
	ZoneScopedX;
	LightTile &tile = _lightGrid[tileIndex];

	switch (type)
	{
	case SPOTLIGHT:
		if (tile.spotlightCount >= MAX_LIGHTS)
			return;
		
		tile.spotlights[tile.spotlightCount++] = lightIndex;
		break;

	case POINTLIGHT:
		if (tile.pointlightCount >= MAX_LIGHTS)
			return;

		tile.pointlights[tile.pointlightCount++] = lightIndex;
		break;

	case SIMPLE_SPOTLIGHT:
		if (tile.simpleSpotlightCount >= MAX_LIGHTS)
			return;

		tile.simpleSpotlights[tile.simpleSpotlightCount++] = lightIndex;
		break;

	case SIMPLE_POINTLIGHT:
		if (tile.simplePointlightCount >= MAX_LIGHTS)
			return;

		tile.simplePointlights[tile.simplePointlightCount++] = lightIndex;
		break;
	}
}


void Graphics::BeginScreenFade(float duration)
{
	if (_currViewCamera)
		_currViewCamera->BeginScreenFade(duration);
}
void Graphics::SetScreenFadeManual(float amount)
{
	if (_currViewCamera)
		_currViewCamera->SetScreenFadeManual(amount);
}
float Graphics::GetScreenFadeAmount() const
{
	if (_currViewCamera)
		return _currViewCamera->GetScreenFadeAmount();
	return 0.0f;
}
float Graphics::GetScreenFadeRate() const
{
	if (_currViewCamera)
		return _currViewCamera->GetScreenFadeRate();
	return 0.0f;
}

bool Graphics::GetRenderTransparent() const
{
	return _renderTransparency;
}
bool Graphics::GetRenderOverlay() const
{
	return _renderOverlay;
}
bool Graphics::GetRenderPostFX() const
{
	return _renderPostFX;
}

ID3D11DepthStencilState *Graphics::GetCurrentDepthStencilState()
{
	return _currViewCamera->GetInverted() ? _rdss.Get() : _ndss.Get();
}

void Graphics::SetDistortionOrigin(const dx::XMFLOAT3A &origin)
{
	_distortionSettings.distortionOrigin = origin;
}
void Graphics::SetDistortionStrength(float strength)
{
	_distortionSettings.distortionStrength = strength;
}

void Graphics::SetGaussianWeightsBuffer(StructuredBufferD3D11 *buffer, float *const weights, UINT count)
{
	ZoneScopedC(RandomUniqueColor());

	if (buffer == nullptr || weights == nullptr || count == 0)
	{
		ErrMsg("Failed to set gaussian weights buffer, invalid parameters!");
		return;
	}

	if (!buffer->Initialize(_device, sizeof(float), count, true, false, false, weights))
	{
		ErrMsg("Failed to initialize gaussian weights buffer!");
		return;
	}
}
void Graphics::SetFogGaussianWeightsBuffer(float *const weights, UINT count)
{
	SetGaussianWeightsBuffer(&_fogGaussianWeightsBuffer, weights, count);
}
void Graphics::SetEmissionGaussianWeightsBuffer(float *const weights, UINT count)
{
	SetGaussianWeightsBuffer(&_emissionGaussianWeightsBuffer, weights, count);
}

FogSettingsBuffer Graphics::GetFogSettings() const
{
	return _currFogSettings;
}
EmissionSettingsBuffer Graphics::GetEmissionSettings() const
{
	return _currEmissionSettings;
}
dx::XMFLOAT3 Graphics::GetAmbientColor() const
{
	return To3(_currAmbientColor);
}
UINT Graphics::GetSkyboxShaderID() const
{
	return _skyboxPsID;
}
UINT Graphics::GetEnvironmentCubemapID() const
{
	return _environmentCubemapID;
}

void Graphics::SetFogSettings(const FogSettingsBuffer &fogSettings)
{
	_currFogSettings = fogSettings;
}
void Graphics::SetEmissionSettings(const EmissionSettingsBuffer &emissionSettings)
{
	_currEmissionSettings = emissionSettings;
}
void Graphics::SetAmbientColor(const dx::XMFLOAT3 &color)
{
	_currAmbientColor.x = color.x;
	_currAmbientColor.y = color.y;
	_currAmbientColor.z = color.z;
}
void Graphics::SetSkyboxShaderID(UINT shaderID)
{
	if (shaderID == CONTENT_NULL)
	{
		_skyboxPsID = CONTENT_NULL;
		return;
	}

	std::string shaderName = _content->GetShaderName(shaderID);
	if (shaderName == "Uninitialized")
	{
		_skyboxPsID = CONTENT_NULL;
		return;
	}

	if (!shaderName.starts_with("PS_Skybox"))
	{
		Warn(std::format("Failed to set skybox shader ID, shader '{}' is not a skybox pixel shader!", shaderName));
		return;
	}

	_skyboxPsID = shaderID;
}
void Graphics::SetEnvironmentCubemapID(UINT cubemapID)
{
	if (cubemapID == CONTENT_NULL)
	{
		_environmentCubemapID = CONTENT_NULL;
		return;
	}

	std::string cubemapName = _content->GetCubemapName(cubemapID);
	if (cubemapName == "Uninitialized")
	{
		_environmentCubemapID = CONTENT_NULL;
		return;
	}

	ShaderResourceTextureD3D11 *cubemapSRT = _content->GetCubemap(cubemapID);
	if (!cubemapSRT->IsCubemap())
		return;

	_environmentCubemapID = cubemapID;
}


bool Graphics::BeginSceneRender()
{
	ZoneScopedC(RandomUniqueColor());

	if (!_isSetup)
	{
		ErrMsg("Failed to begin rendering, graphics has not been set up!");
		return false;
	}

	if (_isRendering)
	{
		ErrMsg("Failed to begin rendering, rendering has already begun!");
		return false;
	}

	// Update buffers if resized window
	auto &input = Input::Instance();
	auto *wnd = input.GetWindow();
	bool didResize = false;

	if (wnd->IsDirty())
	{
		bool fullscreen = wnd->IsFullscreen();
		UINT newWidth = wnd->GetPhysicalWidth();
		UINT newHeight = wnd->GetPhysicalHeight();

		if (!ResizeWindowBuffers(fullscreen, newWidth, newHeight))
		{
			ErrMsg("Failed to resize window buffers!");
			return false;
		}

		didResize = true;
	}

#ifdef USE_IMGUI
	if (input.HasResizedSceneView())
	{
		dx::XMUINT2 newSize = input.GetSceneRenderSize();

		if (!ResizeSceneViewBuffers(newSize.x, newSize.y))
		{
			ErrMsg("Failed to resize scene view buffers!");
			return false;
		}
		
		didResize = true;
	}
#endif

	if (didResize)
	{
		dx::XMUINT2 screenSize, sceneSize;
		screenSize = sceneSize = wnd->GetPhysicalSize();

#ifdef USE_IMGUI
		sceneSize = input.GetSceneRenderSize();
#endif

		if (!DebugDrawer::Instance().Setup(screenSize, sceneSize, _device, _context, _content))
		{
			ErrMsg("Failed to update debug drawer!");
			return false;
		}
	}

	_isRendering = true;
	return true;
}
bool Graphics::EndSceneRender(TimeUtils &time)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Render", RandomUniqueColor());

	// Update buffers
	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, updateBuffersD3D11Zone, "Update Buffers", RandomUniqueColor(), true);

		if (!_isRendering)
		{
			ErrMsg("Failed to end rendering, rendering has not begun!");
			return false;
		}

		if (!_lightGridBuffer.UpdateBuffer(_context, _lightGrid))
		{
			ErrMsg("Failed to update light grid buffer!");
			return false;
		}

		_currAmbientColor.w = GetScreenFadeAmount();
		if (!_globalLightBuffer.UpdateBuffer(_context, &_currAmbientColor))
		{
			ErrMsg("Failed to update global light buffer!");
			return false;
		}

		_generalDataSettings.time = time.GetTime();
		_generalDataSettings.deltaTime = time.GetDeltaTime();
		_generalDataSettings.randInt = rand();
		_generalDataSettings.randNorm = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

		if (!_generalDataBuffer.UpdateBuffer(_context, &_generalDataSettings))
		{
			ErrMsg("Failed to update general data buffer!");
			return false;
		}

		if (!_fogSettingsBuffer.UpdateBuffer(_context, &_currFogSettings))
		{
			ErrMsg("Failed to update fog settings buffer!");
			return false;
		}
		
		if (!_emissionSettingsBuffer.UpdateBuffer(_context, &_currEmissionSettings))
		{
			ErrMsg("Failed to update emission settings buffer!");
			return false;
		}

		if (!_distortionSettingsBuffer.UpdateBuffer(_context, &_distortionSettings))
		{
			ErrMsg("Failed to update distortion settings buffer!");
			return false;
		}
	
#ifdef DEBUG_BUILD
		if (!_outlineSettingsBuffer.UpdateBuffer(_context, &_outlineSettings))
		{
			ErrMsg("Failed to update outline settings buffer!");
			return false;
		}
#endif	
	}

	// Bind default resources
	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, bindDefaultResourcesD3D11Zone, "Bind Default Resources", RandomUniqueColor(), true);

		_context->OMSetDepthStencilState(GetCurrentDepthStencilState(), 0);

		// Bind vertex distortion settings
		ID3D11Buffer *const distortionSettings = _distortionSettingsBuffer.GetBuffer();
		_context->VSSetConstantBuffers(2, 1, &distortionSettings);

		// Bind general data for vertex shader
		ID3D11Buffer *const generalDataBuf = _generalDataBuffer.GetBuffer();
		_context->VSSetConstantBuffers(5, 1, &generalDataBuf);

		// Bind noise sampler and texture for vertex and pixel shaders
		static UINT noiseSamplerID = _content->GetSamplerID("Wrap");
		ID3D11SamplerState *const ss = _content->GetSampler(noiseSamplerID)->GetSamplerState();
		_context->VSSetSamplers(0, 1, &ss);

		static UINT noiseMapID = _content->GetTextureID("Noise");
		ID3D11ShaderResourceView *srv = _content->GetTexture(noiseMapID)->GetSRV();
		_context->VSSetShaderResources(10, 1, &srv);
		_context->PSSetShaderResources(10, 1, &srv);

		ID3D11ShaderResourceView *const cubemap = _content->GetCubemap(_environmentCubemapID)->GetSRV();
		_context->PSSetShaderResources(20, 1, &cubemap);
	}

	if (!RenderShadowCasters())
	{
		ErrMsg("Failed to render shadow casters!");
		return false;
	}

#ifdef DEBUG_BUILD
	if (_renderOutlineFX && _renderPostFX)
	{
		if (!RenderOutlinedGeometry())
		{
			ErrMsg("Failed to render outlined geometry!");
			return false;
		}
	}
#endif

	// Render main camera to screen view
	_renderOutput = (RenderType)((int)_renderOutput % (int)RenderType::COUNT);
	if (!RenderToTarget(nullptr, nullptr, nullptr, nullptr))
	{
		ErrMsg("Failed to render to screen view!");
		return false;
	}

	return true;
}

bool Graphics::RenderToTarget(
	ID3D11RenderTargetView *targetRTV, ID3D11RenderTargetView *targetDepthRTV,
	ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Render To Target", RandomUniqueColor());

#ifdef USE_IMGUI
	if (targetRTV == nullptr)		targetRTV = _intermediateRT.GetRTV();
#else
	if (targetRTV == nullptr)		targetRTV = _rtv.Get();
#endif
	if (targetDepthRTV == nullptr)	targetDepthRTV = _depthRT.GetRTV();
	if (targetDSV == nullptr)		targetDSV = *_sceneDsViewPtr;
	if (targetViewport == nullptr)	targetViewport = &_viewportSceneView;

	switch (_renderOutput)
	{
	case RenderType::DEFAULT:
		if (!RenderOpaque(_sceneRT.GetRTV(), targetDepthRTV, targetDSV, targetViewport))
		{
			ErrMsg("Failed to render opaque!");
			return false;
		}
		
		if (_renderTransparency)
		{
			if (!RenderTransparency(_sceneRT.GetRTV(), targetDSV, targetViewport))
			{
				ErrMsg("Failed to render transparency!");
				return false;
			}
		}

		if (!RenderPostFX())
		{
			ErrMsg("Failed to render post effects!");
			return false;
		}
		
		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		// Overlay
		if (_renderOverlay)
		{
			if (!RenderOpaque(targetRTV, targetDepthRTV, targetDSV, targetViewport, true))
			{
				ErrMsg("Failed to render opaque!");
				return false;
			}
		}
		break;

	case RenderType::POSITION:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewPosition"))
		{
			ErrMsg("Failed to render position view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewPosition", true))
			{
				ErrMsg("Failed to render position view!");
				return false;
			}
		break;

	case RenderType::NORMAL:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewNormal"))
		{
			ErrMsg("Failed to render normal view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewNormal", true))
			{
				ErrMsg("Failed to render normal view!");
				return false;
			}
		break;

	case RenderType::AMBIENT:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewAmbient"))
		{
			ErrMsg("Failed to render ambient view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewAmbient", true))
				return false;
		break;

	case RenderType::DIFFUSE:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewDiffuse"))
		{
			ErrMsg("Failed to render diffuse view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewDiffuse", true))
				return false;
		break;

	case RenderType::DEPTH:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewDepth"))
		{
			ErrMsg("Failed to render depth view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewDepth", true))
				return false;
		break;

	case RenderType::SHADOW:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewShadow"))
		{
			ErrMsg("Failed to render shadow view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewShadow", true))
				return false;
		break;

	case RenderType::REFLECTION:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewReflection"))
		{
			ErrMsg("Failed to render reflection view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewReflection", true))
				return false;
		break;

	case RenderType::REFLECTIVITY:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewReflectivity"))
		{
			ErrMsg("Failed to render reflectivity view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewReflectivity", true))
				return false;
		break;

	case RenderType::SPECULAR:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewSpecular"))
		{
			ErrMsg("Failed to render specular view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewSpecular", true))
				return false;
		break;

	case RenderType::SPECULAR_STRENGTH:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewSpecStr"))
		{
			ErrMsg("Failed to render specular strength view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewSpecStr", true))
				return false;
		break;

	case RenderType::UV_COORDS:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewUVCoords"))
		{
			ErrMsg("Failed to render UV coordinate view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewUVCoords", true))
				return false;
		break;

	case RenderType::OCCLUSION:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewOcclusion"))
		{
			ErrMsg("Failed to render occlusion view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewOcclusion", true))
				return false;
		break;

	case RenderType::TRANSPARENCY:
		_context->ClearRenderTargetView(targetRTV, &_currAmbientColor.x);

		if (!RenderTransparency(targetRTV, targetDSV, targetViewport))
		{
			ErrMsg("Failed to render transparency view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();
		break;

	case RenderType::LIGHT_TILES:
		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewLightTiles"))
		{
			ErrMsg("Failed to render light tile view!");
			return false;
		}

		if (_renderDebugDraw)
		{
			if (!DebugDrawer::Instance().Render(targetRTV, targetDSV, targetViewport))
			{
				ErrMsg("Failed to render debug drawer!");
				return false;
			}
		}
		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewLightTiles", true))
				return false;
		break;

	default:
		ErrMsg("Invalid render type!");
		return false;
	}

	_currInputLayoutID = CONTENT_NULL;
	_currMeshID = CONTENT_NULL;
	_currVsID = CONTENT_NULL;
	_currPsID = CONTENT_NULL;
	_currTexID = CONTENT_NULL;
	_currNormalID = CONTENT_NULL;
	_currSpecularID = CONTENT_NULL;
	_currGlossinessID = CONTENT_NULL;
	_currAmbientID = CONTENT_NULL;
	_currSamplerID = CONTENT_NULL;
	_currBlendStateID = CONTENT_NULL;

	return true;
}

bool Graphics::RenderSpotlights()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Spot Lights", RandomUniqueColor());

	SpotLightCollection *collection = nullptr;
	if (!_currSpotLightCollection.TryGet(collection))
	{
		ErrMsg("Failed to render spotlights, current spotlight collection is nullptr!");
		return false;
	}

	// Used to compare if the mesh uses the distortion shader
	const UINT vsNoDistID = _content->GetShaderID("VS_Geometry");

	const UINT vsDepthID = _content->GetShaderID("VS_Depth");
	const UINT vsDepthDistID = _content->GetShaderID("VS_DepthDistortion");
	if (_currVsID != vsDepthDistID)
	{
		if (!_content->GetShader(vsDepthDistID)->BindShader(_context))
		{
			ErrMsg("Failed to bind depth-stage vertex shader!");
			return false;
		}
		_currVsID = vsDepthDistID;
	}

	_context->RSSetViewports(1, &collection->GetViewport());

	_currMeshID = CONTENT_NULL;
	const MeshD3D11 *loadedMesh = nullptr;

	auto camPos = Load(_currViewCamera->GetTransform()->GetPosition(World));

	const UINT spotLightCount = collection->GetNrOfLights();
	for (UINT spotlight_i = 0; spotlight_i < spotLightCount; spotlight_i++)
	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, renderSpotLightD3D11Zone, "Spotlight", RandomUniqueColor(), true);

		// Skip rendering if disabled
		if (!collection->GetLightEnabled(spotlight_i))
			continue;

		auto lightBehaviour = collection->GetLightBehaviour(spotlight_i);

		// Shadows don't need to update every frame
		if (!lightBehaviour->DoUpdate())
			continue;

		auto camPos = Load(lightBehaviour->GetTransform()->GetPosition(World));
		auto camDir = Load(lightBehaviour->GetTransform()->GetForward(World));

		ID3D11DepthStencilView *dsView = collection->GetShadowMapDSV(spotlight_i);
		_context->ClearDepthStencilView(dsView, D3D11_CLEAR_DEPTH, 0.0f, 0);
		_context->OMSetRenderTargets(0, nullptr, dsView);

		// Bind shadow-camera data
		CameraBehaviour *spotlightCamera = lightBehaviour->GetShadowCamera();

		if (!spotlightCamera->BindShadowCasterBuffers())
		{
			ErrMsgF("Failed to bind shadow-camera buffers for spotlight #{}!", spotlight_i);
			return false;
		}

		auto &geometryQueue = spotlightCamera->GetGeometryQueue();
		auto &transparentQueue = spotlightCamera->GetTransparentQueue();

		std::vector<RenderQueueEntry> queue;
		queue.reserve(geometryQueue.size() + transparentQueue.size());
		queue.insert(queue.end(), geometryQueue.begin(), geometryQueue.end());
		queue.insert(queue.end(), transparentQueue.begin(), transparentQueue.end());

		UINT entity_i = 0;
		for (const RenderQueueEntry &entry : queue)
		{
			TracyD3D11NamedZoneXC(_tracyD3D11Context, renderShadowCasterMeshD3D11Zone, "Shadowcaster Mesh", RandomUniqueColor(), true);

			const auto &instance = entry.instance;
			const auto &resources = entry.resourceGroup;

			if (!resources.shadowCaster)
				continue;

			MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(instance.subject);

			if (!meshBehaviour)
			{
				ErrMsgF("Skipping depth-rendering for non-mesh #{}!", entity_i);
				return false;
			}

			// Bind shared entity data, skip data irrelevant for shadow mapping
			if (_currMeshID != resources.meshID)
			{
				loadedMesh = _content->GetMesh(resources.meshID);
				if (!loadedMesh->BindMeshBuffers(_context))
				{
					ErrMsgF("Failed to bind mesh buffers for instance #{}!", entity_i);
					return false;
				}
				_currMeshID = resources.meshID;
			}
			
			const UINT vsID = resources.material->vsID == vsNoDistID ? vsDepthID : vsDepthDistID;
			if (_currVsID != vsID)
			{
				ShaderD3D11 *vs = _content->GetShader(vsID);
				if (!vs)
				{
					ErrMsgF("Failed to get vertex shader #{} for instance #{}!", vsID, entity_i);
					return false;
				}

				if (!vs->BindShader(_context))
				{
					ErrMsgF("Failed to bind vertex shader #{} for instance #{}!", vsID, entity_i);
					return false;
				}
				_currVsID = vsID;
			}

			// Bind private entity data
			if (!meshBehaviour->InitialBindBuffers(_context))
			{
				ErrMsgF("Failed to bind private buffers for instance #{}!", entity_i);
				return false;
			}

			// Perform draw calls
			if (loadedMesh == nullptr)
			{
				ErrMsgF("Failed to perform draw call for instance #{}, loadedMesh is nullptr!", entity_i);
				return false;
			}

			const UINT subMeshCount = loadedMesh->GetNrOfSubMeshes();

			UINT lodIndex = 0;
			if (subMeshCount > 1)
			{
				// Mesh has LODs, determine which one to use.
				ZoneNamedXNC(getMeshLODZone, "Calculate LOD", RandomUniqueColor(), true);

				// Get the mesh center from the bounding box.
				dx::BoundingOrientedBox bounds;
				meshBehaviour->StoreBounds(bounds);
				auto meshPos = Load(bounds.Center);

				auto toMesh = dx::XMVector3Normalize(dx::XMVectorSubtract(meshPos, camPos));
				float dot = dx::XMVectorGetX(dx::XMVector3Dot(toMesh, camDir));

				// Get the distance to the camera.
				auto distVec = dx::XMVector3LengthEst(dx::XMVectorSubtract(camPos, meshPos));
				float dist = dx::XMVectorGetX(distVec);

				// Calculate the scaled LOD distances.
				float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
				float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

				CameraPlanes camPlanes = _currViewCamera->GetPlanes();
				float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
				float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

				// Get the LOD as a normalized float.
				float clampedDist = std::clamp(dist, lodDistMin, lodDistMax);
				float normalizedDist = (clampedDist - lodDistMin) * (1.0f / (lodDistMax - lodDistMin));

				// Get the LOD index.
				lodIndex = static_cast<UINT>(normalizedDist * (subMeshCount - 1));
				if (dot < 0.0f)
					lodIndex = min(lodIndex + 1, subMeshCount - 1);
			}

			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, renderShadowCasterSubmeshD3D11Zone, "Shadowcaster Submesh", RandomUniqueColor(), true);

				if (!loadedMesh->PerformSubMeshDrawCall(_context, lodIndex))
				{
					ErrMsgF("Failed to perform draw call for instance #{}, sub mesh #{}!", entity_i, lodIndex);
					return false;
				}
			}

			entity_i++;
		}
	}

	return true;
}
bool Graphics::RenderPointlights()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Point Lights", RandomUniqueColor());

	PointLightCollection *collection = nullptr;
	if (!_currPointLightCollection.TryGet(collection))
	{
		ErrMsg("Failed to render pointlights, current pointlight collection is nullptr!");
		return false;
	}

	// Used to compare if the mesh uses the distortion shader
	const UINT vsNoDistID = _content->GetShaderID("VS_Geometry");

	const UINT vsDepthID = _content->GetShaderID("VS_DepthCubemap");
	const UINT vsDepthDistID = _content->GetShaderID("VS_DepthDistortionCubemap");
	if (_currVsID != vsDepthDistID)
	{
		if (!_content->GetShader(vsDepthDistID)->BindShader(_context))
		{
			ErrMsg("Failed to bind depth-stage vertex shader!");
			return false;
		}
		_currVsID = vsDepthDistID;
	}

	if (!_content->GetShader("GS_DepthCubemap")->BindShader(_context))
	{
		ErrMsg("Failed to bind depth-stage geometry shader!");
		return false;
	}

	const UINT psDepthID = _content->GetShaderID("PS_DepthCubemap");
	if (_currPsID != psDepthID)
	{
		if (!_content->GetShader(psDepthID)->BindShader(_context))
		{
			ErrMsg("Failed to bind depth-stage pixel shader!");
			return false;
		}
		_currPsID = psDepthID;
	}

	_context->RSSetViewports(1, &collection->GetViewport());

	_currMeshID = CONTENT_NULL;
	const MeshD3D11 *loadedMesh = nullptr;

	auto camPos = Load(_currViewCamera->GetTransform()->GetPosition(World));

	const UINT pointlightCount = collection->GetNrOfLights();
	for (UINT pointlight_i = 0; pointlight_i < pointlightCount; pointlight_i++)
	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, renderPointLightD3D11Zone, "Pointlight", RandomUniqueColor(), true);

		auto lightBehaviour = collection->GetLightBehaviour(pointlight_i);

		// Shadows don't need to update every frame
		if (!lightBehaviour->DoUpdate())
			continue;

		auto camPos = Load(lightBehaviour->GetTransform()->GetPosition(World));
		auto camDir = Load(lightBehaviour->GetTransform()->GetForward(World));

			// Skip rendering if disabled
		if (!collection->GetLightEnabled(pointlight_i))
			continue;

		ID3D11DepthStencilView *dsView = collection->GetShadowMapDSV(pointlight_i);
		_context->ClearDepthStencilView(dsView, D3D11_CLEAR_DEPTH, 0.0, 0);
		_context->OMSetRenderTargets(0, nullptr, dsView);

		// Bind shadow-camera data
		CameraCubeBehaviour *pointlightCamera = lightBehaviour->GetShadowCameraCube();

		if (!pointlightCamera->BindShadowCasterBuffers())
		{
			ErrMsgF("Failed to bind shadow-camera buffers for pointlight #{}!", pointlight_i);
			return false;
		}

		auto &geometryQueue = pointlightCamera->GetGeometryQueue();
		auto &transparentQueue = pointlightCamera->GetTransparentQueue();

		std::vector<RenderQueueEntry> queue;
		queue.reserve(geometryQueue.size() + transparentQueue.size());
		queue.insert(queue.end(), geometryQueue.begin(), geometryQueue.end());
		queue.insert(queue.end(), transparentQueue.begin(), transparentQueue.end());

		UINT entity_i = 0;
		for (const RenderQueueEntry &entry : queue)
		{
			TracyD3D11NamedZoneXC(_tracyD3D11Context, renderShadowCasterMeshD3D11Zone, "Shadowcaster Mesh", RandomUniqueColor(), true);

			const auto &instance = entry.instance;
			const auto &resources = entry.resourceGroup;

			if (!resources.shadowCaster)
				continue;

			MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(instance.subject);

			if (!meshBehaviour)
			{
				ErrMsgF("Skipping depth-rendering for non-mesh #{}!", entity_i);
				return false;
			}

			// Bind shared entity data, skip data irrelevant for shadow mapping
			if (_currMeshID != resources.meshID)
			{
				loadedMesh = _content->GetMesh(resources.meshID);
				if (!loadedMesh->BindMeshBuffers(_context))
				{
					ErrMsgF("Failed to bind mesh buffers for instance #{}!", entity_i);
					return false;
				}
				_currMeshID = resources.meshID;
			}

			const UINT vsID = resources.material->vsID == vsNoDistID ? vsDepthID : vsDepthDistID;
			if (_currVsID != vsID)
			{
				ShaderD3D11 *vs = _content->GetShader(vsID);
				if (!vs)
				{
					ErrMsgF("Failed to get vertex shader #{} for instance #{}!", vsID, entity_i);
					return false;
				}

				if (!vs->BindShader(_context))
				{
					ErrMsgF("Failed to bind vertex shader #{} for instance #{}!", vsID, entity_i);
					return false;
				}
				_currVsID = vsID;
			}

			// Bind private entity data
			if (!meshBehaviour->InitialBindBuffers(_context))
			{
				ErrMsgF("Failed to bind private buffers for instance #{}!", entity_i);
				return false;
			}

			// Perform draw calls
			if (loadedMesh == nullptr)
			{
				ErrMsgF("Failed to perform draw call for instance #{}, loadedMesh is nullptr!", entity_i);
				return false;
			}

			const UINT subMeshCount = loadedMesh->GetNrOfSubMeshes(); UINT lodIndex = 0;
			if (subMeshCount > 1)
			{
				// Mesh has LODs, determine which one to use.
				ZoneNamedXNC(getMeshLODZone, "Calculate LOD", RandomUniqueColor(), true);

				// Get the mesh center from the bounding box.
				dx::BoundingOrientedBox bounds;
				meshBehaviour->StoreBounds(bounds);
				auto meshPos = Load(bounds.Center);

				auto toMesh = dx::XMVector3Normalize(dx::XMVectorSubtract(meshPos, camPos));
				float dot = dx::XMVectorGetX(dx::XMVector3Dot(toMesh, camDir));

				// Get the distance to the camera.
				auto distVec = dx::XMVector3LengthEst(dx::XMVectorSubtract(camPos, meshPos));
				float dist = dx::XMVectorGetX(distVec);

				// Calculate the scaled LOD distances.
				float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
				float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

				CameraPlanes camPlanes = _currViewCamera->GetPlanes();
				float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
				float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

				// Get the LOD as a normalized float.
				float clampedDist = std::clamp(dist, lodDistMin, lodDistMax);
				float normalizedDist = (clampedDist - lodDistMin) * (1.0f / (lodDistMax - lodDistMin));

				// Get the LOD index.
				lodIndex = static_cast<UINT>(normalizedDist * (subMeshCount - 1));
				if (dot < 0.0f)
					lodIndex = min(lodIndex + 1, subMeshCount - 1);
			}

			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, renderShadowCasterSubmeshD3D11Zone, "Shadowcaster Submesh", RandomUniqueColor(), true);

				if (!loadedMesh->PerformSubMeshDrawCall(_context, lodIndex))
				{
					ErrMsgF("Failed to perform draw call for instance #{}, sub mesh #{}!", entity_i, lodIndex);
					return false;
				}
			}

			entity_i++;
		}
	}

	_context->GSSetShader(nullptr, nullptr, 0);
	_context->PSSetShader(nullptr, nullptr, 0);

	return true;
}
bool Graphics::RenderShadowCasters()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Shadow Casters", RandomUniqueColor());

	// Bind depth stage resources
	const UINT ilID = _content->GetInputLayoutID("Fallback");
	if (_currInputLayoutID != ilID)
	{
		_context->IASetInputLayout(_content->GetInputLayout(ilID)->GetInputLayout());
		_currInputLayoutID = ilID;
	}

	_context->PSSetShader(nullptr, nullptr, 0);
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_context->RSSetState(_shadowRasterizer.Get());

	if (!RenderSpotlights())
	{
		ErrMsg("Failed to render spotlights!");
		return false;
	}

	if (!RenderPointlights())
	{
		ErrMsg("Failed to render pointlights!");
		return false;
	}

	// Unbind render target
	static constexpr ID3D11RenderTargetView* nullViews [] = { nullptr };
	_context->OMSetRenderTargets(1, nullViews, 0);

	_context->RSSetState(_defaultRasterizer.Get());

	return true;
}

#ifdef DEBUG_BUILD
bool Graphics::RenderOutlinedGeometry()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Outlined Geometry", RandomUniqueColor());
	
	// Clear outline render target
	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, clearRTVsD3D11Zone, "Clear Targets", RandomUniqueColor(), true);

		constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_outlineRT.GetRTV(), clearColor);
	}

	// Bind outline render target
	ID3D11RenderTargetView *rtvs[1] = {
		_outlineRT.GetRTV()
	};
	_context->OMSetRenderTargets(1, rtvs, nullptr);

	const UINT ilID = _content->GetInputLayoutID("Fallback");
	if (_currInputLayoutID != ilID)
	{
		_context->IASetInputLayout(_content->GetInputLayout(ilID)->GetInputLayout());
		_currInputLayoutID = ilID;
	}

	const ShaderD3D11 *outlinePS = _content->GetShader("PS_SelectionOutline");
	if (!outlinePS->BindShader(_context))
	{
		ErrMsg("Failed to bind outline pixel shader!");
		return false;
	}

	_context->RSSetViewports(1, &_viewportOutline);
	_context->RSSetState(_defaultRasterizer.Get());
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Bind general buffers
	{
		// Bind camera data
		if (!_currViewCamera->BindViewBuffers())
		{
			ErrMsg("Failed to bind view camera buffers!");
			return false;
		}

		// Bind global light data
		ID3D11Buffer *const globalLightBuffer = _globalLightBuffer.GetBuffer();
		_context->PSSetConstantBuffers(0, 1, &globalLightBuffer);

		// Bind general data
		ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
		_context->PSSetConstantBuffers(5, 1, &generalData);

		static ID3D11SamplerState *const ss = _content->GetSampler("Clamp")->GetSamplerState();
		static ID3D11SamplerState *const ssShadow = _content->GetSampler("Shadow")->GetSamplerState();
		static ID3D11SamplerState *const ssShadowCube = _content->GetSampler("ShadowCube")->GetSamplerState();
		static ID3D11SamplerState *const ssTest = _content->GetSampler("Test")->GetSamplerState();
		static ID3D11SamplerState *const ssHQ = _content->GetSampler("HQ")->GetSamplerState();
		static ID3D11SamplerState *const ssArray[5] = { ss, ssShadow, ssShadowCube, ssTest, ssHQ };
		_context->PSSetSamplers(0, 5, ssArray);

		// Bind camera lighting data
		if (!_currViewCamera->BindPSLightingBuffers())
		{
			ErrMsg("Failed to bind camera buffers!");
			return false;
		}
	}

	// Bind resouces & perform drawcalls
	{
		static UINT defaultVsID = _content->GetShaderID("VS_GeometryDistortion");
		static UINT defaultSamplerID = _content->GetSamplerID("Fallback");
		static UINT defaultAmbientID = _content->GetTextureID("Ambient");

		if (!_content->GetShader(defaultVsID)->BindShader(_context))
		{
			ErrMsg("Failed to bind geometry vertex shader!");
			return false;
		}
		_currVsID = defaultVsID;

		if (_currSamplerID != defaultSamplerID)
		{
			ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
			_context->PSSetSamplers(0, 1, &ss);
			_currSamplerID = defaultSamplerID;
		}

		const MeshD3D11 *loadedMesh = nullptr;
		ID3D11ShaderResourceView *srv;
		auto camPos = Load(_currViewCamera->GetTransform()->GetPosition(World));

		const auto &geoQueue = _currViewCamera->GetGeometryQueue();
		const auto &overlayQueue = _currViewCamera->GetOverlayQueue();
		const auto &transQueue = _currViewCamera->GetTransparentQueue();

		int reserveSize = geoQueue.size();
		if (_renderOverlay) reserveSize += overlayQueue.size();
		if (_renderTransparency) reserveSize += transQueue.size();

		std::vector<RenderQueueEntry> combinedQueues;
		combinedQueues.reserve(reserveSize);

		// Combine all render queues into one
		combinedQueues.insert(combinedQueues.end(), geoQueue.begin(), geoQueue.end());
		if (_renderOverlay) combinedQueues.insert(combinedQueues.end(), overlayQueue.begin(), overlayQueue.end());
		if (_renderTransparency) combinedQueues.insert(combinedQueues.end(), transQueue.begin(), transQueue.end());

		// Filter out non-selected entities from the render queue
		std::vector<RenderQueueEntry> filteredQueue;
		filteredQueue.reserve(_outlinedEntities.size());

		for (int i = 0; i < combinedQueues.size(); i++)
		{
			const RenderQueueEntry &entry = combinedQueues[i];
			const auto &instance = entry.instance;

			Entity *entity = instance.subject->GetEntity();
			for (const auto &outline : _outlinedEntities)
			{
				if (entity == outline.Get())
				{
					// Entity is selected, keep it in the render queue
					filteredQueue.push_back(entry);
					break;
				}
			}
		}

		UINT entity_i = 0;
		for (const RenderQueueEntry &entry : filteredQueue)
		{
			const auto &instance = entry.instance;
			const auto &resources = entry.resourceGroup;

			MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(instance.subject);

			ZoneNamedNC(renderMeshZone, "Draw Entity", RandomUniqueColor(), true);
			const std::string &name = meshBehaviour->GetEntity()->GetName();
			ZoneText(name.c_str(), name.size());
			TracyD3D11NamedZoneC(_tracyD3D11Context, renderMeshD3D11Zone, "Draw Entity", RandomUniqueColor(), true);

			// Bind shared geometry resources
			if (_currMeshID != resources.meshID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Mesh", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Mesh", RandomUniqueColor(), true);

				loadedMesh = _content->GetMesh(resources.meshID);
				if (!loadedMesh->BindMeshBuffers(_context))
				{
					ErrMsgF("Failed to bind mesh buffers for instance #{}!", entity_i);
					return false;
				}
				_currMeshID = resources.meshID;
			}
			else if (loadedMesh == nullptr)
				loadedMesh = _content->GetMesh(resources.meshID);

			if (_currTexID != resources.material->textureID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Texture", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Texture", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->textureID)->GetSRV();
				_context->PSSetShaderResources(0, 1, &srv);
				_currTexID = resources.material->textureID;
			}

			if (resources.material->samplerID != CONTENT_NULL)
			{
				if (_currSamplerID != resources.material->samplerID)
				{
					ZoneNamedXNC(bindResourceZone, "Bind Sampler State", RandomUniqueColor(), true);
					TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State", RandomUniqueColor(), true);

					ID3D11SamplerState *const ss = _content->GetSampler(resources.material->samplerID)->GetSamplerState();
					_context->PSSetSamplers(0, 1, &ss);
					_currSamplerID = resources.material->samplerID;
				}
			}
			else if (_currSamplerID != defaultSamplerID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Sampler State Default", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State Default", RandomUniqueColor(), true);

				ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
				_context->PSSetSamplers(0, 1, &ss);
				_currSamplerID = defaultSamplerID;
			}

			if (resources.material->vsID != CONTENT_NULL)
			{
				if (_currVsID != resources.material->vsID)
				{
					ZoneNamedXNC(bindResourceZone, "Bind Vertex Shader", RandomUniqueColor(), true);
					TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Vertex Shader", RandomUniqueColor(), true);

					ShaderD3D11 *vs = _content->GetShader(resources.material->vsID);
					if (!vs)
					{
						ErrMsgF("Failed to get vertex shader #{}!", resources.material->vsID);
						return false;
					}

					if (!vs->BindShader(_context))
					{
						ErrMsgF("Failed to bind vertex shader #{}!", resources.material->vsID);
						return false;
					}
					_currVsID = resources.material->vsID;
				}
			}
			else if (_currVsID != defaultVsID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Vertex Shader Default", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Vertex Shader Default", RandomUniqueColor(), true);

				if (!_content->GetShader(defaultVsID)->BindShader(_context))
				{
					ErrMsg("Failed to bind default vertex shader!");
					return false;
				}
				_currVsID = defaultVsID;
			}

			// Bind private entity resources
			if (!meshBehaviour->InitialBindBuffers(_context))
			{
				ErrMsgF("Failed to bind private buffers for instance #{}!", entity_i);
				return false;
			}

			// Perform draw calls
			if (loadedMesh == nullptr)
			{
				ErrMsgF("Failed to perform draw call for instance #{}, loadedMesh is nullptr!", entity_i);
				return false;
			}

			const UINT subMeshCount = loadedMesh->GetNrOfSubMeshes();

			UINT lodIndex = 0;
			if (subMeshCount > 1)
			{
				// Mesh has LODs, determine which one to use.
				ZoneNamedXNC(getMeshLODZone, "Calculate LOD", RandomUniqueColor(), true);

				// Get the mesh center from the bounding box.
				dx::BoundingOrientedBox bounds;
				meshBehaviour->StoreBounds(bounds);
				auto meshPos = Load(bounds.Center);

				// Get the distance to the camera.
				auto distVec = dx::XMVector3LengthEst(dx::XMVectorSubtract(camPos, meshPos));
				float dist = dx::XMVectorGetX(distVec);

				// Calculate the scaled LOD distances.
				float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
				float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

				CameraPlanes camPlanes = _currViewCamera->GetPlanes();
				float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
				float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

				// Get the LOD as a normalized float.
				float clampedDist = std::clamp(dist, lodDistMin, lodDistMax);
				float normalizedDist = (clampedDist - lodDistMin) * (1.0f / (lodDistMax - lodDistMin));

				// Get the LOD index.
				lodIndex = static_cast<UINT>(normalizedDist * (subMeshCount - 1));

				//DbgMsg(std::format("Mesh '{}', LOD [{} / {}] at dist ({})!", _content->GetMeshName(resources.meshID), lodIndex + 1, subMeshCount, dist));
				Assert(lodIndex < subMeshCount, std::format("The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount));

				meshBehaviour->SetLastUsedLOD(lodIndex, normalizedDist);
			}

			{
				ZoneNamedXNC(renderSubmeshZone, "Draw Submesh", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, renderSubmeshD3D11Zone, "Draw Submesh", RandomUniqueColor(), true);

				if (!loadedMesh->PerformSubMeshDrawCall(_context, lodIndex))
				{
					ErrMsgF("Failed to perform draw call for instance #{}, sub mesh #{}!", entity_i, lodIndex);
					return false;
				}
			}

			entity_i++;
		}
	}

	// Unbind render target
	static constexpr ID3D11RenderTargetView *nullViews[] = { nullptr };
	_context->OMSetRenderTargets(1, nullViews, 0);

	return true;
}
#endif

bool Graphics::RenderScreenEffect(UINT psID)
{
	if (psID == CONTENT_NULL)
		return false;

	if (_currInputLayoutID != CONTENT_NULL)
	{
		// No vertex/index buffer needed
		_context->IASetInputLayout(nullptr);
		_currInputLayoutID = CONTENT_NULL;
	}

	static UINT screenEffectVsID = _content->GetShaderID("VS_ScreenEffect");
	if (_currVsID != screenEffectVsID)
	{
		if (!_content->GetShader(screenEffectVsID)->BindShader(_context))
		{
			ErrMsg("Failed to bind vertex shader!");
			return false;
		}
		_currVsID = screenEffectVsID;
	}

	if (_currPsID != psID)
	{
		if (!_content->GetShader(psID)->BindShader(_context))
		{
			ErrMsg("Failed to bind pixel shader!");
			return false;
		}
		_currPsID = psID;
	}

	ComPtr<ID3D11DepthStencilState> prevDSS = nullptr;
	UINT stencilRef = 0;
	_context->OMGetDepthStencilState(prevDSS.ReleaseAndGetAddressOf(), &stencilRef);
	_context->OMSetDepthStencilState(_nulldss.Get(), 0);

	// Draw 3 vertices directly, positions are generated by the vertex shader
	_context->Draw(3, 0);

	_context->OMSetDepthStencilState(prevDSS.Get(), stencilRef);

	return true;
}

bool Graphics::RenderGeometry(bool overlayStage, bool skipPixelShader)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Geometry", RandomUniqueColor());

	static UINT defaultVsID = _content->GetShaderID("VS_GeometryDistortion");
	static UINT defaultPsID = _content->GetShaderID("PS_Geometry");
	static UINT defaultSamplerID = _content->GetSamplerID("Fallback");
	static UINT defaultAmbientID = _content->GetTextureID("Ambient");

	if (!_content->GetShader(defaultVsID)->BindShader(_context))
	{
		ErrMsg("Failed to bind geometry vertex shader!");
		return false;
	}
	_currVsID = defaultVsID;

	if (!skipPixelShader)
	{
		if (!_content->GetShader(defaultPsID)->BindShader(_context))
		{
			ErrMsg("Failed to bind geometry pixel shader!");
			return false;
		}
		_currPsID = defaultPsID;
	}

	if (_currSamplerID != defaultSamplerID)
	{
		ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
		_context->PSSetSamplers(0, 1, &ss);
		_currSamplerID = defaultSamplerID;
	}

	const MeshD3D11 *loadedMesh = nullptr;
	_currMeshID = CONTENT_NULL;
	_currNormalID = CONTENT_NULL;
	_currSpecularID = CONTENT_NULL;
	_currGlossinessID = CONTENT_NULL;
	_currReflectiveID = CONTENT_NULL;
	_currOcclusionID = CONTENT_NULL;

	ID3D11ShaderResourceView *srv;

	srv = _content->GetTexture(defaultAmbientID)->GetSRV();
	_context->PSSetShaderResources(4, 1, &srv);
	_currAmbientID = defaultAmbientID;

	auto camPos = Load(_currViewCamera->GetTransform()->GetPosition(World));

	auto &queue = overlayStage ? _currViewCamera->GetOverlayQueue() : _currViewCamera->GetGeometryQueue();

	UINT entity_i = 0;
	for (const RenderQueueEntry &entry : queue)
	{
		const auto &instance = entry.instance;
		const auto &resources = entry.resourceGroup;

		if (resources.shadowsOnly)
			continue;

		MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(instance.subject);

		if (!meshBehaviour)
		{
			Warn(std::format("Skipping rendering for non-mesh #{}!", entity_i));
			continue;
		}

		ZoneNamedNC(renderMeshZone, "Draw Entity", RandomUniqueColor(), true);
		const std::string &name = meshBehaviour->GetEntity()->GetName();
		ZoneText(name.c_str(), name.size());
		TracyD3D11NamedZoneC(_tracyD3D11Context, renderMeshD3D11Zone, "Draw Entity", RandomUniqueColor(), true);

		// Bind shared geometry resources
		if (_currMeshID != resources.meshID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Mesh", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Mesh", RandomUniqueColor(), true);

			loadedMesh = _content->GetMesh(resources.meshID);
			if (!loadedMesh->BindMeshBuffers(_context))
			{
				ErrMsgF("Failed to bind mesh buffers for instance #{}!", entity_i);
				return false;
			}
			_currMeshID = resources.meshID;
		}
		else if (loadedMesh == nullptr)
			loadedMesh = _content->GetMesh(resources.meshID);

		if (_currTexID != resources.material->textureID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Texture", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Texture", RandomUniqueColor(), true);

			srv = _content->GetTexture(resources.material->textureID)->GetSRV();
			_context->PSSetShaderResources(0, 1, &srv);
			_currTexID = resources.material->textureID;
		}

		if (resources.material->normalID != CONTENT_NULL)
			if (_currNormalID != resources.material->normalID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Normal Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Normal Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->normalID)->GetSRV();
				_context->PSSetShaderResources(1, 1, &srv);
				_currNormalID = resources.material->normalID;
			}

		if (resources.material->specularID != CONTENT_NULL)
			if (_currSpecularID != resources.material->specularID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Specular Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Specular Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->specularID)->GetSRV();
				_context->PSSetShaderResources(2, 1, &srv);
				_currSpecularID = resources.material->specularID;
			}

		if (resources.material->glossinessID != CONTENT_NULL)
			if (_currGlossinessID != resources.material->glossinessID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Glossiness Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Glossiness Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->glossinessID)->GetSRV();
				_context->PSSetShaderResources(9, 1, &srv);
				_currGlossinessID = resources.material->glossinessID;
			}

		if (resources.material->ambientID != CONTENT_NULL)
			if (_currAmbientID != resources.material->ambientID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Ambient Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Ambient Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->ambientID)->GetSRV();
				_context->PSSetShaderResources(4, 1, &srv);
				_currAmbientID = resources.material->ambientID;
			}
		
		if (resources.material->reflectiveID != CONTENT_NULL)
			if (_currReflectiveID != resources.material->reflectiveID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Reflective Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Reflective Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->reflectiveID)->GetSRV();
				_context->PSSetShaderResources(3, 1, &srv);
				_currReflectiveID = resources.material->reflectiveID;
			}
		
		if (resources.material->occlusionID != CONTENT_NULL)
			if (_currOcclusionID != resources.material->occlusionID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Occlusion Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Occlusion Map", RandomUniqueColor(), true);

				srv = _content->GetTexture(resources.material->occlusionID)->GetSRV();
				_context->PSSetShaderResources(8, 1, &srv);
				_currOcclusionID = resources.material->occlusionID;
			}

		if (resources.material->samplerID != CONTENT_NULL)
		{
			if (_currSamplerID != resources.material->samplerID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Sampler State", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State", RandomUniqueColor(), true);

				ID3D11SamplerState *const ss = _content->GetSampler(resources.material->samplerID)->GetSamplerState();
				_context->PSSetSamplers(0, 1, &ss);
				_currSamplerID = resources.material->samplerID;
			}
		}
		else if (_currSamplerID != defaultSamplerID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Sampler State Default", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State Default", RandomUniqueColor(), true);

			ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
			_context->PSSetSamplers(0, 1, &ss);
			_currSamplerID = defaultSamplerID;
		}

		if (!skipPixelShader)
		{
			if (resources.material->psID != CONTENT_NULL)
			{
				if (_currPsID != resources.material->psID)
				{
					ZoneNamedXNC(bindResourceZone, "Bind Pixel Shader", RandomUniqueColor(), true);
					TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Pixel Shader", RandomUniqueColor(), true);

					ShaderD3D11 *ps = _content->GetShader(resources.material->psID);
					if (!ps)
					{
						ErrMsgF("Failed to get pixel shader #{}!", resources.material->psID);
						return false;
					}

					if (!ps->BindShader(_context))
					{
						ErrMsgF("Failed to bind pixel shader #{}!", resources.material->psID);
						return false;
					}
					_currPsID = resources.material->psID;
				}
			}
			else if (_currPsID != defaultPsID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Pixel Shader Default", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Pixel Shader Default", RandomUniqueColor(), true);

				if (!_content->GetShader(defaultPsID)->BindShader(_context))
				{
					ErrMsg("Failed to bind default pixel shader!");
					return false;
				}
				_currPsID = defaultPsID;
			}
		}

		if (resources.material->vsID != CONTENT_NULL)
		{
			if (_currVsID != resources.material->vsID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Vertex Shader", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Vertex Shader", RandomUniqueColor(), true);

				ShaderD3D11 *vs = _content->GetShader(resources.material->vsID);
				if (!vs)
				{
					ErrMsgF("Failed to get vertex shader #{}!", resources.material->vsID);
					return false;
				}

				if (!vs->BindShader(_context))
				{
					ErrMsgF("Failed to bind vertex shader #{}!", resources.material->vsID);
					return false;
				}
				_currVsID = resources.material->vsID;
			}
		}
		else if (_currVsID != defaultVsID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Vertex Shader Default", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Vertex Shader Default", RandomUniqueColor(), true);

			if (!_content->GetShader(defaultVsID)->BindShader(_context))
			{
				ErrMsg("Failed to bind default vertex shader!");
				return false;
			}
			_currVsID = defaultVsID;
		}

		// Bind private entity resources
		if (!meshBehaviour->InitialBindBuffers(_context))
		{
			ErrMsgF("Failed to bind private buffers for instance #{}!", entity_i);
			return false;
		}

		// Perform draw calls
		if (loadedMesh == nullptr)
		{
			ErrMsgF("Failed to perform draw call for instance #{}, loadedMesh is nullptr!", entity_i);
			return false;
		}

		const UINT
			prevTexID = _currTexID,
			prevAmbientID = _currAmbientID,
			prevSpecularID = _currSpecularID;

		const UINT subMeshCount = loadedMesh->GetNrOfSubMeshes();

		UINT lodIndex = 0;
		if (subMeshCount > 1)
		{
			// Mesh has LODs, determine which one to use.
			ZoneNamedXNC(getMeshLODZone, "Calculate LOD", RandomUniqueColor(), true);

			// Get the mesh center from the bounding box.
			dx::BoundingOrientedBox bounds;
			meshBehaviour->StoreBounds(bounds);
			auto meshPos = Load(bounds.Center);

			// Get the distance to the camera.
			auto distVec = dx::XMVector3LengthEst(dx::XMVectorSubtract(camPos, meshPos));
			float dist = dx::XMVectorGetX(distVec);

			// Calculate the scaled LOD distances.
			float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
			float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

			CameraPlanes camPlanes = _currViewCamera->GetPlanes();
			float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
			float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

			// Get the LOD as a normalized float.
			float clampedDist = std::clamp(dist, lodDistMin, lodDistMax);
			float normalizedDist = (clampedDist - lodDistMin) * (1.0f / (lodDistMax - lodDistMin));

			// Get the LOD index.
			lodIndex = static_cast<UINT>(normalizedDist * (subMeshCount - 1));

			//DbgMsg(std::format("Mesh '{}', LOD [{} / {}] at dist ({})!", _content->GetMeshName(resources.meshID), lodIndex + 1, subMeshCount, dist));
			Assert(lodIndex < subMeshCount, std::format("The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount));

			meshBehaviour->SetLastUsedLOD(lodIndex, normalizedDist);
		}

		{
			ZoneNamedXNC(renderSubmeshZone, "Draw Submesh", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, renderSubmeshD3D11Zone, "Draw Submesh", RandomUniqueColor(), true);

			ID3D11Buffer *const specularBuffer = loadedMesh->GetSpecularBuffer(lodIndex);
			_context->PSSetConstantBuffers(1, 1, &specularBuffer);

			if (!loadedMesh->PerformSubMeshDrawCall(_context, lodIndex))
			{
				ErrMsgF("Failed to perform draw call for instance #{}, sub mesh #{}!", entity_i, lodIndex);
				return false;
			}
		}

		entity_i++;
	}

	return true;
}

bool Graphics::RenderOpaque(
	ID3D11RenderTargetView *targetSceneRTV,
	ID3D11RenderTargetView *targetDepthRTV,
	ID3D11DepthStencilView *targetDSV,
	const D3D11_VIEWPORT *targetViewport,
	bool overlayStage)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Opaque", RandomUniqueColor());

	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, clearRTVsD3D11Zone, "Clear Targets", RandomUniqueColor(), true);

		ProjectionInfo proj = _currViewCamera->GetCurrProjectionInfo();
		float farDist = max(proj.planes.nearZ, proj.planes.farZ);

		// Clear & bind render targets
		if (!overlayStage) // Skip clearing scene render target if on the overlay-stage
		{
			constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			constexpr float clearEmission[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			float clearDepth[4] = { farDist, farDist, farDist, farDist };
			_context->ClearRenderTargetView(targetSceneRTV, clearColor);
			_context->ClearRenderTargetView(_emissionRT.GetRTV(), clearEmission);
			_context->ClearRenderTargetView(targetDepthRTV, clearDepth);
		}

		_context->ClearDepthStencilView(targetDSV, D3D11_CLEAR_DEPTH, _currViewCamera->GetInverted() ? 0.0f : 1.0f, 0);
	}

	ID3D11RenderTargetView *rtvs[3] = { 
		targetSceneRTV, 
		targetDepthRTV, 
		_emissionRT.GetRTV() 
	};
	_context->OMSetRenderTargets(3, rtvs, targetDSV);

	_context->RSSetViewports(1, targetViewport);
	_context->RSSetState(_wireframe ? _wireframeRasterizer.Get() : _defaultRasterizer.Get());
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Bind camera data
	if (!_currViewCamera->BindViewBuffers())
	{
		ErrMsg("Failed to bind view camera buffers!");
		return false;
	}

	// Bind global light data
	ID3D11Buffer *const globalLightBuffer = _globalLightBuffer.GetBuffer();
	_context->PSSetConstantBuffers(0, 1, &globalLightBuffer);

	// Bind general data
	ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
	_context->PSSetConstantBuffers(5, 1, &generalData);

	ID3D11ShaderResourceView *const cubemap = _content->GetCubemap(_environmentCubemapID)->GetSRV();
	_context->PSSetShaderResources(20, 1, &cubemap);

	// Bind spotlight collection
	if (!_currSpotLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind spotlight buffers!");
		return false;
	}

	// Bind pointlight collection
	if (!_currPointLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind pointlight buffers!");
		return false;
	}

	// Bind light tile data
	ID3D11ShaderResourceView *const lightTileBuffer = _lightGridBuffer.GetSRV();
	_context->PSSetShaderResources(14, 1, &lightTileBuffer);

	static ID3D11SamplerState *const ss = _content->GetSampler("Clamp")->GetSamplerState();
	static ID3D11SamplerState *const ssShadow = _content->GetSampler("Shadow")->GetSamplerState();
	static ID3D11SamplerState *const ssShadowCube = _content->GetSampler("ShadowCube")->GetSamplerState();
	static ID3D11SamplerState *const ssTest = _content->GetSampler("Test")->GetSamplerState();
	static ID3D11SamplerState *const ssHQ = _content->GetSampler("HQ")->GetSamplerState();
	static ID3D11SamplerState *const ssArray[5] = { ss, ssShadow, ssShadowCube, ssTest, ssHQ };
	_context->PSSetSamplers(0, 5, ssArray);

	// Bind camera lighting data
	if (!_currViewCamera->BindPSLightingBuffers())
	{
		ErrMsg("Failed to bind camera buffers!");
		return false;
	}

	// Render skybox
	if (_skyboxPsID != CONTENT_NULL && !overlayStage)
	{
		if (!RenderScreenEffect(_skyboxPsID))
		{
			ErrMsg("Failed to render skybox!");
			return false;
		}
	}

	// Bind geometry stage resources
	static UINT geometryInputLayoutID = _content->GetInputLayoutID("Fallback");
	_context->IASetInputLayout(_content->GetInputLayout(geometryInputLayoutID)->GetInputLayout());
	_currInputLayoutID = geometryInputLayoutID;

	if (!RenderGeometry(overlayStage))
	{
		ErrMsg("Failed to render geometry in RenderOpaque()!");
		return false;
	}

	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, unbindBuffersD3D11Zone, "Unbind Buffers", RandomUniqueColor(), true);

		// Unbind pointlight collection
		if (!_currPointLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind pointlight buffers!");
			return false;
		}

		// Unbind spotlight collection
		if (!_currSpotLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind spotlight buffers!");
			return false;
		}

		// Unbind render targets
		static ID3D11RenderTargetView *const nullRTVS[3] = { };
		_context->OMSetRenderTargets(3, nullRTVS, nullptr);
	}

	return true;
}

bool Graphics::RenderCustom(
	ID3D11RenderTargetView *targetSceneRTV,
	ID3D11RenderTargetView *targetDepthRTV,
	ID3D11DepthStencilView *targetDSV,
	const D3D11_VIEWPORT *targetViewport,
	const std::string &pixelShader, 
	bool overlayStage)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Custom", RandomUniqueColor());

	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, clearRTVsD3D11Zone, "Clear Targets", RandomUniqueColor(), true);

		ProjectionInfo proj = _currViewCamera->GetCurrProjectionInfo();
		float farDist = max(proj.planes.nearZ, proj.planes.farZ);

		// Clear & bind render targets
		if (!overlayStage) // Skip clearing scene render target if on the overlay-stage
		{
			constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			_context->ClearRenderTargetView(targetSceneRTV, clearColor);
		}

		float clearDepth[4] = { farDist, farDist, farDist, farDist };
		_context->ClearRenderTargetView(targetDepthRTV, clearDepth);
		_context->ClearDepthStencilView(targetDSV, D3D11_CLEAR_DEPTH, _currViewCamera->GetInverted() ? 0.0f : 1.0f, 0);
	}

	_context->OMSetRenderTargets(1, &targetSceneRTV, targetDSV);

	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_context->RSSetViewports(1, &_viewportSceneView);
	_context->RSSetState(_wireframe ? _wireframeRasterizer.Get() : _defaultRasterizer.Get());

	// Bind camera data
	if (!_currViewCamera->BindViewBuffers())
	{
		ErrMsg("Failed to bind view camera buffers!");
		return false;
	}

	// Bind global light data
	ID3D11Buffer *const globalLightBuffer = _globalLightBuffer.GetBuffer();
	_context->PSSetConstantBuffers(0, 1, &globalLightBuffer);

	// Bind general data
	ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
	_context->PSSetConstantBuffers(5, 1, &generalData);

	// Bind spotlight collection
	if (!_currSpotLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind spotlight buffers!");
		return false;
	}

	// Bind pointlight collection
	if (!_currPointLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind pointlight buffers!");
		return false;
	}

	// Bind light tile data
	ID3D11ShaderResourceView *const lightTileBuffer = _lightGridBuffer.GetSRV();
	_context->PSSetShaderResources(14, 1, &lightTileBuffer);
	
	static ID3D11SamplerState *const ss = _content->GetSampler("Clamp")->GetSamplerState();
	static ID3D11SamplerState *const ssShadow = _content->GetSampler("Shadow")->GetSamplerState();
	static ID3D11SamplerState *const ssShadowCube = _content->GetSampler("ShadowCube")->GetSamplerState();
	static ID3D11SamplerState *const ssTest = _content->GetSampler("Test")->GetSamplerState();
	static ID3D11SamplerState *const ssHQ = _content->GetSampler("HQ")->GetSamplerState();
	static ID3D11SamplerState *const ssArray[5] = { ss, ssShadow, ssShadowCube, ssTest, ssHQ };
	_context->PSSetSamplers(0, 5, ssArray);

	// Bind camera lighting data
	if (!_currViewCamera->BindPSLightingBuffers())
	{
		ErrMsg("Failed to bind camera buffers!");
		return false;
	}

	// Bind geometry stage resources
	static UINT geometryInputLayoutID = _content->GetInputLayoutID("Fallback");
	_context->IASetInputLayout(_content->GetInputLayout(geometryInputLayoutID)->GetInputLayout());
	_currInputLayoutID = geometryInputLayoutID;

	if (!_content->GetShader(pixelShader)->BindShader(_context))
	{
		ErrMsg("Failed to bind pixel shader!");
		return false;
	}

	if (!RenderGeometry(overlayStage, true))
	{
		ErrMsg("Failed to render geometry!");
		return false;
	}

	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, unbindBuffersD3D11Zone, "Unbind Buffers", RandomUniqueColor(), true);

		// Unbind pointlight collection
		if (!_currPointLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind pointlight buffers!");
			return false;
		}

		// Unbind spotlight collection
		if (!_currSpotLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind spotlight buffers!");
			return false;
		}

		// Unbind render target
		static ID3D11RenderTargetView *const nullRTV = nullptr;
		_context->OMSetRenderTargets(1, &nullRTV, nullptr);
	}

	return true;
}

bool Graphics::RenderTransparency(
	ID3D11RenderTargetView *targetRTV,
	ID3D11DepthStencilView *targetDSV,
	const D3D11_VIEWPORT *targetViewport)
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Transparency", RandomUniqueColor());

	_context->OMSetDepthStencilState(_tdss.Get(), 0);

	ID3D11BlendState *prevBlendState;
	FLOAT prevBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	UINT prevSampleMask = 0;
	_context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

	static UINT defaultBlendStateID = _content->GetBlendStateID("Fallback");
	ID3D11BlendState *const defaultBlendState = _content->GetBlendState(defaultBlendStateID);
	constexpr float transparentBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	if (_currBlendStateID != defaultBlendStateID)
	{
		_context->OMSetBlendState(defaultBlendState, transparentBlendFactor, 0xffffffff);
		_currBlendStateID = defaultBlendStateID;
	}

	_context->OMSetRenderTargets(1, &targetRTV, targetDSV);
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_context->RSSetViewports(1, targetViewport);
	_context->RSSetState(_wireframe ? _wireframeRasterizer.Get() : _defaultRasterizer.Get());

	// Bind camera data
	if (!_currViewCamera->BindTransparentBuffers())
	{
		ErrMsg("Failed to bind billboard camera buffers!");
		return false;
	}

	// Bind transparency stage resources
	static UINT transparencyInputLayoutID = _content->GetInputLayoutID("Fallback");
	if (_currInputLayoutID != transparencyInputLayoutID)
	{
		_context->IASetInputLayout(_content->GetInputLayout(transparencyInputLayoutID)->GetInputLayout());
		_currInputLayoutID = transparencyInputLayoutID;
	}

	static UINT vsID = _content->GetShaderID("VS_Geometry");
	if (_currVsID != vsID)
	{
		if (!_content->GetShader(vsID)->BindShader(_context))
		{
			ErrMsg("Failed to bind geometry vertex shader!");
			return false;
		}
		_currVsID = vsID;
	}

	static UINT psID = _content->GetShaderID("PS_Transparent");
	if (_currPsID != psID)
	{
		if (!_content->GetShader(psID)->BindShader(_context))
		{
			ErrMsg("Failed to bind transparent pixel shader!");
			return false;
		}
		_currPsID = psID;
	}

	static UINT defaultSamplerID = _content->GetSamplerID("Clamp");
	if (_currSamplerID != defaultSamplerID)
	{
		ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
		_context->PSSetSamplers(0, 1, &ss);
		_currSamplerID = defaultSamplerID;
	}

	// Bind global light data
	ID3D11Buffer *const globalLightBuffer = _globalLightBuffer.GetBuffer();
	_context->PSSetConstantBuffers(0, 1, &globalLightBuffer);

	// Bind general data
	ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
	_context->PSSetConstantBuffers(5, 1, &generalData);

	// Bind light tile data
	ID3D11ShaderResourceView *const lightTileBuffer = _lightGridBuffer.GetSRV();
	_context->PSSetShaderResources(14, 1, &lightTileBuffer);

	// Bind spotlight collection
	if (!_currSpotLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind spotlight buffers!");
		return false;
	}

	// Bind pointlight collection
	if (!_currPointLightCollection.Get()->BindPSBuffers(_context))
	{
		ErrMsg("Failed to bind pointlight buffers!");
		return false;
	}

	static UINT defaultNormalID = _content->GetTextureID("Default_Normal");
	if (_currNormalID != defaultNormalID)
	{
		ID3D11ShaderResourceView *const srv = _content->GetTexture(defaultNormalID)->GetSRV();
		_context->PSSetShaderResources(1, 1, &srv);
		_currNormalID = defaultNormalID;
	}

	static UINT defaultSpecularID = _content->GetTextureID("Default_Specular");
	if (_currSpecularID != defaultSpecularID)
	{
		ID3D11ShaderResourceView *const srv = _content->GetTexture(defaultSpecularID)->GetSRV();
		_context->PSSetShaderResources(2, 1, &srv);
		_currSpecularID = defaultSpecularID;
	}
	
	static UINT defaultAmbientID = _content->GetTextureID("Ambient");
	if (_currAmbientID != defaultAmbientID)
	{
		ID3D11ShaderResourceView *const srv = _content->GetTexture(defaultAmbientID)->GetSRV();
		_context->PSSetShaderResources(4, 1, &srv);
		_currAmbientID = defaultAmbientID;
	}

	_currMeshID = CONTENT_NULL;
	const MeshD3D11 *loadedMesh = nullptr;

	auto camPos = Load(_currViewCamera->GetTransform()->GetPosition(World));
	
	UINT entity_i = 0;
	for (const RenderQueueEntry &entry : _currViewCamera->GetTransparentQueue())
	{
		const auto &instance = entry.instance;
		const auto &resources = entry.resourceGroup;

		if (resources.shadowsOnly)
			continue;

		MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(instance.subject);

		if (!meshBehaviour)
		{
			ErrMsgF("Skipping rendering for non-mesh #{}!", entity_i);
			return false;
		}

		ZoneNamedNC(renderMeshZone, "Draw Entity", RandomUniqueColor(), true);
		const std::string &name = meshBehaviour->GetEntity()->GetName();
		ZoneText(name.c_str(), name.size());
		TracyD3D11NamedZoneC(_tracyD3D11Context, renderMeshD3D11Zone, "Draw Entity", RandomUniqueColor(), true);

		// Bind shared geometry resources
		if (_currMeshID != resources.meshID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Mesh", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindMeshD3D11Zone, "Bind Mesh", RandomUniqueColor(), true);

			loadedMesh = _content->GetMesh(resources.meshID);
			if (!loadedMesh->BindMeshBuffers(_context))
			{
				ErrMsgF("Failed to bind mesh buffers for instance #{}!", entity_i);
				return false;
			}
			_currMeshID = resources.meshID;
		}
		else if (loadedMesh == nullptr)
			loadedMesh = _content->GetMesh(resources.meshID);

		if (_currTexID != resources.material->textureID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Texture", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Texture", RandomUniqueColor(), true);

			ID3D11ShaderResourceView *const srv = _content->GetTexture(resources.material->textureID)->GetSRV();
			_context->PSSetShaderResources(0, 1, &srv);
			_currTexID = resources.material->textureID;
		}

		if (resources.material->normalID != CONTENT_NULL)
			if (_currNormalID != resources.material->normalID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Normal Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Normal Map", RandomUniqueColor(), true);

				ID3D11ShaderResourceView *const srv = _content->GetTexture(resources.material->normalID)->GetSRV();
				_context->PSSetShaderResources(1, 1, &srv);
				_currNormalID = resources.material->normalID;
			}

		if (resources.material->specularID != CONTENT_NULL)
			if (_currSpecularID != resources.material->specularID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Specular Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Specular Map", RandomUniqueColor(), true);

				ID3D11ShaderResourceView *const srv = _content->GetTexture(resources.material->specularID)->GetSRV();
				_context->PSSetShaderResources(2, 1, &srv);
				_currSpecularID = resources.material->specularID;
			}
		
		if (resources.material->ambientID != CONTENT_NULL)
			if (_currAmbientID != resources.material->ambientID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Ambient Map", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Ambient Map", RandomUniqueColor(), true);

				ID3D11ShaderResourceView *const srv = _content->GetTexture(resources.material->ambientID)->GetSRV();
				_context->PSSetShaderResources(4, 1, &srv);
				_currAmbientID = resources.material->ambientID;
			}

		if (resources.material->samplerID != CONTENT_NULL)
		{
			if (_currSamplerID != resources.material->samplerID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Sampler State", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State", RandomUniqueColor(), true);

				ID3D11SamplerState *const ss = _content->GetSampler(resources.material->samplerID)->GetSamplerState();
				_context->PSSetSamplers(0, 1, &ss);
				_currSamplerID = resources.material->samplerID;
			}
		}
		else if (_currSamplerID != defaultSamplerID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Sampler State Default", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Sampler State Default", RandomUniqueColor(), true);

			ID3D11SamplerState *const ss = _content->GetSampler(defaultSamplerID)->GetSamplerState();
			_context->PSSetSamplers(0, 1, &ss);
			_currSamplerID = defaultSamplerID;
		}

		if (resources.blendStateID != CONTENT_NULL)
		{
			if (_currBlendStateID != resources.blendStateID)
			{
				ZoneNamedXNC(bindResourceZone, "Bind Blend State", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Blend State", RandomUniqueColor(), true);

				ID3D11BlendState *const bs = _content->GetBlendState(resources.blendStateID);
				_context->OMSetBlendState(bs, transparentBlendFactor, 0xffffffff);
				_currBlendStateID = resources.blendStateID;
			}
		}
		else if (_currBlendStateID != defaultBlendStateID)
		{
			ZoneNamedXNC(bindResourceZone, "Bind Blend State Default", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, bindBufferD3D11Zone, "Bind Blend State Default", RandomUniqueColor(), true);

			_context->OMSetBlendState(defaultBlendState, transparentBlendFactor, 0xffffffff);
			_currBlendStateID = defaultBlendStateID;
		}

		// Bind private entity resources
		if (!meshBehaviour->InitialBindBuffers(_context))
		{
			ErrMsgF("Failed to bind private buffers for instance #{}!", entity_i);
			return false;
		}

		// Perform draw calls
		if (loadedMesh == nullptr)
		{
			ErrMsgF("Failed to perform draw call for instance #{}, loadedMesh is nullptr!", entity_i);
			return false;
		}

		const UINT subMeshCount = loadedMesh->GetNrOfSubMeshes();

		UINT lodIndex = 0;
		if (subMeshCount > 1)
		{
			// Mesh has LODs, determine which one to use.
			ZoneNamedXNC(getMeshLODZone, "Calculate LOD", RandomUniqueColor(), true);

			// Get the mesh center from the bounding box.
			dx::BoundingOrientedBox bounds;
			meshBehaviour->StoreBounds(bounds);
			auto meshPos = Load(bounds.Center);

			// Get the distance to the camera.
			auto distVec = dx::XMVector3LengthEst(dx::XMVectorSubtract(camPos, meshPos));
			float dist = dx::XMVectorGetX(distVec);

			// Calculate the scaled LOD distances.
			float meshAvgSideLength = (bounds.Extents.x + bounds.Extents.y + bounds.Extents.z) * (1.0f / 3.0f);
			float lodDimMult = 1.0f - exp(-meshAvgSideLength * LOD_DIST_DIM_SCALE_FACTOR);

			CameraPlanes camPlanes = _currViewCamera->GetPlanes();
			float lodDistMin = camPlanes.nearZ * LOD_DIST_MIN_MULT * lodDimMult;
			float lodDistMax = camPlanes.farZ * LOD_DIST_MAX_MULT * lodDimMult;

			// Get the LOD as a normalized float.
			float clampedDist = std::clamp(dist, lodDistMin, lodDistMax);
			float normalizedDist = (clampedDist - lodDistMin) * (1.0f / (lodDistMax - lodDistMin));

			// Get the LOD index.
			lodIndex = static_cast<UINT>(normalizedDist * (subMeshCount - 1));

			//DbgMsg(std::format("Mesh '{}', LOD [{} / {}] at dist ({})!", _content->GetMeshName(resources.meshID), lodIndex + 1, subMeshCount, dist));
			Assert(lodIndex < subMeshCount, std::format("The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount));

			meshBehaviour->SetLastUsedLOD(lodIndex, normalizedDist);
		}

		{
			ZoneNamedXNC(drawSubmeshZone, "Draw Submesh", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, drawSubmeshD3D11Zone, "Draw Submesh", RandomUniqueColor(), true);

			ID3D11Buffer *const specularBuffer = loadedMesh->GetSpecularBuffer(lodIndex);
			_context->PSSetConstantBuffers(1, 1, &specularBuffer);

			if (!loadedMesh->PerformSubMeshDrawCall(_context, lodIndex))
			{
				ErrMsgF("Failed to perform draw call for instance #{}, sub mesh #{}!", entity_i, lodIndex);
				return false;
			}
		}

		entity_i++;
	}

	{
		TracyD3D11NamedZoneXC(_tracyD3D11Context, ubbindBuffersD3D11Zone, "Unbind Buffers", RandomUniqueColor(), true);

		// Unbind pointlight collection
		if (!_currPointLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind pointlight buffers!");
			return false;
		}

		// Unbind spotlight collection
		if (!_currSpotLightCollection.Get()->UnbindPSBuffers(_context))
		{
			ErrMsg("Failed to unbind spotlight buffers!");
			return false;
		}

		// Reset blend state
		_context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);
		_context->OMSetDepthStencilState(GetCurrentDepthStencilState(), 0);

		// Unbind render target
		static ID3D11RenderTargetView *const nullRTV = nullptr;
		_context->OMSetRenderTargets(1, &nullRTV, nullptr);
	}

	return true;
}

bool Graphics::RenderPostFX()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Post-Processing", RandomUniqueColor());

	static ID3D11SamplerState *const ss = _content->GetSampler("Clamp")->GetSamplerState();
	_context->CSSetSamplers(0, 1, &ss); // Verify slot

	// Bind global light data
	ID3D11Buffer *const globalLightBuffer = _globalLightBuffer.GetBuffer();
	_context->CSSetConstantBuffers(0, 1, &globalLightBuffer);

	if (_renderPostFX)
	{
		// Perform Fog
		if (_renderFogFX)
		{
			ZoneNamedXNC(renderFogVolumeZone, "Render Fog", RandomUniqueColor(), true);
			TracyD3D11NamedZoneC(_tracyD3D11Context, renderFogVolumeD3D11Zone, "Render Fog", RandomUniqueColor(), true);

			if (true) // compute
			{
				// Bind distortion settings
				ID3D11Buffer *const distortionSettings = _distortionSettingsBuffer.GetBuffer();
				_context->CSSetConstantBuffers(2, 1, &distortionSettings);

				// Bind fog settings
				ID3D11Buffer *const fogSettings = _fogSettingsBuffer.GetBuffer();
				_context->CSSetConstantBuffers(6, 1, &fogSettings);

				// Bind general data
				ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
				_context->CSSetConstantBuffers(5, 1, &generalData);

				// Bind light tile data
				ID3D11ShaderResourceView *const lightTileBuffer = _lightGridBuffer.GetSRV();
				_context->CSSetShaderResources(14, 1, &lightTileBuffer);

				// Bind fog compute shader
				if (!_content->GetShader("CS_FogFX")->BindShader(_context))
				{
					ErrMsg("Failed to bind fog compute shader!");
					return false;
				}

				// Bind fog render target
				ID3D11UnorderedAccessView *const uav[1] = { _fogRT.GetUAV() };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind depth resource
				ID3D11ShaderResourceView *const srv[1] = { _depthRT.GetSRV() };
				_context->CSSetShaderResources(0, 1, srv);

				// Bind spotlight collection
				if (!_currSpotLightCollection.Get()->BindCSBuffers(_context))
				{
					ErrMsg("Failed to bind spotlight buffers!");
					return false;
				}

				// Bind pointlight collection
				if (!_currPointLightCollection.Get()->BindCSBuffers(_context))
				{
					ErrMsg("Failed to bind pointlight buffers!");
					return false;
				}

				// Bind shadow sampler
				static ID3D11SamplerState *const ssShadow = _content->GetSampler("Shadow")->GetSamplerState();
				static ID3D11SamplerState *const ssShadowCube = _content->GetSampler("ShadowCube")->GetSamplerState();
				static ID3D11SamplerState *const ssTest = _content->GetSampler("Test")->GetSamplerState();
				static ID3D11SamplerState *const ssHQ = _content->GetSampler("HQ")->GetSamplerState();
				static ID3D11SamplerState *const ssArray[4] = { ssShadow, ssShadowCube, ssTest, ssHQ };
				_context->CSSetSamplers(1, 4, ssArray);

				// Bind camera lighting data
				if (!_currViewCamera->BindCSLightingBuffers())
				{
					ErrMsg("Failed to bind camera buffers!");
					return false;
				}

				// Bind camera inverse view data
				if (!_currViewCamera->BindInverseBuffers())
				{
					ErrMsg("Failed to bind inverse camera buffers!");
					return false;
				}


				// Send execution command
				_context->Dispatch(static_cast<UINT>(ceil(_viewportFog.Width / 8.0f)), static_cast<UINT>(ceil(_viewportFog.Height / 8.0f)), 1);


				// Unbind pointlight collection
				if (!_currPointLightCollection.Get()->UnbindCSBuffers(_context))
				{
					ErrMsg("Failed to unbind pointlight buffers!");
					return false;
				}

				// Unbind spotlight collection
				if (!_currSpotLightCollection.Get()->UnbindCSBuffers(_context))
				{
					ErrMsg("Failed to unbind spotlight buffers!");
					return false;
				}

				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			}
			else
			{
				// Bind distortion settings
				ID3D11Buffer *const distortionSettings = _distortionSettingsBuffer.GetBuffer();
				_context->PSSetConstantBuffers(2, 1, &distortionSettings);

				// Bind fog settings
				ID3D11Buffer *const fogSettings = _fogSettingsBuffer.GetBuffer();
				_context->PSSetConstantBuffers(6, 1, &fogSettings);

				// Bind general data
				ID3D11Buffer *const generalData = _generalDataBuffer.GetBuffer();
				_context->PSSetConstantBuffers(5, 1, &generalData);

				// Bind light tile data
				ID3D11ShaderResourceView *const lightTileBuffer = _lightGridBuffer.GetSRV();
				_context->PSSetShaderResources(14, 1, &lightTileBuffer);

				// Bind fog render target
				ID3D11RenderTargetView *const rtv[1] = { _fogRT.GetRTV()};
				_context->OMSetRenderTargets(1, rtv, nullptr);

				// Bind depth resource
				ID3D11ShaderResourceView *const srv[1] = { _depthRT.GetSRV() };
				_context->PSSetShaderResources(32, 1, srv);

				// Bind spotlight collection
				if (!_currSpotLightCollection.Get()->BindPSBuffers(_context))
				{
					ErrMsg("Failed to bind spotlight buffers!");
					return false;
				}

				// Bind pointlight collection
				if (!_currPointLightCollection.Get()->BindPSBuffers(_context))
				{
					ErrMsg("Failed to bind pointlight buffers!");
					return false;
				}

				// Bind shadow sampler
				static ID3D11SamplerState *const ssShadow = _content->GetSampler("Shadow")->GetSamplerState();
				static ID3D11SamplerState *const ssShadowCube = _content->GetSampler("ShadowCube")->GetSamplerState();
				static ID3D11SamplerState *const ssTest = _content->GetSampler("Test")->GetSamplerState();
				static ID3D11SamplerState *const ssHQ = _content->GetSampler("HQ")->GetSamplerState();
				static ID3D11SamplerState *const ssArray[4] = { ssShadow, ssShadowCube, ssTest, ssHQ };
				_context->PSSetSamplers(1, 4, ssArray);

				// Bind camera lighting data
				if (!_currViewCamera->BindPSLightingBuffers())
				{
					ErrMsg("Failed to bind camera buffers!");
					return false;
				}

				// Bind camera inverse view data
				if (!_currViewCamera->BindInverseBuffers())
				{
					ErrMsg("Failed to bind inverse camera buffers!");
					return false;
				}

				// Render full-screen quad
				RenderScreenEffect(_content->GetShaderID("PS_ScreenEffectFog"));

				// Unbind pointlight collection
				if (!_currPointLightCollection.Get()->UnbindPSBuffers(_context))
				{
					ErrMsg("Failed to unbind pointlight buffers!");
					return false;
				}

				// Unbind spotlight collection
				if (!_currSpotLightCollection.Get()->UnbindPSBuffers(_context))
				{
					ErrMsg("Failed to unbind spotlight buffers!");
					return false;
				}

				// Unbind resource views
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->PSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11RenderTargetView *const nullRTV = nullptr;
				_context->OMSetRenderTargets(1, &nullRTV, nullptr);
			}
		}

		// Perform Fog Blur
		if (_renderFogFX && _fogBlurIterations > 0)
		{
			ZoneNamedXNC(fogBlurZone, "Fog Blur", RandomUniqueColor(), true);
			TracyD3D11NamedZoneC(_tracyD3D11Context, fogBlurD3D11Zone, "Fog Blur", RandomUniqueColor(), true);

			// Bind depth resource
			ID3D11ShaderResourceView *const depthSRV[1] = { _depthRT.GetSRV() };
			_context->CSSetShaderResources(1, 1, depthSRV);

			// Bind blur weights
			ID3D11ShaderResourceView *const srvGaussianWeights[1] = { _fogGaussianWeightsBuffer.GetSRV() };
			_context->CSSetShaderResources(3, 1, srvGaussianWeights);

			for (int i = 0; i < _fogBlurIterations; i++)
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, fogBlurIterationD3D11Zone, "Blur Iteration", RandomUniqueColor(), true);

				ID3D11UnorderedAccessView *uavStageOne = _intermediateFogRT.GetUAV();
				ID3D11ShaderResourceView *srvStageOne = _fogRT.GetSRV();

				ID3D11UnorderedAccessView *uavStageTwo = _fogRT.GetUAV();
				ID3D11ShaderResourceView *srvStageTwo = _intermediateFogRT.GetSRV();

				// Blur Stage One
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, fogBlurIterationXD3D11Zone, "Horizontal", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurHorizontalFX")->BindShader(_context))
					{
						ErrMsg("Failed to bind horizontal blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageOne };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageOne };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportBlur.Width / 8.0f)), static_cast<UINT>(ceil(_viewportBlur.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}

				// Blur Stage Two
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, fogBlurIterationYD3D11Zone, "Vertical", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurVerticalFX")->BindShader(_context))
					{
						ErrMsg("Failed to bind vertical blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageTwo };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageTwo };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportFog.Width / 8.0f)), static_cast<UINT>(ceil(_viewportFog.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}
			}

			// Unbind depth & weight resource
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(1, 1, nullSRV);
			_context->CSSetShaderResources(3, 1, nullSRV);
		}


		// Perform Emission downsample
		if (_renderEmissionFX && _emissionBlurIterations > 0)
		{
			ZoneNamedXNC(emissionDownsampleZone, "Emission Downsample", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionDownsampleD3D11Zone, "Emission Downsample", RandomUniqueColor(), true);

			// Bind compute shader
			if (!_content->GetShader("CS_Downsample")->BindShader(_context))
			{
				ErrMsg("Failed to bind downscale emission compute shader!");
				return false;
			}

			// Bind render target
			ID3D11UnorderedAccessView *const uav[1] = { _blurRT.GetUAV() };
			_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

			// Bind shader resource
			ID3D11ShaderResourceView *const srv[1] = { _emissionRT.GetSRV() };
			_context->CSSetShaderResources(0, 1, srv);


			// Send execution command
			_context->Dispatch(static_cast<UINT>(ceil(_viewportBlur.Width / 8.0f)), static_cast<UINT>(ceil(_viewportBlur.Height / 8.0f)), 1);


			// Unbind compute shader resources
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(0, 1, nullSRV);

			// Unbind render target
			static ID3D11UnorderedAccessView *const nullUAV = nullptr;
			_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		}

		// Perform Emission Blur
		if (_renderEmissionFX && _emissionBlurIterations > 0)
		{
			ZoneNamedXNC(emissionBlurZone, "Emission Blur", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurD3D11Zone, "Emission Blur", RandomUniqueColor(), true);

			// Bind depth resource
			ID3D11ShaderResourceView *const depthSRV[1] = { _depthRT.GetSRV() };
			_context->CSSetShaderResources(1, 1, depthSRV);

			// Bind blur weights
			ID3D11ShaderResourceView *const srvGaussianWeights[1] = { _emissionGaussianWeightsBuffer.GetSRV() };
			_context->CSSetShaderResources(3, 1, srvGaussianWeights);

			for (int i = 0; i < _emissionBlurIterations; i++)
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurIterationD3D11Zone, "Blur Iteration", RandomUniqueColor(), true);

				ID3D11UnorderedAccessView *uavStageOne = _intermediateBlurRT.GetUAV();
				ID3D11ShaderResourceView *srvStageOne = _blurRT.GetSRV();

				ID3D11UnorderedAccessView *uavStageTwo = _blurRT.GetUAV();
				ID3D11ShaderResourceView *srvStageTwo = _intermediateBlurRT.GetSRV();

				// Blur Stage One
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurIterationXD3D11Zone, "Horizontal", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurHorizontalEmission")->BindShader(_context))
					{
						ErrMsg("Failed to bind horizontal blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageOne };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageOne };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportBlur.Width / 8.0f)), static_cast<UINT>(ceil(_viewportBlur.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}

				// Blur Stage Two
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurIterationYD3D11Zone, "Vertical", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurVerticalEmission")->BindShader(_context))
					{
						ErrMsg("Failed to bind vertical blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageTwo };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageTwo };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportBlur.Width / 8.0f)), static_cast<UINT>(ceil(_viewportBlur.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}
			}

			// Unbind depth & weight resource
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(1, 1, nullSRV);
			_context->CSSetShaderResources(3, 1, nullSRV);
		}


#ifdef DEBUG_BUILD
		// Perform Outline Blur
		if (_renderOutlineFX && _outlineBlurIterations > 0)
		{
			ZoneNamedXNC(outlineBlurZone, "Outline Blur", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurD3D11Zone, "Outline Blur", RandomUniqueColor(), true);

			// Bind blur weights
			ID3D11ShaderResourceView *const srvGaussianWeights[1] = { _outlineGaussianWeightsBuffer.GetSRV() };
			_context->CSSetShaderResources(3, 1, srvGaussianWeights);

			for (int i = 0; i < _outlineBlurIterations; i++)
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurIterationD3D11Zone, "Blur Iteration", RandomUniqueColor(), true);

				ID3D11UnorderedAccessView *uavStageOne = _intermediateOutlineRT.GetUAV();
				ID3D11ShaderResourceView *srvStageOne = _outlineRT.GetSRV();

				ID3D11UnorderedAccessView *uavStageTwo = _outlineRT.GetUAV();
				ID3D11ShaderResourceView *srvStageTwo = _intermediateOutlineRT.GetSRV();

				// Blur Stage One
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurIterationXD3D11Zone, "Horizontal", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurHorizontalOutline")->BindShader(_context))
					{
						ErrMsg("Failed to bind horizontal blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageOne };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageOne };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportSceneView.Width / 8.0f)), static_cast<UINT>(ceil(_viewportSceneView.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}

				// Blur Stage Two
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurIterationYD3D11Zone, "Vertical", RandomUniqueColor(), true);

					// Bind compute shader
					if (!_content->GetShader("CS_BlurVerticalOutline")->BindShader(_context))
					{
						ErrMsg("Failed to bind vertical blur compute shader!");
						return false;
					}

					// Bind render target
					ID3D11UnorderedAccessView *const uav[1] = { uavStageTwo };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					// Bind shader resource
					ID3D11ShaderResourceView *const srv[1] = { srvStageTwo };
					_context->CSSetShaderResources(0, 1, srv);


					// Send execution command
					_context->Dispatch(static_cast<UINT>(ceil(_viewportSceneView.Width / 8.0f)), static_cast<UINT>(ceil(_viewportSceneView.Height / 8.0f)), 1);


					// Unbind compute shader resources
					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
					_context->CSSetShaderResources(0, 1, nullSRV);

					// Unbind render target
					static ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}
			}

			// Unbind weight resource
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(3, 1, nullSRV);
		}
#endif
	}

	// Combine
	{
		ZoneNamedXNC(combinePostFXZone, "Combine Post-Processing", RandomUniqueColor(), true);
		TracyD3D11NamedZoneC(_tracyD3D11Context, combinePostFXD3D11Zone, "Combine Post-Processing", RandomUniqueColor(), true);

		std::string shaderName = "CS_CombineFX";

		ID3D11Buffer *const emissionSettings = _emissionSettingsBuffer.GetBuffer();
		_context->CSSetConstantBuffers(6, 1, &emissionSettings);

#ifdef DEBUG_BUILD
		ID3D11Buffer *const outlineSettings = _outlineSettingsBuffer.GetBuffer();
		_context->CSSetConstantBuffers(7, 1, &outlineSettings);

		if (_renderOutlineFX)
			shaderName = "CS_CombineOutlineFX";
#endif

		// Bind combine compute shader
		if (!_content->GetShader(shaderName)->BindShader(_context))
		{
			ErrMsg("Failed to bind fog compute shader!");
			return false;
		}

		// Bind combine render target
		ID3D11UnorderedAccessView *const uav[1] = { 
#ifdef USE_IMGUI
			_intermediateRT.GetUAV()
#else
			_uav.Get()
#endif
		};
		_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

		// Bind screen, emission & fog resources
		ID3D11ShaderResourceView *srvs[3] = { 
			_sceneRT.GetSRV(),
			_blurRT.GetSRV(),
			_fogRT.GetSRV(),
		};
		_context->CSSetShaderResources(0, 3, srvs);

#ifdef DEBUG_BUILD
		if (_renderOutlineFX)
		{
			ID3D11ShaderResourceView *outlineSrv[1] = { _outlineRT.GetSRV() };
			_context->CSSetShaderResources(3, 1, outlineSrv);
		}
#endif

		// Send execution command
		_context->Dispatch(static_cast<UINT>(ceil(_viewportSceneView.Width / 8.0f)), static_cast<UINT>(ceil(_viewportSceneView.Height / 8.0f)), 1);


		// Unbind shader resources
		memset(srvs, 0, sizeof(srvs));
		_context->CSSetShaderResources(0, 3, srvs);

#ifdef DEBUG_BUILD
		if (_renderOutlineFX)
		{
			ID3D11ShaderResourceView *outlineSrv[1] = { nullptr };
			_context->CSSetShaderResources(3, 1, outlineSrv);
		}
#endif

		// Unbind render target
		static ID3D11UnorderedAccessView *const nullUAV = nullptr;
		_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	return true;
}

#ifdef USE_IMGUI
bool Graphics::BeginUIRender()
{
	ZoneScopedXC(RandomUniqueColor());

	_context->OMSetRenderTargets(1, _rtv.GetAddressOf(), _dsView.Get());

	ImGuiIO &io = ImGui::GetIO();
	io.NavActive = false;

	auto &input = Input::Instance();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	_backgroundDockID = ImGui::DockSpaceOverViewport();

	if (input.IsCursorLocked())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX); // Hide mouse cursor when locked
	}

#ifdef USE_IMGUIZMO
	ImGuizmo::BeginFrame();
	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	ImGuizmo::SetGizmoSizeClipSpace(DebugData::Get().transformScale);

	auto windowPos = input.GetWindowPos();
	auto scenePos = input.GetSceneViewPos();
	auto sceneSize = input.GetSceneViewSize();	
	ImGuizmo::SetRect(windowPos.x + scenePos.x, windowPos.y + scenePos.y, sceneSize.x, sceneSize.y);
#endif

	return true;
}
bool Graphics::EndUIRender() const
{
	ZoneScopedXC(RandomUniqueColor());

	//_context->OMSetRenderTargets(1, _imGuiRtv.GetAddressOf(), nullptr);
	_context->OMSetRenderTargets(1, _rtv.GetAddressOf(), nullptr);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
	return true;
}

bool Graphics::RenderUI(TimeUtils &time)
{
	ZoneScopedXC(RandomUniqueColor());

	ImGui::SeparatorText("Info");
	dx::XMUINT2 resolution = Input::Instance().GetRealWindowSize();
	ImGui::Text(std::format("Resolution: {}x{}", resolution.x, resolution.y).c_str());

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Settings");

	// Render type dropdown
	{
		const char *renderTypeNames[(int)RenderType::COUNT] = {
			"Default",
			"Position",
			"Normal",
			"Ambient",
			"Diffuse",
			"Depth",
			"Shadow",
			"Reflection",
			"Reflectivity",
			"Specular",
			"Specular Strength",
			"UV Coordinates",
			"Occlusion",
			"Transparency",
			"Light Tiles"
		};

		ImGui::Text("Render Type:");
		ImGui::SameLine();
		if (ImGui::BeginCombo("##RenderTypeCombo", renderTypeNames[(int)_renderOutput], ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < (int)RenderType::COUNT; i++)
			{
				const bool isSelected = (i == (int)_renderOutput);
				if (ImGui::Selectable(renderTypeNames[i], isSelected))
					_renderOutput = (RenderType)i;

				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Skybox Shader dropdown
	{
		std::vector<std::string> shaderNames;
		_content->GetShaderNames(&shaderNames);

		ImGui::Text("Skybox:"); 
		ImGui::SameLine();
		if (ImGui::BeginCombo("##SkyboxShaderCombo", _skyboxPsID == CONTENT_NULL ? "None" : (shaderNames[_skyboxPsID].c_str())))
		{
			// Add "None" option
			{
				const bool isNoneSelected = (_skyboxPsID == CONTENT_NULL);
				if (ImGui::Selectable("None", isNoneSelected))
					SetSkyboxShaderID(CONTENT_NULL);

				if (isNoneSelected)
					ImGui::SetItemDefaultFocus();
			}

			for (int i = 0; i < shaderNames.size(); i++)
			{
				std::string &shaderName = shaderNames[i];

				if (!shaderName.starts_with("PS_Skybox"))
					continue; // Only show pixel shaders

				const bool isSelected = (_skyboxPsID == i);
				if (ImGui::Selectable(shaderName.c_str(), isSelected))
					SetSkyboxShaderID(i);

				if (isSelected)
					ImGui::SetItemDefaultFocus();

			}
			ImGui::EndCombo();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Environment Cubemap dropdown
	{
		std::vector<std::string> cubemapNames;
		_content->GetCubemapNames(&cubemapNames);

		ImGui::Text("Cubemap:"); 
		ImGui::SameLine();
		if (ImGui::BeginCombo("##EnvironmentCubemapCombo", _environmentCubemapID == CONTENT_NULL ? "None" : (cubemapNames[_environmentCubemapID].c_str()), ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < cubemapNames.size(); i++)
			{
				std::string &cubemapName = cubemapNames[i];

				const bool isSelected = (_environmentCubemapID == i);
				if (ImGui::Selectable(cubemapName.c_str(), isSelected))
					SetEnvironmentCubemapID(i);

				if (isSelected)
					ImGui::SetItemDefaultFocus();

			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ImGui::PayloadTags.at(ImGui::PayloadType::CUBEMAP)))
			{
				IM_ASSERT(payload->DataSize == sizeof(ImGui::ContentPayload));
				ImGui::ContentPayload contentPayload = *(const ImGui::ContentPayload *)payload->Data;

				SetEnvironmentCubemapID(contentPayload.id);
			}
			ImGui::EndDragDropTarget();
		}
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));

	// Set ambient color
	ImGui::ColorEdit3("Ambient Color", &(_currAmbientColor.x), ImGuiColorEditFlags_NoInputs);
	
	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Toggles");

	bool useVsync = (_vSync != 0);
	if (ImGui::Checkbox("V-Sync", &useVsync))
	{
		_vSync = useVsync ? 1 : 0;
	}

	if (useVsync)
	{
		ImGui::SameLine();
		int interval = (int)_vSync;
		if (ImGui::InputInt("##VSyncInterval", &interval, 1))
		{
			_vSync = (uint8_t)std::clamp(interval, 1, 4);
		}
	}

	ImGui::Checkbox("Wireframe Mode", &_wireframe);

	ImGui::Checkbox("Transparency", &_renderTransparency);

	ImGui::Checkbox("Overlay", &_renderOverlay);

	ImGui::Checkbox("Debug Drawing", &_renderDebugDraw);

	if (ImGui::Checkbox("Post Processing", &_renderPostFX))
	{
		if (!_renderPostFX)
		{
			// Clear post processing resources
			constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			_context->ClearRenderTargetView(_blurRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_fogRT.GetRTV(), clearColor);
			_context->ClearRenderTargetView(_outlineRT.GetRTV(), clearColor);
		}
	}

	ImGui::Dummy(ImVec2(0.0f, 6.0f));
	ImGui::SeparatorText("Properties");

	if (_renderPostFX)
	{
		if (ImGui::TreeNode("Post Processing Settings"))
		{
			if (ImGui::TreeNode("Volumetric Fog"))
			{
				if (ImGui::Checkbox("Render Fog", &_renderFogFX))
				{
					if (!_renderFogFX)
					{
						// Clear fog resources
						constexpr float clearFog[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_fogRT.GetRTV(), clearFog);
					}
				}

				if (_renderFogFX)
				{
					ImGui::PushID("FogSettings");

					ImGui::DragFloat("Thickness", &_currFogSettings.thickness, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Step Size", &_currFogSettings.stepSize, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					int minSteps = _currFogSettings.minSteps;
					if (ImGui::DragInt("Min Samples", &minSteps, 1, 0, _currFogSettings.maxSteps))
						_currFogSettings.minSteps = minSteps;
					ImGuiUtils::LockMouseOnActive();

					int maxSteps = _currFogSettings.maxSteps;
					if (ImGui::DragInt("Max Samples", &maxSteps, 1, minSteps))
						_currFogSettings.maxSteps = max(maxSteps, 0);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::DragInt("Blur Iterations", &_fogBlurIterations, 1, 0, 16))
					{
						constexpr float clearFog[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_fogRT.GetRTV(), clearFog);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _fogGaussWeights;
						bool modified = false;

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = max(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = max(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j];
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetFogGaussianWeightsBuffer(gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Emission"))
			{
				if (ImGui::Checkbox("Render Emission", &_renderEmissionFX))
				{
					if (!_renderEmissionFX)
					{
						// Clear emission resources
						constexpr float clearBlur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_blurRT.GetRTV(), clearBlur);
					}
				}

				if (_renderEmissionFX)
				{
					ImGui::PushID("EmissionSettings");

					ImGui::DragFloat("Strength", &_currEmissionSettings.strength, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Exponent", &_currEmissionSettings.exponent, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Threshold", &_currEmissionSettings.threshold, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::DragInt("Blur Iterations", &_emissionBlurIterations, 1, 0, 16))
					{
						constexpr float clearBlur[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_blurRT.GetRTV(), clearBlur);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _emissionGaussWeights;
						bool modified = false;

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = max(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = max(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j];
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetEmissionGaussianWeightsBuffer(gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Outline"))
			{
				if (ImGui::Checkbox("Render Outline", &_renderOutlineFX))
				{
					if (!_renderOutlineFX)
					{
						// Clear outline resources
						constexpr float clearOutline = 0.0f;
						_context->ClearRenderTargetView(_outlineRT.GetRTV(), &clearOutline);
					}
				}

				if (_renderOutlineFX)
				{
					ImGui::PushID("OutlineSettings");

					ImGui::ColorEdit4("Color", &_outlineSettings.color.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);

					ImGui::DragFloat("Strength", &_outlineSettings.strength, 0.001f, 0.0001f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::DragFloat("Exponent", &_outlineSettings.exponent, 0.001f, 0.01f);
					ImGuiUtils::LockMouseOnActive();

					ImGui::SliderFloat("Smoothing", &_outlineSettings.smoothing, 0.0f, 1.0f);

					if (ImGui::DragInt("Blur Iterations", &_outlineBlurIterations, 1, 0, 16))
					{
						constexpr float clearOutline[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						_context->ClearRenderTargetView(_outlineRT.GetRTV(), clearOutline);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::TreeNode("Blur Weights"))
					{
						std::vector<float> &gaussWeights = _outlineGaussWeights;
						bool modified = false;

						static float valueRange[2] = { 0.0f, 1.0f };
						if (ImGui::DragFloat2("Range", valueRange, 0.01f))
							valueRange[1] = max(valueRange[1], valueRange[0]);
						ImGuiUtils::LockMouseOnActive();

						static bool normalizeWeights = true;
						if (ImGui::Checkbox("Normalize", &normalizeWeights))
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}

								modified = true;
							}
						}
						ImGui::SetItemTooltip("Normalize the weights to sum to 1.0.");

						int weightCount = gaussWeights.size();
						if (ImGui::InputInt("Weight Count", &weightCount))
						{
							weightCount = max(weightCount, 1);

							if (weightCount != gaussWeights.size())
							{
								gaussWeights.resize(weightCount);
								modified = true;
							}
						}

						ImGui::Separator();
						ImGui::BeginChild("Weights", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
						for (int i = 0; i < weightCount; i++)
						{
							ImGui::PushID(i);

							if (ImGui::SliderFloat("", &gaussWeights[i], valueRange[0], valueRange[1]))
							{
								modified = true;

								// Normalize weights without changing the modified weight
								if (normalizeWeights)
								{
									float restSum = 1.0f - gaussWeights[i];

									if (std::abs(restSum) > 0.001f)
									{
										float sum = 0.0f;
										for (int j = 0; j < weightCount; j++)
										{
											if (j != i)
												sum += gaussWeights[j];
										}

										if (sum > 0.0f)
										{
											float invScaledSum = restSum / sum;

											for (int j = 0; j < weightCount; j++)
											{
												if (j != i)
													gaussWeights[j] *= invScaledSum;
											}
										}
									}
								}

							}

							ImGui::PopID();
						}
						ImGui::EndChild();
						ImGui::Separator();

						static bool applyContinuously = false;
						if (ImGui::Checkbox("Apply Continuously", &applyContinuously))
							modified = true;
						ImGui::SetItemTooltip("Apply the new weights as soon as they are modified.\nUseful for seeing changes in real-time.");

						bool apply = applyContinuously && modified;

						if (!applyContinuously)
						{
							if (ImGui::Button("Apply"))
								apply = true;
						}

						if (apply)
						{
							if (normalizeWeights)
							{
								float sum = 0.0f;
								for (float &weight : gaussWeights)
									sum += weight;

								if (sum > 0.0f)
								{
									for (float &weight : gaussWeights)
										weight /= sum;
								}
							}

							SetGaussianWeightsBuffer(&_outlineGaussianWeightsBuffer, gaussWeights.data(), gaussWeights.size());
						}

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}
	
	if (ImGui::TreeNode("Render Distance Fog"))
	{
		ImGui::ColorEdit4("##RenderDistanceFogColor", &_generalDataSettings.fadeoutColor.x, ImGuiColorEditFlags_AlphaBar);

		ImGui::SliderFloat("Begin Depth##RenderDistanceFogBeginDepth", &_generalDataSettings.fadeoutDepthBegin, 0.0f, 1.0f);

		if (ImGui::DragFloat("Exponent##RenderDistanceFogExponent", &_generalDataSettings.fadeoutExponent, 0.01f))
			_generalDataSettings.fadeoutExponent = max(_generalDataSettings.fadeoutExponent, 0.01f);
		ImGuiUtils::LockMouseOnActive();

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Distortion"))
	{
		static dx::XMFLOAT3 dO = { 0, 0, 0 };
		ImGui::InputFloat("Distortion Strength", &_distortionSettings.distortionStrength);
		ImGui::InputFloat3("Distortion Origin", &dO.x);
		_distortionSettings.distortionOrigin = dO;
		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Shadow Rasterization"))
	{
		bool hasChanged = false;

		int fillMode = (int)_shadowRasterizerDesc.FillMode - 2;
		int cullMode = (int)_shadowRasterizerDesc.CullMode - 1;
		bool frontCounterClockwise = _shadowRasterizerDesc.FrontCounterClockwise;
		int depthBias = _shadowRasterizerDesc.DepthBias;
		float depthBiasClamp = _shadowRasterizerDesc.DepthBiasClamp;
		float slopeScaledDepthBias = _shadowRasterizerDesc.SlopeScaledDepthBias;
		bool depthClipEnable = _shadowRasterizerDesc.DepthClipEnable;
		bool scissorEnable = _shadowRasterizerDesc.ScissorEnable;
		bool multisampleEnable = _shadowRasterizerDesc.MultisampleEnable;
		bool antialiasedLineEnable = _shadowRasterizerDesc.AntialiasedLineEnable;

		if (ImGui::Combo("Fill Mode", &fillMode, "Wireframe\0Solid\0"))
		{
			_shadowRasterizerDesc.FillMode = (D3D11_FILL_MODE)(fillMode + 2);
			hasChanged = true;
		}

		if (ImGui::Combo("Cull Mode", &cullMode, "None\0Front\0Back\0"))
		{
			_shadowRasterizerDesc.CullMode = (D3D11_CULL_MODE)(cullMode + 1);
			hasChanged = true;
		}

		if (ImGui::Checkbox("Front Counter Clockwise", &frontCounterClockwise))
		{
			_shadowRasterizerDesc.FrontCounterClockwise = frontCounterClockwise;
			hasChanged = true;
		}

		if (ImGui::DragInt("Depth Bias", &depthBias, 0.01f))
		{
			_shadowRasterizerDesc.DepthBias = depthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Depth Bias Clamp", &depthBiasClamp, 0.001f))
		{
			_shadowRasterizerDesc.DepthBiasClamp = depthBiasClamp;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::DragFloat("Slope Scaled Depth Bias", &slopeScaledDepthBias, 0.01f))
		{
			_shadowRasterizerDesc.SlopeScaledDepthBias = slopeScaledDepthBias;
			hasChanged = true;
		}
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Checkbox("Depth Clip", &depthClipEnable))
		{
			_shadowRasterizerDesc.DepthClipEnable = depthClipEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Scissor", &scissorEnable))
		{
			_shadowRasterizerDesc.ScissorEnable = scissorEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Multisample", &multisampleEnable))
		{
			_shadowRasterizerDesc.MultisampleEnable = multisampleEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Antialiased Line", &antialiasedLineEnable))
		{
			_shadowRasterizerDesc.AntialiasedLineEnable = antialiasedLineEnable;
			hasChanged = true;
		}

		ImGui::Dummy({ 0.0f, 6.0f });

		static bool applyContinuously = false;
		ImGui::Checkbox("Apply Continuously", &applyContinuously);

		bool applyPreset = false;
		if (!applyContinuously)
		{
			if (ImGui::Button("Apply Preset"))
				applyPreset = true;
		}

		static bool invalidPreset = false;
		if ((applyContinuously && hasChanged) || applyPreset)
		{
			ID3D11RasterizerState *tempRasterizer;

			invalidPreset = FAILED(_device->CreateRasterizerState(&_shadowRasterizerDesc, &tempRasterizer));

			if (!invalidPreset)
			{
				_shadowRasterizer.Reset();
				_shadowRasterizer = tempRasterizer;
			}
		}

		if (invalidPreset)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Invalid Preset!");
			ImGui::PopStyleColor();
		}

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Transparency Blending"))
	{
		static std::string blendStateName = "";
		ImGui::InputText("Blend State Name", &blendStateName);

		bool hasChanged = false;

		static bool alphaToCoverageEnable = _transparentBlendDesc.AlphaToCoverageEnable;
		static bool independentBlendEnable = _transparentBlendDesc.IndependentBlendEnable;
		static bool blendEnable = _transparentBlendDesc.RenderTarget[0].BlendEnable;
		
		static int srcBlend = 4;
		static int destBlend = 1;
		static int blendOp = 0;
		
		static int srcBlendAlpha = 4;
		static int destBlendAlpha = 5;
		static int blendOpAlpha = 4;

		static int renderTargetWriteMask = _transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask;

		constexpr D3D11_BLEND blendValues[] = {
			D3D11_BLEND_ZERO,
			D3D11_BLEND_ONE,
			D3D11_BLEND_SRC_COLOR,
			D3D11_BLEND_INV_SRC_COLOR,
			D3D11_BLEND_SRC_ALPHA,
			D3D11_BLEND_INV_SRC_ALPHA,
			D3D11_BLEND_DEST_ALPHA,
			D3D11_BLEND_INV_DEST_ALPHA,
			D3D11_BLEND_DEST_COLOR,
			D3D11_BLEND_INV_DEST_COLOR,
			D3D11_BLEND_SRC_ALPHA_SAT,
			D3D11_BLEND_BLEND_FACTOR,
			D3D11_BLEND_INV_BLEND_FACTOR,
			D3D11_BLEND_SRC1_COLOR,
			D3D11_BLEND_INV_SRC1_COLOR,
			D3D11_BLEND_SRC1_ALPHA,
			D3D11_BLEND_INV_SRC1_ALPHA
		};
		constexpr char blendNames[] = "ZERO\0ONE\0SRC_COLOR\0INV_SRC_COLOR\0SRC_ALPHA\0INV_SRC_ALPHA\0DEST_ALPHA\0INV_DEST_ALPHA\0DEST_COLOR\0INV_DEST_COLOR\0SRC_ALPHA_SAT\0BLEND_FACTOR\0INV_BLEND_FACTOR\0SRC1_COLOR\0INV_SRC1_COLOR\0SRC1_ALPHA\0INV_SRC1_ALPHA\0";
		
		constexpr D3D11_BLEND_OP blendOpValues[] = {
			D3D11_BLEND_OP_ADD,
			D3D11_BLEND_OP_SUBTRACT,
			D3D11_BLEND_OP_REV_SUBTRACT,
			D3D11_BLEND_OP_MIN,
			D3D11_BLEND_OP_MAX
		};
		constexpr char blendOpNames[] = "ADD\0SUBTRACT\0REV_SUBTRACT\0MIN\0MAX\0";

		if (ImGui::Checkbox("Alpha to Coverage", &alphaToCoverageEnable))
		{
			_transparentBlendDesc.AlphaToCoverageEnable = alphaToCoverageEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Independent Blend", &independentBlendEnable))
		{
			_transparentBlendDesc.IndependentBlendEnable = independentBlendEnable;
			hasChanged = true;
		}

		if (ImGui::Checkbox("Blend", &blendEnable))
		{
			_transparentBlendDesc.RenderTarget[0].BlendEnable = blendEnable ? 1 : 0;
			hasChanged = true;
		}

		if (ImGui::Combo("Source Blend", &srcBlend, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].SrcBlend = blendValues[srcBlend];
			hasChanged = true;
		}
		
		if (ImGui::Combo("Destination Blend", &destBlend, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].DestBlend = blendValues[destBlend];
			hasChanged = true;
		}

		if (ImGui::Combo("Blend Operation", &blendOp, blendOpNames))
		{
			_transparentBlendDesc.RenderTarget[0].BlendOp = blendOpValues[blendOp];
			hasChanged = true;
		}

		if (ImGui::Combo("Source Blend Alpha", &srcBlendAlpha, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].SrcBlendAlpha = blendValues[srcBlendAlpha];
			hasChanged = true;
		}

		if (ImGui::Combo("Destination Blend Alpha", &destBlendAlpha, blendNames))
		{
			_transparentBlendDesc.RenderTarget[0].DestBlendAlpha = blendValues[destBlendAlpha];
			hasChanged = true;
		}

		if (ImGui::Combo("Blend Operation Alpha", &blendOpAlpha, blendOpNames))
		{
			_transparentBlendDesc.RenderTarget[0].BlendOpAlpha = blendOpValues[blendOpAlpha];
			hasChanged = true;
		}

		bool renderTargetWriteMaskR = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_RED;
		if (ImGui::Checkbox("Write Red", &renderTargetWriteMaskR))
		{
			renderTargetWriteMask = renderTargetWriteMaskR ? 
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_RED : 
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_RED;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskG = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_GREEN;
		if (ImGui::Checkbox("Write Green", &renderTargetWriteMaskG))
		{
			renderTargetWriteMask = renderTargetWriteMaskG ? 
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_GREEN : 
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_GREEN;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskB = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_BLUE;
		if (ImGui::Checkbox("Write Blue", &renderTargetWriteMaskB))
		{
			renderTargetWriteMask = renderTargetWriteMaskB ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_BLUE :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_BLUE;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		bool renderTargetWriteMaskA = renderTargetWriteMask & D3D11_COLOR_WRITE_ENABLE_ALPHA;
		if (ImGui::Checkbox("Write Alpha", &renderTargetWriteMaskA))
		{
			renderTargetWriteMask = renderTargetWriteMaskA ?
				renderTargetWriteMask | D3D11_COLOR_WRITE_ENABLE_ALPHA :
				renderTargetWriteMask & ~D3D11_COLOR_WRITE_ENABLE_ALPHA;

			_transparentBlendDesc.RenderTarget[0].RenderTargetWriteMask = renderTargetWriteMask;
			hasChanged = true;
		}

		ImGui::Dummy({ 0.0f, 6.0f });

		bool applyPreset = false;
		if (ImGui::Button("Apply Preset"))
			applyPreset = true;

		static bool invalidPreset = false;
		if (applyPreset)
		{
			auto *blendStatePtr = _content->GetBlendStateAddress(std::format("{}", blendStateName));
			if (blendStatePtr)
			{
				// Replace existing blend state
				if (FAILED(_device->CreateBlendState(&_transparentBlendDesc, blendStatePtr->ReleaseAndGetAddressOf())))
				{
					invalidPreset = true;
				}
				else
				{
					invalidPreset = false;
				}
			}
			else
			{
				// Create new blend state
				UINT ret = _content->AddBlendState(_device, std::format("{}", blendStateName), _transparentBlendDesc);
				invalidPreset = ret == 0;
			}
		}

		if (invalidPreset)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			ImGui::Text("Invalid Preset!");
			ImGui::PopStyleColor();
		}

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Light Draw Info"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;
		childFlags |= ImGuiChildFlags_ResizeY;

		ImGui::BeginChild("Entity Hierarchy", ImVec2(0, 300), childFlags);

		ImGui::Text(std::format("Main Draws: {}", _currViewCamera->GetCullCount()).c_str());
		for (UINT i = 0; i < _currSpotLightCollection.Get()->GetNrOfLights(); i++)
		{
			const CameraBehaviour *spotlightCamera = _currSpotLightCollection.Get()->GetLightBehaviour(i)->GetShadowCamera();
			ImGui::Text(std::format("Spotlight #{} Draws: {}", i, spotlightCamera->GetCullCount()).c_str());
		}

		for (UINT i = 0; i < _currPointLightCollection.Get()->GetNrOfLights(); i++)
		{
			const CameraCubeBehaviour *pointlightCamera = _currPointLightCollection.Get()->GetLightBehaviour(i)->GetShadowCameraCube();
			ImGui::Text(std::format("Pointlight #{} Draws: {}", i, pointlightCamera->GetCullCount()).c_str());
		}

		ImGui::EndChild();
		ImGui::TreePop();
	}

	return true;
}
bool Graphics::RenderSceneView()
{
	ZoneScopedXC(RandomUniqueColor());

	const float padding = 0.0f;

	Input &input = Input::Instance();
	DebugDrawer &debugDrawer = DebugDrawer::Instance();

	dx::XMINT2 wPos = input.GetWindowPos();
	dx::XMUINT2 wSize = input.GetWindowSize();
	dx::XMUINT2 wRealSize = input.GetRealWindowSize();

	dx::XMINT2 sPos = input.GetScreenPos();
	dx::XMUINT2 sSize = input.GetScreenSize();

	MouseState mState = input.GetMouse();
	dx::XMFLOAT2 localMousePos = input.GetLocalMousePos();

	ImVec2 mPos = ImGui::GetMousePos() - ImVec2(wPos.x, wPos.y);
	mPos /= ImVec2((float)sSize.x, (float)sSize.y);
	mPos *= ImVec2((float)wSize.x, (float)wSize.y);

	ImVec2 displayPos = { (float)sPos.x, (float)sPos.y };
	ImVec2 displaySize = { (float)sSize.x, (float)sSize.y };

	ImVec2 appWindowPos = { (float)wPos.x, (float)wPos.y };
	ImVec2 appWindowSize = { (float)wRealSize.x, (float)wRealSize.y };

	ImVec2 sceneWindowPos = ImGui::GetWindowPos();
	ImVec2 sceneWindowSize = ImGui::GetWindowSize();
	ImVec2 sceneWindowLocalPos = sceneWindowPos - appWindowPos;

	ImVec2 viewRegionMin = ImGui::GetWindowContentRegionMin();
	ImVec2 viewRegionMax = ImGui::GetWindowContentRegionMax();
	ImVec2 viewRegionSize = viewRegionMax - viewRegionMin;

	ImVec2 sceneViewPos, sceneViewSize;

	auto &debugData = DebugData::Get();
	if (debugData.stretchToFitView)
	{
		dx::XMFLOAT2 sceneViewLocalPos = {
			sceneWindowLocalPos.x + viewRegionMin.x + padding,
			sceneWindowLocalPos.y + viewRegionMin.y + padding
		};
		dx::XMFLOAT2 sceneViewLocalSize = {
			sceneWindowLocalPos.x + viewRegionMax.x - padding - sceneViewLocalPos.x,
			sceneWindowLocalPos.y + viewRegionMax.y - padding - sceneViewLocalPos.y
		};
		
		sceneViewPos = ImGui::GetCursorPos();
		sceneViewSize = viewRegionSize;

		debugData.sceneViewSizeX = sceneViewSize.x;
		debugData.sceneViewSizeY = sceneViewSize.y;

		input.SetSceneViewPos(dx::XMINT2((int)sceneViewLocalPos.x - padding, (int)sceneViewLocalPos.y - padding));
		input.SetSceneViewSize(dx::XMUINT2((uint32_t)(sceneViewLocalSize.x + 2.0f * padding), (uint32_t)(sceneViewLocalSize.y + 2.0f * padding)));
	}
	else
	{
		dx::XMUINT2 srSize = input.GetSceneRenderSize();

		float renderAspect = (float)srSize.x / (float)srSize.y;
		float viewAspect = viewRegionSize.x / viewRegionSize.y;

		ImVec2 scaledBounds = ImVec2(0, 0);
		if (renderAspect > viewAspect)
		{
			scaledBounds.x = viewRegionSize.x;
			scaledBounds.y = viewRegionSize.x / renderAspect;
		}
		else
		{
			scaledBounds.x = viewRegionSize.y * renderAspect;
			scaledBounds.y = viewRegionSize.y;
		}

		ImVec2 viewRegionCenter = (viewRegionMin + viewRegionMax) * 0.5f;
		ImVec2 renderTextureCorner = viewRegionCenter - (scaledBounds * 0.5f);

		sceneViewPos = renderTextureCorner;
		sceneViewSize = scaledBounds;

		input.SetSceneViewPos(dx::XMINT2((int)(sceneWindowLocalPos.x + renderTextureCorner.x), (int)(sceneWindowLocalPos.y + renderTextureCorner.y)));
		input.SetSceneViewSize(dx::XMUINT2((uint32_t)scaledBounds.x, (uint32_t)scaledBounds.y));
	}

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration;
	windowFlags |= ImGuiWindowFlags_NoMove;
	windowFlags |= ImGuiWindowFlags_NoSavedSettings;
	windowFlags |= ImGuiWindowFlags_NoBackground;
	windowFlags |= ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::SetCursorPos(sceneViewPos);
	ImGui::BeginChild("SceneViewChild", sceneViewSize, ImGuiChildFlags_None, windowFlags);

	ImGui::SetCursorPos({0, 0});
	ImGui::Image((ImTextureID)_intermediateRT.GetSRV(), sceneViewSize);

	if (ImGui::IsWindowFocused(ImGuiHoveredFlags_None))
		input.SetKeyboardAbsorbed(false);
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None))
		input.SetMouseAbsorbed(false);

	if (!notifications.empty())
	{
		float dTime = ImGui::GetIO().DeltaTime;
		const float offset = 8.0f;

		ImGui::SetCursorPosY(offset);
		for (int i = 0; i < notifications.size(); i++)
		{
			NotificationMessage &notification = notifications[i];

			ImVec4 severityColor;
			switch (notification.severity)
			{
			case 0:  severityColor = { 1.0f, 1.0f, 1.0f, 1 }; break; // Default	- White
			case 1:	 severityColor = { 0.0f, 1.0f, 0.0f, 1 }; break; // Info	- Green
			case 2:	 severityColor = { 1.0f, 1.0f, 0.0f, 1 }; break; // Notice	- Yellow
			case 3:	 severityColor = { 1.0f, 0.5f, 0.0f, 1 }; break; // Warning	- Orange
			case 4:	 severityColor = { 1.0f, 0.0f, 0.0f, 1 }; break; // Problem	- Red
			default: severityColor = { 1.0f, 0.0f, 1.0f, 1 }; break; // Other	- Magenta
			}

			ImGui::SetCursorPosX(offset);
			ImGui::TextColored(severityColor, notification.message.data());

			if (notification.duration >= 0.0f)
			{
				notification.duration -= dTime;
				if (notification.duration <= 0.0f)
				{
					notifications.erase(notifications.begin() + i);
					i--;
				}
			}
		}
	}

#ifdef USE_IMGUIZMO
		ImGuizmo::SetDrawlist(ImGui::GetCurrentWindow()->DrawList);
#endif

	ImGui::EndChild();

	return true;
}

ImGuiID Graphics::GetBackgroundDockID() const
{
	return _backgroundDockID;
}

#endif

bool Graphics::ScreenSpaceRender()
{
	if (_renderDebugDraw)
	{
		if (!DebugDrawer::Instance().RenderScreenSpace(_rtv.Get(), _dsView.Get(), &_viewport))
		{
			ErrMsg("Failed to render screen-space debug drawer!");
			return false;
		}
	}
	DebugDrawer::Instance().ClearScreenSpace();

	return true;
}

bool Graphics::EndFrame()
{
	ZoneScopedC(RandomUniqueColor());
	TracyD3D11ZoneC(_tracyD3D11Context, "Present", RandomUniqueColor());

#ifdef TRACY_SCREEN_CAPTURE
	if (TracyIsStarted && TracyIsConnected)
	{
		ZoneNamedNC(screenCaptureZone, "Tracy Capture Screen", RandomUniqueColor(), true);

		// Get the back buffer from swap chain
		ID3D11Texture2D *backBuffer;
		if (FAILED(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&backBuffer)))
		{
			ErrMsg("Failed to get back buffer from swap chain for screen capture!");
			return false;
		}

		// Create a texture that can be used as shader resource
		D3D11_TEXTURE2D_DESC texDesc;
		backBuffer->GetDesc(&texDesc);
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		ID3D11Texture2D *shaderResourceTex;
		if (FAILED(_device->CreateTexture2D(&texDesc, nullptr, &shaderResourceTex)))
		{
			ErrMsg("Failed to create shader resource texture for screen capture!");
			backBuffer->Release();
			return false;
		}

		// Copy the back buffer to your texture
		_context->CopyResource(shaderResourceTex, backBuffer);

		// Create the SRV
		ID3D11ShaderResourceView *srv;
		
		if (FAILED(_device->CreateShaderResourceView(shaderResourceTex, nullptr, &srv)))
		{
			ErrMsg("Failed to create shader resource view for screen capture!");
			backBuffer->Release();
			shaderResourceTex->Release();
			return false;
		}

		// Downsample screen to tracy capture
		{
			// Bind compute shader
			if (!_content->GetShader("CS_DownsampleCheap")->BindShader(_context))
			{
				ErrMsg("Failed to bind downsample compute shader!");
				return false;
			}

			// Bind render target
			ID3D11UnorderedAccessView *const uav[1] = { _tracyCaptureRT.GetUAV() };
			_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

			// Bind shader resource
			ID3D11ShaderResourceView *const ptrSRV[1] = { srv };
			_context->CSSetShaderResources(0, 1, ptrSRV);

			
			// Send execution command
			_context->Dispatch(static_cast<UINT>(ceil(_tracyCaptureWidth / 8.0f)), static_cast<UINT>(ceil(_tracyCaptureHeight / 8.0f)), 1);


			// Unbind compute shader resources
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(0, 1, nullSRV);

			// Unbind render target
			static ID3D11UnorderedAccessView *const nullUAV = nullptr;
			_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		}

		// Clean up temporary resources
		backBuffer->Release();
		shaderResourceTex->Release();
		srv->Release();

		ID3D11Texture2D *sourceTexture = _tracyCaptureRT.GetTexture();
		ID3D11Texture2D *stagingTexture = nullptr;

		D3D11_TEXTURE2D_DESC desc;
		sourceTexture->GetDesc(&desc);

		// Modify description for staging
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = 0;

		if (FAILED(_device->CreateTexture2D(&desc, nullptr, &stagingTexture)))
		{
			ErrMsg("Failed to create staging texture for screen capture!");
			return false;
		}

		_context->CopyResource(stagingTexture, sourceTexture);

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (FAILED(_context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			ErrMsg("Failed to map texture for screen capture!");
			return false;
		}

		const uint8_t *pixels = static_cast<const uint8_t *>(mapped.pData);

		// Capture the screen image
		FrameImage(
			pixels,
			_tracyCaptureWidth,
			_tracyCaptureHeight,
			0,
			false
		);

		// Unmap & release when done
		_context->Unmap(stagingTexture, 0);
		stagingTexture->Release();
	}
#endif

	if (FAILED(_swapChain->Present(_vSync, 0)))
	{
		TracyD3D11Collect(_tracyD3D11Context);
		return true;
		ErrMsg("Failed to present geometry!");
		return false;
	}

	TracyD3D11Collect(_tracyD3D11Context);

	return true;
}
bool Graphics::ResetRenderState()
{
	ZoneScopedXC(RandomUniqueColor());

	_currViewCamera->ResetRenderQueue();
	TracyPlot("View Cull Count", (int64_t)_currViewCamera->GetCullCount());

#ifdef DEBUG_BUILD
	_outlinedEntities.clear();
#endif

	if (_currSpotLightCollection.IsValid())
	{
		auto &collection = *_currSpotLightCollection.Get();

		for (UINT i = 0; i < collection.GetNrOfLights(); i++)
			collection.GetLightBehaviour(i)->GetShadowCamera()->ResetRenderQueue();
	}

	if (_currPointLightCollection.IsValid())
	{
		auto &collection = *_currPointLightCollection.Get();

		for (UINT i = 0; i < collection.GetNrOfLights(); i++)
			collection.GetLightBehaviour(i)->GetShadowCameraCube()->ResetRenderQueue();
	}

	_currInputLayoutID	= CONTENT_NULL;
	_currMeshID			= CONTENT_NULL;
	_currVsID			= CONTENT_NULL;
	_currPsID			= CONTENT_NULL;
	_currTexID			= CONTENT_NULL;
	_currNormalID		= CONTENT_NULL;
	_currSpecularID		= CONTENT_NULL;
	_currGlossinessID	= CONTENT_NULL;
	_currAmbientID		= CONTENT_NULL;
	_currReflectiveID	= CONTENT_NULL;
	_currOcclusionID	= CONTENT_NULL;
	_currSamplerID		= CONTENT_NULL;
	_currBlendStateID	= CONTENT_NULL;
	_currVsID			= CONTENT_NULL;
	_currPsID			= CONTENT_NULL;

	_isRendering = false;
	return true;
}
