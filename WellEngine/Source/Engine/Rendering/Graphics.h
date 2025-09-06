#pragma once

#include <d3d11.h>
#include <array>
#include <wrl/client.h>
#include "Content/Content.h"
#include "Timing/TimeUtils.h"
#include "D3D/RenderTargetD3D11.h"
#include "Window/Window.h"
#include "Rendering/Lighting/SpotLightCollection.h"
#include "Rendering/Lighting/PointLightCollection.h"
#include "Behaviours/CameraBehaviour.h"
#include "Debug/DebugDrawer.h"

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
	float thickness = 0.2f;
	float stepSize = 0.0f;
	int minSteps = 0;
	int maxSteps = 64;
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

struct OutlineSettingsBuffer
{
	dx::XMFLOAT4 color = { 0.2f, 0.7f, 1.0f, 1.0f };
	float strength = 3.0f;
	float exponent = 1.0f;
	float smoothing = 0.6f;

	float _padding[1];
};

/// Handles rendering of the scene and the GUI.
class Graphics
{
private:
	bool _isSetup		= false;
	bool _isRendering	= false;

	ID3D11Device *_device			= nullptr;
	ID3D11DeviceContext	*_context	= nullptr;
#ifdef DEFERRED_CONTEXTS
	ID3D11DeviceContext **_deferredContexts = nullptr;
#endif
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
	D3D11_VIEWPORT _viewportEmission = { };
	D3D11_VIEWPORT _viewportBlur = { };
	D3D11_VIEWPORT _viewportFog = { };

	ComPtr<ID3D11RasterizerState> _defaultRasterizer = nullptr;
	ComPtr<ID3D11RasterizerState> _wireframeRasterizer = nullptr;
	ComPtr<ID3D11RasterizerState> _shadowRasterizer = nullptr;

	bool _renderTransparency = true;
	bool _renderOverlay = true;
	bool _renderDebugDraw = true;
	bool _renderPostFX = true;
	bool _renderFogFX = true;
	bool _renderEmissionFX = true;
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

	RenderType _renderOutput = RenderType::DEFAULT;

	dx::XMFLOAT4A _currAmbientColor = { 0.01f, 0.01f, 0.01f, 0.0f };
	FogSettingsBuffer _currFogSettings = { };
	EmissionSettingsBuffer _currEmissionSettings = { };

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

	std::vector<float> _fogGaussWeights = { 0.7788081181217f, 0.2165377067336f, 0.0046541751447f };
	std::vector<float> _emissionGaussWeights = { 0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f };

	StructuredBufferD3D11 _fogGaussianWeightsBuffer;
	StructuredBufferD3D11 _emissionGaussianWeightsBuffer;

	Ref<SpotLightCollection> _currSpotLightCollection = nullptr;
	Ref<PointLightCollection> _currPointLightCollection = nullptr;

	UINT _skyboxPsID = CONTENT_NULL;
	UINT _environmentCubemapID = CONTENT_NULL;

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

	std::vector<Ref<Entity>> _outlinedEntities;

	OutlineSettingsBuffer _outlineSettings = {};
	ConstantBufferD3D11 _outlineSettingsBuffer;

	int _outlineBlurIterations = 3;
	std::vector<float> _outlineGaussWeights = { 0.05f, 0.165f, 0.221f };
	StructuredBufferD3D11 _outlineGaussianWeightsBuffer;

	RenderTargetD3D11 _outlineRT;
	RenderTargetD3D11 _intermediateOutlineRT;
#endif

#ifdef USE_IMGUI
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

	[[nodiscard]] FogSettingsBuffer GetFogSettings() const;
	[[nodiscard]] EmissionSettingsBuffer GetEmissionSettings() const;
	[[nodiscard]] dx::XMFLOAT3 GetAmbientColor() const;
	[[nodiscard]] UINT GetSkyboxShaderID() const;
	[[nodiscard]] UINT GetEnvironmentCubemapID() const;

	void SetFogSettings(const FogSettingsBuffer &fogSettings);
	void SetEmissionSettings(const EmissionSettingsBuffer &emissionSettings);
	void SetAmbientColor(const dx::XMFLOAT3 &color);
	void SetSkyboxShaderID(UINT shaderID);
	void SetEnvironmentCubemapID(UINT cubemapID);


	/// Begins scene rendering, enabling entities to be queued for rendering.
	[[nodiscard]] bool BeginSceneRender();

	/// Renders all queued entities to the window.
	[[nodiscard]] bool EndSceneRender(TimeUtils &time);

#ifdef USE_IMGUI
	struct NotificationMessage
	{
		std::string_view message;
		int severity = 0;
		float duration = -1.0f;

		NotificationMessage(std::string_view message, int severity = 0, float duration = -1.0f) 
			: message(message), severity(severity), duration(duration) {}
	};
	std::vector<NotificationMessage> notifications;

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
