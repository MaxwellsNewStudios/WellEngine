#pragma once

#include <d3d11.h>
#include <array>
#include <wrl/client.h>

#include "./Lighting/SpotLightCollection.h"
#include "./Lighting/PointLightCollection.h"
#include "Source/Engine/Content/Content.h"
#include "Source/Engine/Timing/TimeUtils.h"
#include "Source/Engine/Utils/RepeatTracker.h"
#include "Source/Engine/D3D/RenderTargetD3D11.h"
#include "Source/Engine/Window/Window.h"
#include "Source/Engine/Debug/DebugDrawer.h"
#include "Source/Game/Behaviours/Rendering/Camera/CameraBehaviour.h"

namespace we = WellEngine;

enum class RenderType
{
	DEFAULT,
	POSITION,
	NORMAL,
	AMBIENT,
	DIFFUSE,
	DEPTH,
	SHADOW,
	REFLECTION,
	REFLECTIVITY,
	SPECULAR,
	SPECULAR_STRENGTH,
	UV_COORDS,
	OCCLUSION,
	TRANSPARENCY,
	LIGHT_TILES,
	OVERDRAW,

	COUNT
};

enum LightType
{
	SPOTLIGHT,
	POINTLIGHT,
	SIMPLE_SPOTLIGHT,
	SIMPLE_POINTLIGHT,
};

struct LightTile
{
	UINT spotlights[MAX_LIGHTS]{};
	UINT spotlightCount = 0;

	UINT pointlights[MAX_LIGHTS]{};
	UINT pointlightCount = 0;

	UINT simpleSpotlights[MAX_LIGHTS]{};
	UINT simpleSpotlightCount = 0;

	UINT simplePointlights[MAX_LIGHTS]{};
	UINT simplePointlightCount = 0;
};

struct GeneralDataBuffer
{
	float time = 0.0f;
	float deltaTime = 0.0f;
	int randInt = 0;
	float randNorm = 0.0f;

	// Distance based fade-out
	dx::XMFLOAT4 fadeoutColor = { 0, 0, 0, 1 };
	float fadeoutDepthBegin = 0.5f;
	float fadeoutExponent = 2.0f;

	float _padding[2];
};

struct FogSettingsBuffer
{
	float	thickness = 0.2f;
	float	sampleBias = 1.5f;
	int		maxSteps = 64;
	float	depthFadeBegin = 0.5f;
	float	depthFadeEnd = 1.0f;
	float	depthFadeExp = 1.0f;

	float	_padding[2];
};

struct EmissionSettingsBuffer
{
	float strength = 1.0f;
	float exponent = 0.5f;
	float threshold = 1.0f;

	float _padding[1];
};

struct DistortionSettingsBuffer
{
	dx::XMFLOAT3 distortionOrigin = { 0.0f, 0.0f, 0.0f };
	float distortionStrength = 0.0f;
};

struct DepthOfFieldSettingsBuffer
{
	float focalPlane = 0.25f;
	float aperture = 15;
	float imageDistance = 1;

	float _padding[1];
};

struct OutlineSettingsBuffer
{
	dx::XMFLOAT4 color = { 0.2f, 0.7f, 1.0f, 1.0f };
	float strength = 2.5f;
	float exponent = 1.0f;
	float smoothing = 0.8f;

	float _padding[1];
};

#ifdef USE_IMGUI
struct NotificationMessage
{
	enum class SeverityColor
	{
		White = 0, // Default
		Green,	   // Info
		Yellow,	   // Notice
		Orange,	   // Warning
		Red,	   // Problem

		// Misc
		Blue,
		Magenta,
		Cyan,
		Black,
		Gray,

		// Special
		Rainbow,
	};

	std::string message, id = "";
	SeverityColor severity;
	float duration;
	float fontSize;
	float blinkFrequency = -1.0f;

	NotificationMessage(std::string message, SeverityColor severity = SeverityColor::White, float duration = -1.0f, float fontSize = 16.0f, float blinkFrequency = -1.0f)
		: message(std::move(message)), severity(severity), duration(duration), fontSize(fontSize), blinkFrequency(blinkFrequency) {	}

	NotificationMessage(std::string message, int severity = 0, float duration = -1.0f, float fontSize = 16.0f, float blinkFrequency = -1.0f)
		: message(std::move(message)), severity((SeverityColor)severity), duration(duration), fontSize(fontSize), blinkFrequency(blinkFrequency) { }
};
#endif

/// Handles rendering of the scene and the GUI.
class Graphics
{
private:
	bool _isSetup		= false;
	bool _isRendering	= false;

	ID3D11Device *_device			= nullptr;
	ID3D11DeviceContext	*_context	= nullptr;
	Content	*_content				= nullptr;

	ComPtr<IDXGISwapChain>				_swapChain = nullptr;
	ComPtr<ID3D11RenderTargetView>		_rtv = nullptr;
	ComPtr<ID3D11Texture2D>				_dsTexture = nullptr;
	ComPtr<ID3D11DepthStencilView>		_dsView = nullptr;
#ifdef USE_IMGUI
	ComPtr<ID3D11Texture2D>				_sceneDsTexture = nullptr;
	ComPtr<ID3D11DepthStencilView>		_sceneDsView = nullptr;
#endif
	ComPtr<ID3D11UnorderedAccessView>	_uav = nullptr;
	ComPtr<ID3D11DepthStencilState>		_ndss = nullptr; // Normal depth stencil state
	ComPtr<ID3D11DepthStencilState>		_rdss = nullptr; // Reverse Z depth stencil state
	ComPtr<ID3D11DepthStencilState>		_tdss = nullptr; // Transparent depth stencil state
	ComPtr<ID3D11DepthStencilState>		_nulldss = nullptr;

	ID3D11Texture2D			**_sceneDsTexturePtr = nullptr;
	ID3D11DepthStencilView	**_sceneDsViewPtr = nullptr;

	D3D11_VIEWPORT _viewport = { };
	D3D11_VIEWPORT _viewportSceneView = { };
	D3D11_VIEWPORT _viewportBlur = { };
	D3D11_VIEWPORT _viewportFog = { };
	D3D11_VIEWPORT _viewportDof = { };

	float _emissionResolutionScale = 0.25f;
	float _fogResolutionScale = 0.25f;
	float _dofResolutionScale = 0.5f;

	ComPtr<ID3D11RasterizerState> _defaultRasterizer = nullptr;
	ComPtr<ID3D11RasterizerState> _wireframeRasterizer = nullptr;
	ComPtr<ID3D11RasterizerState> _shadowRasterizer = nullptr;

	bool _renderTransparency = true;
	bool _renderOverlay = true;
	bool _renderDebugDraw = true;
	bool _renderPostFX = true;
	bool _renderFogFX = true;
	bool _renderEmissionFX = true;
	bool _renderDepthOfFieldFX = false;
	bool _renderOutlineFX = true;
	bool _wireframe = false;
	uint8_t _vSync = 1;

#ifdef USE_IMGUI
	/// Render the scene to an intermediate texture, to then be rendered in an ImGui window.
	RenderTargetD3D11 _intermediateRT;
	ImGuiID _backgroundDockID = 0;
#endif

	// The view camera is rendered to sceneRT, the alpha channel is used for emission strength and depth is rendered to depthRT.
	RenderTargetD3D11 _sceneRT; // RGBA
	RenderTargetD3D11 _depthRT; // R
	RenderTargetD3D11 _emissionRT; // RGB
	RenderTargetD3D11 _blurRT; // RGB
	RenderTargetD3D11 _intermediateBlurRT; // RGB
	RenderTargetD3D11 _fogRT; // RGBA
	RenderTargetD3D11 _intermediateFogRT; // RGBA
	RenderTargetD3D11 _cocRT; // R
	RenderTargetD3D11 _dofSharpRT; // RGBA
	RenderTargetD3D11 _dofHalfBlur1RT; // RGB
	RenderTargetD3D11 _dofHalfBlur2RT; // RGB
	RenderTargetD3D11 _dofFullBlurRT; // RGBA

	RenderType _renderOutput = RenderType::DEFAULT;

	dx::XMFLOAT4A _currAmbientColor = { 0.01f, 0.01f, 0.01f, 0.0f };
	FogSettingsBuffer _currFogSettings = { };
	EmissionSettingsBuffer _currEmissionSettings = { };
	DepthOfFieldSettingsBuffer _currDepthOfFieldSettings = { };

	GeneralDataBuffer _generalDataSettings = { };
	DistortionSettingsBuffer _distortionSettings = { };
	int _fogBlurIterations = 2;
	int _emissionBlurIterations = 4;

	CameraBehaviour *_currViewCamera = nullptr;

	LightTile *_lightGrid = nullptr;
	StructuredBufferD3D11 _lightGridBuffer;

	ConstantBufferD3D11 _globalLightBuffer;
	ConstantBufferD3D11 _generalDataBuffer;
	ConstantBufferD3D11 _fogSettingsBuffer;
	ConstantBufferD3D11 _emissionSettingsBuffer;
	ConstantBufferD3D11 _distortionSettingsBuffer;
	ConstantBufferD3D11 _depthOfFieldSettingsBuffer;

	std::vector<float> _fogGaussWeights = { 0.7788081181217f, 0.2165377067336f, 0.0046541751447f };
	std::vector<float> _emissionGaussWeights = { 0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f };
	std::vector<float> _dofGaussWeights = { 0.562f, 0.243f, 0.122f, 0.053f, 0.02f };

	StructuredBufferD3D11 _fogGaussianWeightsBuffer;
	StructuredBufferD3D11 _emissionGaussianWeightsBuffer;
	StructuredBufferD3D11 _dofGaussianWeightsBuffer;

	Ref<SpotLightCollection> _currSpotLightCollection = nullptr;
	Ref<PointLightCollection> _currPointLightCollection = nullptr;

	UINT _skyboxPsID = CONTENT_NULL;
	UINT _environmentCubemapID = CONTENT_NULL;
	UINT _colorLutID = CONTENT_NULL;

	UINT
		_currMeshID			= CONTENT_NULL,
		_currTexID			= CONTENT_NULL,
		_currNormalID		= CONTENT_NULL,
		_currSpecularID		= CONTENT_NULL,
		_currGlossinessID	= CONTENT_NULL,
		_currAmbientID		= CONTENT_NULL,
		_currReflectiveID	= CONTENT_NULL,
		_currOcclusionID	= CONTENT_NULL,
		_currSamplerID		= CONTENT_NULL,
		_currBlendStateID	= CONTENT_NULL,
		_currVsID			= CONTENT_NULL,
		_currPsID			= CONTENT_NULL,
		_currInputLayoutID	= CONTENT_NULL;

#ifdef DEBUG_BUILD
	D3D11_VIEWPORT _viewportOutline = { };
	float _outlineResolutionScale = 0.5f;

	std::vector<Ref<Entity>> _outlinedEntities;

	OutlineSettingsBuffer _outlineSettings = {};
	ConstantBufferD3D11 _outlineSettingsBuffer;

	int _outlineBlurIterations = 2;
	std::vector<float> _outlineGaussWeights = { 0.05f, 0.165f, 0.221f };
	StructuredBufferD3D11 _outlineGaussianWeightsBuffer;

	RenderTargetD3D11 _outlineRT;
	RenderTargetD3D11 _intermediateOutlineRT;

	ImVec4 _overdrawBlendFactor = { 0.2f, 0.05f, 0.4f, 0.0f };
	bool _overdrawIncludeDiscards = true;

	RepeatTracker _mainDrawCallTracker, _mainTriDrawTracker;
	RepeatTracker _overlayDrawCallTracker, _overlayTriDrawTracker;
	RepeatTracker _transparentDrawCallTracker, _transparentTriDrawTracker;
	RepeatTracker _lightDrawCallTracker, _lightTriDrawTracker;
#endif

#ifdef USE_IMGUI
	ComPtr<ID3D11SamplerState> _sceneSampler = nullptr;

	D3D11_RASTERIZER_DESC _shadowRasterizerDesc = { };
	D3D11_BLEND_DESC _transparentBlendDesc = { };
#endif

#ifdef TRACY_ENABLE
#ifdef TRACY_GPU
	tracy::D3D11Ctx *_tracyD3D11Context = nullptr;
#endif

#ifdef TRACY_SCREEN_CAPTURE
	// Resouce with CPU read access for drawing the screen rtv to ina compute shader.
	RenderTargetD3D11 _tracyCaptureRT;
	UINT _tracyCaptureWidth = 0;
	UINT _tracyCaptureHeight = 0;
#endif
#else
	void *_tracyD3D11Context = nullptr;
#endif

	/// Renders all queued entities to the specified target.
	[[nodiscard]] bool RenderToTarget(
		ID3D11RenderTargetView *targetRTV, ID3D11RenderTargetView *targetDepthRTV, 
		ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport);

	[[nodiscard]] bool RenderSpotlights();
	[[nodiscard]] bool RenderPointlights();

	/// Renders all queued opaque entities to the depth buffers of all shadow-casting lights.
	[[nodiscard]] bool RenderShadowCasters();

#ifdef DEBUG_BUILD
	[[nodiscard]] bool RenderOutlinedGeometry();
#endif

	bool RenderScreenEffect(UINT psID);

	[[nodiscard]] bool RenderGeometry(bool overlayStage, bool skipPixelShader = false);

	[[nodiscard]] bool RenderOpaque(ID3D11RenderTargetView *targetSceneRTV, ID3D11RenderTargetView *targetDepthRTV, 
		ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport, bool overlayStage = false);

	[[nodiscard]] bool RenderCustom(ID3D11RenderTargetView *targetRTV, ID3D11RenderTargetView *targetDepthRTV, 
		ID3D11DepthStencilView *targetDSV, const D3D11_VIEWPORT *targetViewport, const std::string &pixelShader, bool overlayStage = false);

	[[nodiscard]] bool RenderTransparency(ID3D11RenderTargetView *targetRTV, ID3D11DepthStencilView *targetDSV,
		const D3D11_VIEWPORT *targetViewport);

	[[nodiscard]] bool RenderPostFX();

	[[nodiscard]] ID3D11DepthStencilState *GetCurrentDepthStencilState();

	[[nodiscard]] bool ResizeWindowBuffers(bool fullscreen, UINT newWidth, UINT newHeight, bool skipResizeD3D11 = false);
	[[nodiscard]] bool ResizeSceneViewBuffers(UINT newWidth, UINT newHeight);
	[[nodiscard]] bool RefreshEmissionBuffers();
	[[nodiscard]] bool RefreshFogBuffers();
	[[nodiscard]] bool RefreshDofBuffers();
#ifdef DEBUG_BUILD
	[[nodiscard]] bool RefreshOutlineBuffers();
#endif

	void SetGaussianWeightsBuffer(StructuredBufferD3D11 *buffer, float *const weights, UINT count);

public:
	~Graphics();

	[[nodiscard]] bool Setup(bool fullscreen, UINT width, UINT height, const Window &window,
		ID3D11Device *&device, ID3D11DeviceContext *&immediateContext, 
		ID3D11DeviceContext **deferredContexts, Content *content);

	void Shutdown();

	[[nodiscard]] bool SetCamera(CameraBehaviour *viewCamera);
	[[nodiscard]] bool SetSpotlightCollection(SpotLightCollection *spotlights);
	[[nodiscard]] bool SetPointlightCollection(PointLightCollection *pointlights);

#ifdef DEBUG_BUILD
	size_t GetMainDrawCallCount() const noexcept		{ return _mainDrawCallTracker.GetCount(); }
	size_t GetMainTriDrawCount() const noexcept			{ return _mainTriDrawTracker.GetCount(); }
	size_t GetOverlayDrawCallCount() const noexcept		{ return _overlayDrawCallTracker.GetCount(); }
	size_t GetOverlayTriDrawCount() const noexcept		{ return _overlayTriDrawTracker.GetCount(); }
	size_t GetTransparentDrawCallCount() const noexcept { return _transparentDrawCallTracker.GetCount(); }
	size_t GetTransparentTriDrawCount() const noexcept	{ return _transparentTriDrawTracker.GetCount(); }
	size_t GetLightDrawCallCount() const noexcept		{ return _lightDrawCallTracker.GetCount(); }
	size_t GetLightTriDrawCount() const noexcept		{ return _lightTriDrawTracker.GetCount(); }

	void AddOutlinedEntity(Entity *entity);
#endif

	void ResetLightGrid();
	void AddLightToTile(UINT tileIndex, UINT lightIndex, LightType type);

	/// Begins a screen fade with the specified duration.
	/// Set to positive to fade to black.
	/// Set to negative to fade back from black.
	void BeginScreenFade(float duration);
	/// Manually sets the screen fade amount to a constant value.
	void SetScreenFadeManual(float amount);
	[[nodiscard]] float GetScreenFadeAmount() const;
	[[nodiscard]] float GetScreenFadeRate() const;

	[[nodiscard]] bool GetRenderTransparent() const;
	[[nodiscard]] bool GetRenderOverlay() const;
	[[nodiscard]] bool GetRenderPostFX() const;

	void SetDistortionOrigin(const dx::XMFLOAT3A &origin);
	void SetDistortionStrength(float strength);

	void SetFogGaussianWeightsBuffer(float *const weights, UINT count);
	void SetEmissionGaussianWeightsBuffer(float *const weights, UINT count);
	void SetDofGaussianWeightsBuffer(float *const weights, UINT count);

	[[nodiscard]] FogSettingsBuffer GetFogSettings() const;
	[[nodiscard]] EmissionSettingsBuffer GetEmissionSettings() const;
	[[nodiscard]] DepthOfFieldSettingsBuffer GetDepthOfFieldSettings() const;
	[[nodiscard]] dx::XMFLOAT3 GetAmbientColor() const;
	[[nodiscard]] UINT GetSkyboxShaderID() const;
	[[nodiscard]] UINT GetEnvironmentCubemapID() const;

	void SetFogSettings(const FogSettingsBuffer &fogSettings);
	void SetEmissionSettings(const EmissionSettingsBuffer &emissionSettings);
	void SetDepthOfFieldSettings(const DepthOfFieldSettingsBuffer& dofSettings);
	void SetAmbientColor(const dx::XMFLOAT3 &color);
	void SetSkyboxShaderID(UINT shaderID);
	void SetEnvironmentCubemapID(UINT cubemapID);


	/// Begins scene rendering, enabling entities to be queued for rendering.
	[[nodiscard]] bool BeginSceneRender();

	/// Renders all queued entities to the window.
	[[nodiscard]] bool EndSceneRender(TimeUtils &time);

#ifdef USE_IMGUI
	std::vector<NotificationMessage> notifications;

	NotificationMessage *GetNotification(const std::string &id)
	{
		for (auto &notification : notifications)
		{
			if (notification.id.compare(id) == 0)
				return &notification;
		}
		return nullptr;
	}

	void SetScenePointFiltering(bool state);

	[[nodiscard]] bool BeginUIRender();
	[[nodiscard]] bool EndUIRender() const;

	[[nodiscard]] bool RenderUI(TimeUtils &time);
	[[nodiscard]] bool RenderSceneView();

	[[nodiscard]] ImGuiID GetBackgroundDockID() const;
#endif

	[[nodiscard]] bool ScreenSpaceRender();

	[[nodiscard]] bool ResetRenderState();

	/// Resets variables and clears all render queues.
	[[nodiscard]] bool EndFrame();

	TESTABLE()
};
