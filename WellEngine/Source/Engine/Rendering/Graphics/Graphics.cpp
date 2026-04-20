#include "stdafx.h"
#include "Graphics.h"
#include "Game/Entity.h"
#include "Game/Behaviours/Rendering/Camera/CameraBehaviour.h"
#include "Game/Behaviours/Rendering/Mesh/MeshBehaviour.h"
#include "Engine/Debug/DebugDrawer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


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

		if (_skyboxBuffer)
		{
			if (!_skyboxBuffer->UpdateBuffer(_context, &_currSkyboxColor))
			{
				ErrMsg("Failed to update skybox buffer!");
				return false;
			}
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

		if (!_depthOfFieldSettingsBuffer.UpdateBuffer(_context, &_currDepthOfFieldSettings))
		{
			ErrMsg("Failed to update depth of field settings buffer")
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

#ifdef DEBUG_BUILD
	{
		_mainDrawCallTracker.EndFrame();
		_mainTriDrawTracker.EndFrame();

		_overlayDrawCallTracker.EndFrame();
		_overlayTriDrawTracker.EndFrame();

		_transparentDrawCallTracker.EndFrame();
		_transparentTriDrawTracker.EndFrame();

		_lightDrawCallTracker.EndFrame();
		_lightTriDrawTracker.EndFrame();
	}
#endif

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

#ifdef DEBUG_BUILD
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

	case RenderType::OVERDRAW:
	{
		if (_overdrawIncludeDiscards)
			_context->OMSetDepthStencilState(_tdss.Get(), 0);
		
		ID3D11BlendState *prevBlendState;
		FLOAT prevBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		UINT prevSampleMask = 0;
		_context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

		static UINT overdrawBlendStateID = _content->GetBlendStateID("Overdraw");
		ID3D11BlendState *const overdrawBlendState = _content->GetBlendState(overdrawBlendStateID);
		if (_currBlendStateID != overdrawBlendStateID)
		{
			_context->OMSetBlendState(overdrawBlendState, &_overdrawBlendFactor.x, 0xffffffff);
			_currBlendStateID = overdrawBlendStateID;
		}

		if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewOverdraw"))
		{
			ErrMsg("Failed to render overdraw view!");
			return false;
		}

		DebugDrawer::Instance().Clear();

		if (_renderOverlay)
			if (!RenderCustom(targetRTV, targetDepthRTV, targetDSV, targetViewport, "PS_DebugViewOverdraw", true))
				return false;

		// Reset blend state
		_context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);

		if (_overdrawIncludeDiscards)
			_context->OMSetDepthStencilState(GetCurrentDepthStencilState(), 0);

		break;
	}
#endif

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

	if (!collection->DoUpdate())
		return true;

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
		lightBehaviour->MarkUpdated();

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
				WarnF("Skipping depth-rendering for non-mesh #{}!", entity_i);
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
				float normalizedDist = (clampedDist - lodDistMin) / (lodDistMax - lodDistMin);

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

#ifdef DEBUG_BUILD
				_lightDrawCallTracker.Step();
				_lightTriDrawTracker.Step(loadedMesh->GetSubMeshIndexCount(lodIndex));
#endif
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

	if (!collection->DoUpdate())
		return true;

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
		lightBehaviour->MarkUpdated();

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
				WarnF("Skipping depth-rendering for non-mesh #{}!", entity_i);
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

#ifdef DEBUG_BUILD
				_lightDrawCallTracker.Step();
				_lightTriDrawTracker.Step(loadedMesh->GetSubMeshIndexCount(lodIndex));
#endif
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
	SetRasterizerState(_shadowRasterizer.Get());

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

	SetRasterizerState(GetRasterizerDefault());

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
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SetRasterizerState(GetRasterizerDefault());

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

			if (!meshBehaviour)
			{
				WarnF("Skipping non-mesh #{} in outline rendering!", entity_i);
				return false;
			}

			ZoneNamedNC(renderMeshZone, "Draw Entity", RandomUniqueColor(), true);
			const std::string &name = meshBehaviour->GetEntity()->GetName();
			ZoneText(name.c_str(), name.size());
			TracyD3D11NamedZoneC(_tracyD3D11Context, renderMeshD3D11Zone, "Draw Entity", RandomUniqueColor(), true);

			// Bind shared geometry resources
			FaceCullingType cullMode = meshBehaviour->GetCullMode();
			SetRasterizerState(_wireframe ?
				GetWireframeRasterizerByCullMode(cullMode) :
				GetRasterizerByCullMode(cullMode)
			);

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

				AssertF(lodIndex < subMeshCount, "The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount);

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

	// If has skybox buffer, bind it to slot 4
	if (_skyboxBuffer)
	{
		ID3D11Buffer *const skyboxBuffer = _skyboxBuffer->GetBuffer();
		_context->PSSetConstantBuffers(4, 1, &skyboxBuffer);
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
			WarnF("Skipping rendering for non-mesh #{}!", entity_i);
			continue;
		}

		ZoneNamedNC(renderMeshZone, "Draw Entity", RandomUniqueColor(), true);
		const std::string &name = meshBehaviour->GetEntity()->GetName();
		ZoneText(name.c_str(), name.size());
		TracyD3D11NamedZoneC(_tracyD3D11Context, renderMeshD3D11Zone, "Draw Entity", RandomUniqueColor(), true);

		// Bind shared geometry resources
		FaceCullingType cullMode = meshBehaviour->GetCullMode();
		SetRasterizerState(_wireframe ?
			GetWireframeRasterizerByCullMode(cullMode) :
			GetRasterizerByCullMode(cullMode)
		);

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

			AssertF(lodIndex < subMeshCount, "The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount);

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

#ifdef DEBUG_BUILD
			if (overlayStage)
			{
				_overlayDrawCallTracker.Step();
				_overlayTriDrawTracker.Step(loadedMesh->GetSubMeshIndexCount(lodIndex));
			}
			else
			{
				_mainDrawCallTracker.Step();
				_mainTriDrawTracker.Step(loadedMesh->GetSubMeshIndexCount(lodIndex));
			}
#endif
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
	_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SetRasterizerState(_wireframe ? GetWireframeRasterizerDefault() : GetRasterizerDefault());

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
	SetRasterizerState(_wireframe ? GetWireframeRasterizerDefault() : GetRasterizerDefault());

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
	SetRasterizerState(_wireframe ? GetWireframeRasterizerDefault() : GetRasterizerDefault());

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
		FaceCullingType cullMode = meshBehaviour->GetCullMode();
		SetRasterizerState(_wireframe ?
			GetWireframeRasterizerByCullMode(cullMode) :
			GetRasterizerByCullMode(cullMode)
		);

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

			AssertF(lodIndex < subMeshCount, "The chosen LOD level ({}) exceeds the meshes LOD count ({})!", lodIndex, subMeshCount);

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

#ifdef DEBUG_BUILD
			_transparentDrawCallTracker.Step();
			_transparentTriDrawTracker.Step(loadedMesh->GetSubMeshIndexCount(lodIndex));
#endif
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

	static ID3D11SamplerState *const clampSS = _content->GetSampler("Clamp")->GetSamplerState();
	_context->CSSetSamplers(0, 1, &clampSS);

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
			_context->Dispatch(
				static_cast<UINT>(std::ceil(_viewportFog.Width / 8.0f)),
				static_cast<UINT>(std::ceil(_viewportFog.Height / 8.0f)),
				1
			);


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
					_context->Dispatch(
						static_cast<UINT>(std::ceil(_viewportFog.Width / 8.0f)),
						static_cast<UINT>(std::ceil(_viewportFog.Height / 8.0f)),
						1
					);


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
					_context->Dispatch(
						static_cast<UINT>(std::ceil(_viewportFog.Width / 8.0f)),
						static_cast<UINT>(std::ceil(_viewportFog.Height / 8.0f)),
						1
					);


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

		// Perform Emission 
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
			ID3D11UnorderedAccessView *const uav[1] = { _blurRT.GetUAV(0) };
			_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

			// Bind shader resource
			ID3D11ShaderResourceView *const srv[1] = { _emissionRT.GetSRV() };
			_context->CSSetShaderResources(0, 1, srv);


			// Send execution command
			_context->Dispatch(
				static_cast<UINT>(std::ceil(_viewportBlur.Width / 8.0f)),
				static_cast<UINT>(std::ceil(_viewportBlur.Height / 8.0f)),
				1
			);


			// Unbind compute shader resources
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(0, 1, nullSRV);

			// Unbind render target
			static ID3D11UnorderedAccessView *const nullUAV = nullptr;
			_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
		}

		// Perform Emission mipmap generation
		if (_renderEmissionFX && _emissionBlurIterations > 0)
		{
			ZoneNamedXNC(emissionMipZone, "Emission Mipmap", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionMipD3D11Zone, "Emission Mipmap", RandomUniqueColor(), true);

			// Bind compute shader
			if (!_content->GetShader("CS_DownsampleHalf")->BindShader(_context))
			{
				ErrMsg("Failed to bind downsample half emission compute shader!");
				return false;
			}

			UINT mipLevels = _blurRT.GetMipLevels();

			for (UINT i = 1; i < mipLevels; i++)
			{
				ZoneNamedXNC(emissionDownsampleIterationZone, "Iteration", RandomUniqueColor(), true);
				TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionDownsampleIterationD3D11Zone, "Downsample Iteration", RandomUniqueColor(), true);

				// Bind render target
				ID3D11UnorderedAccessView *const uav[1] = { _blurRT.GetUAV(i) };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind shader resource
				ID3D11ShaderResourceView *const srv[1] = { _blurRT.GetSRV(i-1) };
				_context->CSSetShaderResources(0, 1, srv);


				// Send execution command
				_context->Dispatch(
					static_cast<UINT>(std::ceil(_viewportBlur.Width / 8.0f)),
					static_cast<UINT>(std::ceil(_viewportBlur.Height / 8.0f)),
					1
				);


				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(nullSRV));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			}
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

			UINT mipLevels = _blurRT.GetMipLevels();
			for (UINT m = 0; m < mipLevels; m++)
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurMipD3D11Zone, "Blur Mip Level", RandomUniqueColor(), true);

				UINT mipWidth = 0, mipHeight = 0;
				_blurRT.GetMipSize(m, &mipWidth, &mipHeight);

				for (int i = 0; i < _emissionBlurIterations; i++)
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionBlurIterationD3D11Zone, "Blur Mip Level Iteration", RandomUniqueColor(), true);

					ID3D11UnorderedAccessView *uavStageOne = _intermediateBlurRT.GetUAV(m);
					ID3D11ShaderResourceView *srvStageOne = _blurRT.GetSRV(m);

					ID3D11UnorderedAccessView *uavStageTwo = _blurRT.GetUAV(m);
					ID3D11ShaderResourceView *srvStageTwo = _intermediateBlurRT.GetSRV(m);

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
						_context->Dispatch(
							static_cast<UINT>(std::ceil((float)mipWidth / 8.0f)),
							static_cast<UINT>(std::ceil((float)mipHeight / 8.0f)),
							1
						);


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
						_context->Dispatch(
							static_cast<UINT>(std::ceil((float)mipWidth / 8.0f)),
							static_cast<UINT>(std::ceil((float)mipHeight / 8.0f)),
							1
						);


						// Unbind compute shader resources
						ID3D11ShaderResourceView *nullSRV[1] = {};
						memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
						_context->CSSetShaderResources(0, 1, nullSRV);

						// Unbind render target
						static ID3D11UnorderedAccessView *const nullUAV = nullptr;
						_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
					}
				}
			}

			// Unbind depth & weight resource
			ID3D11ShaderResourceView *nullSRV[1] = {};
			memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(1, 1, nullSRV);
			_context->CSSetShaderResources(3, 1, nullSRV);
		}

		// Merge Emission mips into mip 0
		if (_renderEmissionFX && _emissionBlurIterations > 0)
		{
			ZoneNamedXNC(emissionMergeZone, "Emission Mip Merge", RandomUniqueColor(), true);
			TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionMergeD3D11Zone, "Emission Mip Merge", RandomUniqueColor(), true);

			if (!_content->GetShader("CS_MergeEmissionMips")->BindShader(_context))
			{
				ErrMsg("Failed to bind emission mip merge compute shader!");
				return false;
			}

			UINT mipLevels = _blurRT.GetMipLevels();
			if (mipLevels > 1)
			{
				for (UINT inMipLevel = mipLevels - 1; inMipLevel > 0; --inMipLevel)
				{
					TracyD3D11NamedZoneXC(_tracyD3D11Context, emissionMergeIterationD3D11Zone, "Merge Mip Level", RandomUniqueColor(), true);

					UINT outMipLevel = inMipLevel - 1;

					UINT outMipWidth = 0, outMipHeight = 0;
					_blurRT.GetMipSize(outMipLevel, &outMipWidth, &outMipHeight);

					ID3D11UnorderedAccessView *const uav[1] = { _blurRT.GetUAV(outMipLevel) };
					_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

					ID3D11ShaderResourceView *const srv[1] = { _blurRT.GetSRV(inMipLevel) };
					_context->CSSetShaderResources(0, 1, srv);

					_context->Dispatch(
						static_cast<UINT>(std::ceil((float)outMipWidth / 8.0f)),
						static_cast<UINT>(std::ceil((float)outMipHeight / 8.0f)),
						1
					);

					ID3D11ShaderResourceView *nullSRV[1] = {};
					memset(nullSRV, 0, sizeof(nullSRV));
					_context->CSSetShaderResources(0, 1, nullSRV);

					ID3D11UnorderedAccessView *const nullUAV = nullptr;
					_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
				}
			}
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
					_context->Dispatch(
						static_cast<UINT>(std::ceil(_viewportOutline.Width / 8.0f)), 
						static_cast<UINT>(std::ceil(_viewportOutline.Height / 8.0f)), 
						1
					);


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
					_context->Dispatch(
						static_cast<UINT>(std::ceil(_viewportOutline.Width / 8.0f)), 
						static_cast<UINT>(std::ceil(_viewportOutline.Height / 8.0f)), 
						1
					);


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
		ID3D11UnorderedAccessView *outputUAV = _uav.Get();
#ifdef USE_IMGUI
		outputUAV = _intermediateRT.GetUAV();
#endif

		ID3D11UnorderedAccessView *const uav[1] = { 
			outputUAV
		};
		_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);


		// Bind screen, emission & fog resources
		ID3D11ShaderResourceView *srvs[3] = {
			_sceneRT.GetSRV(),
			_blurRT.GetSRV(0),
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
		_context->Dispatch(
			static_cast<UINT>(std::ceil(_viewportSceneView.Width / 8.0f)), 
			static_cast<UINT>(std::ceil(_viewportSceneView.Height / 8.0f)), 
			1
		);


		// Apply color grading LUT if one is set
		if (_colorLutID != CONTENT_NULL)
		{
			std::string lutShaderName = "CS_ColorLUT";
			if (!_content->GetShader(lutShaderName)->BindShader(_context))
			{
				ErrMsg("Failed to bind color LUT compute shader!");
				return false;
			}

			ID3D11ShaderResourceView *lutSRV = _content->GetTexture(_colorLutID)->GetSRV();
			_context->CSSetShaderResources(0, 1, &lutSRV);

			// Send execution command
			_context->Dispatch(
				static_cast<UINT>(std::ceil(_viewportSceneView.Width / 8.0f)),
				static_cast<UINT>(std::ceil(_viewportSceneView.Height / 8.0f)),
				1
			);
		}


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

#ifdef USE_IMGUI // TODO: DoF disabled without ImGui for now
	if (_renderDepthOfFieldFX)
	{
		ZoneNamedXNC(depthOfFieldZone, "Depth of Field", RandomUniqueColor(), true);
		TracyD3D11NamedZoneC(_tracyD3D11Context, depthOfFieldD3D11Zone, "Depth of Field", RandomUniqueColor(), true);

		// Calculate circle of confusion
		{
			ZoneNamedXNC(cocZone, "Circle Of Confusion", RandomUniqueColor(), true);
			TracyD3D11NamedZoneC(_tracyD3D11Context, cocD3D11Zone, "Depth of Field", RandomUniqueColor(), true);

			// Bind compute shader
			if (!_content->GetShader("CS_CircleOfConfusionFX")->BindShader(_context))
			{
				ErrMsg("Failed to bind downscale emission compute shader!");
				return false;
			}

			// Bind render target
			ID3D11UnorderedAccessView *uav[2] = {
				_cocRT.GetUAV(),
				_dofSharpRT.GetUAV()
			};
			_context->CSSetUnorderedAccessViews(0, 2, uav, nullptr);

			// Bind depth & sharp resource
			ID3D11ShaderResourceView *const srv[2] = {
#ifdef USE_IMGUI
				_intermediateRT.GetSRV(),
#else
				nullptr, // TODO: Make sure this works without IMGUI
#endif
				_depthRT.GetSRV()
			};
			_context->CSSetShaderResources(0, 2, srv);

			ID3D11Buffer *const dofSettings = _depthOfFieldSettingsBuffer.GetBuffer();
			_context->CSSetConstantBuffers(6, 1, &dofSettings);

			// Send execution command
			_context->Dispatch(
				static_cast<UINT>(std::ceil(_viewportSceneView.Width / 8.0f)), 
				static_cast<UINT>(std::ceil(_viewportSceneView.Height / 8.0f)), 
				1
			);


			// Unbind compute shader resources
			ID3D11ShaderResourceView *nullSRV[2] = {};
			memset(nullSRV, 0, 2 * sizeof(ID3D11ShaderResourceView));
			_context->CSSetShaderResources(0, 2, nullSRV);

			// Unbind render target
			static ID3D11UnorderedAccessView *const nullUAV[2] = { nullptr, nullptr };
			_context->CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);
		}

		// Pre-calculate Blur
		{
			ZoneNamedXNC(blurZone, "Blur", RandomUniqueColor(), true);
			TracyD3D11NamedZoneC(_tracyD3D11Context, blurD3D11Zone, "Blur", RandomUniqueColor(), true);

			// Downsample
			{
				// Bind compute shader
				if (!_content->GetShader("CS_DownsampleCheap")->BindShader(_context))
				{
					ErrMsg("Failed to bind downscale emission compute shader!");
					return false;
				}

				// Bind render target
				ID3D11UnorderedAccessView *const uav[1] = { _dofHalfBlur1RT.GetUAV() };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind shader resource
				ID3D11ShaderResourceView *const srv[1] = { _dofSharpRT.GetSRV() };
				_context->CSSetShaderResources(0, 1, srv);


				// Send execution command
				_context->Dispatch(
					static_cast<UINT>(std::ceil(_viewportDof.Width / 8.0f)),
					static_cast<UINT>(std::ceil(_viewportDof.Height / 8.0f)),
					1
				);


				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			}

			ID3D11ShaderResourceView *const srvGaussianWeights[1] = { _dofGaussianWeightsBuffer.GetSRV() };
			_context->CSSetShaderResources(3, 1, srvGaussianWeights);

			// Horizontal Blur
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurIterationXD3D11Zone, "Horizontal", RandomUniqueColor(), true);

				// Bind compute shader
				if (!_content->GetShader("CS_BlurHorizontalDof")->BindShader(_context))
				{
					ErrMsg("Failed to bind horizontal blur compute shader!");
					return false;
				}

				// Bind render target
				ID3D11UnorderedAccessView *const uav[1] = { _dofHalfBlur2RT.GetUAV() };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind shader resource
				ID3D11ShaderResourceView *const srv[1] = { _dofHalfBlur1RT.GetSRV() };
				_context->CSSetShaderResources(0, 1, srv);


				// Send execution command
				_context->Dispatch(
					static_cast<UINT>(std::ceil(_viewportDof.Width / 8.0f)),
					static_cast<UINT>(std::ceil(_viewportDof.Height / 8.0f)),
					1
				);


				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			}

			// Vertical Blur
			{
				TracyD3D11NamedZoneXC(_tracyD3D11Context, outlineBlurIterationXD3D11Zone, "Vertical", RandomUniqueColor(), true);

				// Bind compute shader
				if (!_content->GetShader("CS_BlurVerticalDof")->BindShader(_context))
				{
					ErrMsg("Failed to bind horizontal blur compute shader!");
					return false;
				}

				// Bind render target
				ID3D11UnorderedAccessView *const uav[1] = { _dofHalfBlur1RT.GetUAV() };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind shader resource
				ID3D11ShaderResourceView *const srv[1] = { _dofHalfBlur2RT.GetSRV() };
				_context->CSSetShaderResources(0, 1, srv);


				// Send execution command
				_context->Dispatch(
					static_cast<UINT>(std::ceil(_viewportDof.Width / 8.0f)), 
					static_cast<UINT>(std::ceil(_viewportDof.Height / 8.0f)), 
					1
				);


				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
			}

			ID3D11ShaderResourceView *const nullSRV[1] = { nullptr };
			_context->CSSetShaderResources(3, 1, nullSRV);

			// Upsample
			{
				// Bind compute shader
				if (!_content->GetShader("CS_Upsample")->BindShader(_context))
				{
					ErrMsg("Failed to bind downscale emission compute shader!");
					return false;
				}

				// Bind render target
				ID3D11UnorderedAccessView *const uav[1] = { _dofFullBlurRT.GetUAV() };
				_context->CSSetUnorderedAccessViews(0, 1, uav, nullptr);

				// Bind shader resource
				ID3D11ShaderResourceView *const srv[1] = { _dofHalfBlur1RT.GetSRV() };
				_context->CSSetShaderResources(0, 1, srv);

				// Send execution command
				_context->Dispatch(
					static_cast<UINT>(std::ceil(_viewportSceneView.Width / 8.0f)), 
					static_cast<UINT>(std::ceil(_viewportSceneView.Height / 8.0f)), 
					1
				);

				// Unbind compute shader resources
				ID3D11ShaderResourceView *nullSRV[1] = {};
				memset(nullSRV, 0, sizeof(ID3D11ShaderResourceView));
				_context->CSSetShaderResources(0, 1, nullSRV);

				// Unbind render target
				static ID3D11UnorderedAccessView *const nullUAV = nullptr;
				_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

			}
		}

		// Combine depth of field
		{
			// Bind combine compute shader
			if (!_content->GetShader("CS_CombineDepthOfFieldFX")->BindShader(_context))
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
				_dofSharpRT.GetSRV(),
				_cocRT.GetSRV(),
				_dofFullBlurRT.GetSRV(),
			};
			_context->CSSetShaderResources(0, 3, srvs);

			// Send execution command
			_context->Dispatch(
				static_cast<UINT>(std::ceil(_viewportSceneView.Width / 8.0f)), 
				static_cast<UINT>(std::ceil(_viewportSceneView.Height / 8.0f)), 
				1
			);


			// Unbind shader resources
			memset(srvs, 0, sizeof(srvs));
			_context->CSSetShaderResources(0, 3, srvs);

			// Unbind render target
			static ID3D11UnorderedAccessView *const nullUAV = nullptr;
			_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

		}
	}
#endif

	return true;
}


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

	_currRasterizer		= nullptr;
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
