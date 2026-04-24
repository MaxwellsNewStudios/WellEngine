#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "Scene.h"

#include "Engine/Audio/SoundEngine.h"
#include "Game/Game.h"
#include <Game/Behaviours/Rendering/Mesh/B_Mesh.h>
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Game/Behaviours/Sound/B_SoundListener.h"
#ifdef DEBUG_BUILD
#include "Game/Behaviours/Debug/B_DebugManager.h"
#endif

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion


#pragma region Initialization & Destruction
Scene::Scene(std::string name, bool transitional) 
	: _sceneName(std::move(name)), _transitionScene(transitional)
{
	_spotlights = std::make_unique<LightSpotCollection>();
	_pointlights = std::make_unique<LightPointCollection>();
}
Scene::~Scene()
{
	_isDestroyed = true;
}

bool Scene::InitCommon()
{
	// Set up physics instance
	if (!_physInstance.Initialize(_game->GetJoltManager()))
	{
		ErrMsg("Failed to initialize physics instance!");
		return false;
	}

	return true;
}
bool Scene::InitializeNull(ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics)
{
	ZoneScopedC(RandomUniqueColor());

	if (_initialized)
		return false;
	_isDestroyed = false;

	_game = game;
	_device = device;
	_context = context;
	_content = content;
	_graphics = graphics;

	if (!InitCommon())
	{
		ErrMsg("Failed to initialize common scene data!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(100.0f, 100.0f, 100.0f));
	if (!_sceneHolder.Initialize(sceneBounds))
	{
		ErrMsg("Failed to initialize scene holder!");
		return false;
	}

	if (!_pointlights->Initialize(device, 1))
	{
		ErrMsg("Failed to initialize pointlight collection!");
		return false;
	}

	if (!_spotlights->Initialize(device, 1))
	{
		ErrMsg("Failed to initialize spotlight collection!");
		return false;
	}

	_physInstance.GetSystem().OptimizeBroadPhase();

	_initialized = true;
	return true;
}
bool Scene::InitializeBase(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume)
{
	ZoneScopedC(RandomUniqueColor());

	if (_initialized)
		return false;
	_isDestroyed = false;

	_game = game;
	_device = device;
	_context = context;
	_content = content;
	_graphics = graphics;
	_sceneName = std::move(sceneName);

	if (!InitCommon())
	{
		ErrMsg("Failed to initialize common scene data!");
		return false;
	}

	dx::AUDIO_ENGINE_FLAGS audioFlags = dx::AudioEngine_Default;
	audioFlags |= dx::AudioEngine_EnvironmentalReverb;
	audioFlags |= dx::AudioEngine_ReverbUseFilters;
	//audioFlags |= dx::AudioEngine_UseMasteringLimiter;
	//audioFlags |= dx::AudioEngine_DisableLFERedirect;
	//audioFlags |= dx::AudioEngine_DisableDopplerEffect;
	//audioFlags |= dx::AudioEngine_ZeroCenter3D;

	if (!_soundEngine.Initialize(audioFlags, dx::Reverb_Cave, gameVolume))
	{
		ErrMsg("Failed to initialize sound engine!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(500.0f, 500.0f, 500.0f));
	if (!_sceneHolder.Initialize(sceneBounds))
	{
		ErrMsg("Failed to initialize scene holder!");
		return false;
	}

	if (!_pointlights->Initialize(device, 256))
	{
		ErrMsg("Failed to initialize pointlight collection!");
		return false;
	}

	if (!_spotlights->Initialize(device, 512))
	{
		ErrMsg("Failed to initialize spotlight collection!");
		return false;
	}

	// Set visual effect parameters
	{
		_ambientColor = { 0.1f, 0.1f, 0.1f };

		_fogSettings.thickness = 0.15f;
		_fogSettings.sampleBias = 1.5f;
		_fogSettings.maxSteps = 64;
		_fogSettings.depthFadeBegin = 0.5f;
		_fogSettings.depthFadeEnd = 1.0f;
		_fogSettings.depthFadeExp = 1.0f;

		_emissionSettings.strength = 1.0f;
		_emissionSettings.exponent = 0.5f;
		_emissionSettings.threshold = 1.0f;

		_depthOfFieldSettings.focalPlane = 0.0f;
		_depthOfFieldSettings.aperture = 0.0f;
		_depthOfFieldSettings.imageDistance = 0.0f;
	}

#ifdef DEBUG_BUILD
	// Create debug manager
	{
		Entity *manager = nullptr;
		if (!CreateEntity(&manager, "DebugManager", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}
		manager->SetSerialization(false);

		B_DebugManager *behaviour = new B_DebugManager();
		if (!behaviour->Initialize(manager))
		{
			ErrMsg("Failed to initialize debug manager behaviour!");
			return false;
		}

		_debugManager = behaviour;
	}
#endif

	// Deserialize scene
	if (!Deserialize())
	{
		Warn("Could not deserialize scene!");
		return false;
	}

	_physInstance.GetSystem().OptimizeBroadPhase();

	_initialized = true;
	return true;
}

void Scene::EnterScene()
{
	_graphics->SetAmbientColor(_ambientColor);
	_graphics->SetSkyboxColor(_skyboxColor);

	_graphics->SetFogBlurIterations(_fogBlurIterations);
	if (!_fogGaussWeights.empty())
		_graphics->SetFogGaussianWeights(_fogGaussWeights.data(), (UINT)_fogGaussWeights.size());
	_graphics->SetFogSettings(_fogSettings);

	_graphics->SetEmissionBlurIterations(_emissionBlurIterations);
	if (!_emissionGaussWeights.empty())
		_graphics->SetEmissionGaussianWeights(_emissionGaussWeights.data(), (UINT)_emissionGaussWeights.size());
	_graphics->SetEmissionSettings(_emissionSettings);

	_graphics->SetDepthOfFieldSettings(_depthOfFieldSettings);

	_graphics->SetEnvironmentCubemapID(_envCubemapID);
	_graphics->SetSkyboxShaderID(_skyboxShaderID);

#ifdef DEBUG_BUILD
	UpdateBillboardGizmos();
#endif

	if (!_unentered)
		ResumeSceneSound();

	_unentered = false;
}
void Scene::ExitScene()
{
	_ambientColor = _graphics->GetAmbientColor();
	_skyboxColor = _graphics->GetSkyboxColor();

	_fogBlurIterations = _graphics->GetFogBlurIterations();
	_fogGaussWeights = _graphics->GetFogWeights();
	_fogSettings = _graphics->GetFogSettings();

	_emissionBlurIterations = _graphics->GetEmissionBlurIterations();
	_emissionGaussWeights = _graphics->GetEmissionWeights();
	_emissionSettings = _graphics->GetEmissionSettings();

	_depthOfFieldSettings = _graphics->GetDepthOfFieldSettings();

	_envCubemapID = _graphics->GetEnvironmentCubemapID();
	_skyboxShaderID = _graphics->GetSkyboxShaderID();

	SuspendSceneSound();
}
void Scene::ResetScene()
{
	_initialized = false;
	_game = nullptr;
	_device = nullptr;
	_context = nullptr;
	_content = nullptr;
	_graphics = nullptr;
	_sceneHolder.ResetSceneHolder();
#ifdef DEBUG_BUILD
	_debugManager = nullptr;
#endif
	_input = nullptr;

	_mainCamera = nullptr;
	_mainListener = nullptr;

	_soundEngine.ResetSoundEngine();

#ifdef DEBUG_BUILD
	_isGeneratingEntityBounds = false;
	_isGeneratingVolumeTree = false;
	_isGeneratingCameraCulling = false;
	_rayCastFromMouse = false;
	_cameraCubeSide = 0;
#endif

#ifdef USE_IMGUI
	_collapsedEntities = {};
#endif

	_sceneName.clear();

	_spotlights = std::make_unique<LightSpotCollection>();
	_pointlights = std::make_unique<LightPointCollection>();
}
#pragma endregion


#pragma region Update
void Scene::AddUpdateCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_updateCallbacks.begin(), _updateCallbacks.end(), beh) != _updateCallbacks.end())
		return;

	_updateCallbacks.emplace_back(beh);
}
void Scene::RemoveUpdateCallback(Behaviour *beh)
{
	// Remove any callbacks from the same behaviour
	auto it = std::remove(_updateCallbacks.begin(), _updateCallbacks.end(), beh);
	if (it != _updateCallbacks.end())
		_updateCallbacks.erase(it);
}

void Scene::AddParallelUpdateCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_parallelUpdateCallbacks.begin(), _parallelUpdateCallbacks.end(), beh) != _parallelUpdateCallbacks.end())
		return;

	_parallelUpdateCallbacks.emplace_back(beh);
}
void Scene::RemoveParallelUpdateCallback(Behaviour *beh)
{
	// Remove any callbacks from the same behaviour
	auto it = std::remove(_parallelUpdateCallbacks.begin(), _parallelUpdateCallbacks.end(), beh);
	if (it != _parallelUpdateCallbacks.end())
		_parallelUpdateCallbacks.erase(it);
}

void Scene::AddLateUpdateCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_lateUpdateCallbacks.begin(), _lateUpdateCallbacks.end(), beh) != _lateUpdateCallbacks.end())
		return;

	_lateUpdateCallbacks.emplace_back(beh);
}
void Scene::RemoveLateUpdateCallback(Behaviour *beh)
{
	// Remove any callbacks from the same behaviour
	auto it = std::remove(_lateUpdateCallbacks.begin(), _lateUpdateCallbacks.end(), beh);
	if (it != _lateUpdateCallbacks.end())
		_lateUpdateCallbacks.erase(it);
}

void Scene::AddFixedUpdateCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_fixedUpdateCallbacks.begin(), _fixedUpdateCallbacks.end(), beh) != _fixedUpdateCallbacks.end())
		return;

	_fixedUpdateCallbacks.emplace_back(beh);
}
void Scene::RemoveFixedUpdateCallback(Behaviour *beh)
{
	// Remove any callbacks from the same behaviour
	auto it = std::remove(_fixedUpdateCallbacks.begin(), _fixedUpdateCallbacks.end(), beh);
	if (it != _fixedUpdateCallbacks.end())
		_fixedUpdateCallbacks.erase(it);
}

void Scene::AddPhysicsUpdateCallback(Behaviour *beh)
{
	// Check if the callback already exists
	if (std::find(_physicsUpdateCallbacks.begin(), _physicsUpdateCallbacks.end(), beh) != _physicsUpdateCallbacks.end())
		return;

	_physicsUpdateCallbacks.emplace_back(beh);
}
void Scene::RemovePhysicsUpdateCallback(Behaviour *beh)
{
	// Remove any callbacks from the same behaviour
	auto it = std::remove(_physicsUpdateCallbacks.begin(), _physicsUpdateCallbacks.end(), beh);
	if (it != _physicsUpdateCallbacks.end())
		_physicsUpdateCallbacks.erase(it);
}


bool Scene::Update(TimeUtils &time, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	if (BindingCollection::IsTriggered(InputBindings::InputAction::Save))
	{
		bool isSave = true;
#ifdef EDIT_MODE
		isSave = false;
#endif

		if (!Serialize(isSave))
		{
			ErrMsg("Failed to serialize scene!");
			return false;
		}
	}

	_input = &input;

	// Update entities
	for (UINT i = 0; i < _updateCallbacks.size(); i++)
	{
		if (!_updateCallbacks[i]->InitialUpdate(time, input))
		{
			ErrMsgF("Failed to update entity '{}'!", _updateCallbacks[i]->GetEntity()->GetName());
			return false;
		}
	}

#ifdef PARALLEL_UPDATE
	bool parallelFailed = false;
	int parallelBallbackCount = static_cast<int>(_parallelUpdateCallbacks.size());

#pragma omp parallel for schedule(dynamic) num_threads(PARALLEL_THREADS)
	for (int i = 0; i < parallelBallbackCount; i++)
	{
		if (!parallelFailed)
		{
			if (!_parallelUpdateCallbacks[i]->InitialParallelUpdate(time, input))
			{
#pragma omp critical
				{
					ErrMsgF("Failed to update entity '{}' in parallel!", _parallelUpdateCallbacks[i]->GetEntity()->GetName());
					parallelFailed = true;
				}
			}
		}
	}

	if (parallelFailed)
	{
		ErrMsg("Parallel update failed!");
		return false;
	}
#endif

	if (!UpdateSound())
	{
		ErrMsg("Failed to update sound!");
		return false;
	}

	if (!_graphics->SetSpotlightCollection(_spotlights.get()))
	{
		ErrMsg("Failed to set spotlight collection!");
		return false;
	}

	if (!_graphics->SetLightPointCollection(_pointlights.get()))
	{
		ErrMsg("Failed to set pointlight collection!");
		return false;
	}

	if (_mainCamera)
	{
		if (!_mainCamera.Get()->UpdateBuffers())
		{
			ErrMsg("Failed to update view camera's buffers!");
			return false;
		}
	}

	return true;
}
bool Scene::LateUpdate(TimeUtils &time, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	// Late update entities
	for (UINT i = 0; i < _lateUpdateCallbacks.size(); i++)
	{
		if (!_lateUpdateCallbacks[i]->InitialLateUpdate(time, input))
		{
			ErrMsgF("Failed to late update entity '{}'!", _lateUpdateCallbacks[i]->GetEntity()->GetName());
			return false;
		}
	}

	// Update volume tree & insert new entities
	if (!_sceneHolder.Update())
	{
		ErrMsg("Failed to update scene holder!");
		return false;
	}

	// Update light collections
	if (!_spotlights->UpdateBuffers(_device, _context))
	{
		ErrMsg("Failed to update spotlight buffers!");
		return false;
	}

	if (!_pointlights->UpdateBuffers(_device, _context))
	{
		ErrMsg("Failed to update pointlight buffers!");
		return false;
	}

	return true;
}
bool Scene::FixedUpdate(float deltaTime, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	for (UINT i = 0; i < _fixedUpdateCallbacks.size(); i++)
	{
		if (!_fixedUpdateCallbacks[i]->InitialFixedUpdate(deltaTime, input))
		{
			ErrMsgF("Failed to fixed update entity '{}'!", _fixedUpdateCallbacks[i]->GetEntity()->GetName());
			return false;
		}
	}

	return true;
}
bool Scene::PhysUpdate(float deltaTime)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	if (_physInstance.GetPaused())
		return true;

	for (UINT i = 0; i < _physicsUpdateCallbacks.size(); i++)
	{
		if (!_physicsUpdateCallbacks[i]->InitialPhysicsUpdate(deltaTime))
		{
			ErrMsgF("Failed to physics update entity '{}'!", _physicsUpdateCallbacks[i]->GetEntity()->GetName());
			return false;
		}
	}

	if (!_physInstance.Update(deltaTime))
		return false;

	return true;
}

bool Scene::UpdateCullingTree()
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	const UINT entityCount = _sceneHolder.GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		Entity *ent = _sceneHolder.GetEntity(i);

		if (!ent)
			continue;

		if (ent->IsRemoved())
			continue;

		if (!ent->GetTransform()->IsScenePosDirty())
			continue;

		if (!_sceneHolder.UpdateEntityPosition(ent))
		{
			ErrMsg("Failed to update entity transform!");
			return false;
		}
	}

	_sceneHolder.RecalculateTreeCullingBounds();

	return true;
}
bool Scene::UpdateSound()
{
	ZoneScopedXC(RandomUniqueColor());

	if (!_soundEngine.Update())
	{
		ErrMsg("Failed to update sound engine!");
		return false;
	}

	return true;
}

#ifdef DEBUG_BUILD
void Scene::UpdateBillboardGizmos()
{
	if (!_debugManager)
		return;

	_debugManager.Get()->UpdateGizmoBillboards();
}
#endif
#pragma endregion


#pragma region Render
bool Scene::Render(TimeUtils &time, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	if (!_graphics->SetCamera(_mainCamera.Get()))
	{
		ErrMsg("Failed to set camera!");
		return false;
	}

	DebugDrawer::Instance().SetCamera(_mainCamera.Get());

	std::vector<Entity *> entitiesToRender;
	entitiesToRender.reserve(_mainCamera.Get()->GetCullCount());

	union {
		dx::BoundingFrustum frustum = {};
		dx::BoundingOrientedBox box;
	} view;
	bool isCameraOrtho = _mainCamera.Get()->GetOrtho();

	time.TakeSnapshot("FrustumCull");
	if (isCameraOrtho)
	{
		if (!_mainCamera.Get()->StoreBounds(view.box, false))
		{
			ErrMsg("Failed to store camera box!");
			return false;
		}

		if (!_sceneHolder.BoxCull(view.box, entitiesToRender))
		{
			ErrMsg("Failed to perform box culling!");
			return false;
		}
	}
	else
	{
		if (!_mainCamera.Get()->StoreBounds(view.frustum, false))

		{
			ErrMsg("Failed to store camera frustum!");
			return false;
		}

		if (!_sceneHolder.FrustumCull(view.frustum, entitiesToRender))
		{
			ErrMsg("Failed to perform frustum culling!");
			return false;
		}
	}

	for (UINT i = 0; i < entitiesToRender.size(); i++)
	{
		Entity *ent = entitiesToRender[i];

		if (!ent->InitialRender(*(_mainCamera.Get()), _mainCamera.Get()->GetRendererInfo()))
		{
			ErrMsg("Failed to render entity!");
			return false;
		}
	}

	_mainCamera.Get()->SortGeometryQueue();
	if (_graphics->GetRenderTransparent())
		_mainCamera.Get()->SortTransparentQueue();
	if (_graphics->GetRenderOverlay())
		_mainCamera.Get()->SortOverlayQueue();

	time.TakeSnapshot("FrustumCull");

	const int spotlightCount = static_cast<int>(_spotlights->GetNrOfLights());
	const int pointlightCount = static_cast<int>(_pointlights->GetNrOfLights());

#pragma warning(disable: 6993)
#pragma omp parallel num_threads(PARALLEL_THREADS)
	{
#pragma omp for schedule(dynamic) nowait
		for (int i = 0; i < spotlightCount; i++)
		{
			ZoneNamedXNC(cullSpotlightZone, "Cull Spotlight", RandomUniqueColor(), true);

			if (!_spotlights.get()->GetLightBehaviour(i)->DoUpdate())
				continue;

			B_Camera *spotlightCamera = _spotlights.get()->GetLightBehaviour(i)->GetShadowCamera();

			std::vector<Entity *> entitiesToCastShadows;
			entitiesToCastShadows.reserve(spotlightCamera->GetCullCount());

			bool isSpotlightOrtho = spotlightCamera->GetOrtho();

			bool intersectResult = false;
			if (isSpotlightOrtho)
			{
				dx::BoundingOrientedBox lightBounds;
				if (!spotlightCamera->StoreBounds(lightBounds, false))
				{
					ErrMsg("Failed to store spotlight camera oriented box!");
					continue;
				}

				if (isCameraOrtho)	intersectResult = intersectResult || view.box.Intersects(lightBounds);
				else				intersectResult = intersectResult || view.frustum.Intersects(lightBounds);

				if (!intersectResult)
				{ // Skip rendering if the bounds don't intersect
					_spotlights->SetLightEnabled(i, false);
					continue;
				}

				if (!_sceneHolder.BoxCull(lightBounds, entitiesToCastShadows))
				{
					ErrMsgF("Failed to perform box culling for spotlight #{}!", i);
					continue;
				}
			}
			else
			{
				dx::BoundingFrustum lightBounds;
				if (!spotlightCamera->StoreBounds(lightBounds, false))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					continue;
				}

				if (isCameraOrtho)	intersectResult = intersectResult || view.box.Intersects(lightBounds);
				else				intersectResult = intersectResult || view.frustum.Intersects(lightBounds);

				if (!intersectResult)
				{ // Skip rendering if the bounds don't intersect
					_spotlights->SetLightEnabled(i, false);
					continue;
				}
				_spotlights->SetLightEnabled(i, true);

				if (!_sceneHolder.FrustumCull(lightBounds, entitiesToCastShadows))
				{
					ErrMsgF("Failed to perform frustum culling for spotlight #{}!", i);
					continue;
				}
			}

			for (Entity *ent : entitiesToCastShadows)
			{
				if (!ent->InitialRender(*spotlightCamera, spotlightCamera->GetRendererInfo()))
				{
					ErrMsgF("Failed to render entity for spotlight #{}!", i);
					break;
				}
			}

			spotlightCamera->SortGeometryQueue();
		}

#pragma omp for schedule(dynamic) nowait
		for (int i = 0; i < pointlightCount; i++)
		{
			ZoneNamedXNC(cullPointlightZone, "Cull Pointlight", RandomUniqueColor(), true);

			if (!_pointlights.get()->GetLightBehaviour(i)->DoUpdate())
				continue;

			B_CameraCube *pointlightCamera = _pointlights.get()->GetLightBehaviour(i)->GetShadowCameraCube();

			std::vector<Entity *> entitiesToCastShadows;
			entitiesToCastShadows.reserve(pointlightCamera->GetCullCount());

			dx::BoundingBox pointlightBox;
			if (!pointlightCamera->StoreBounds(pointlightBox))
			{
				ErrMsg("Failed to store pointlight bounds!");
				continue;
			}

			bool intersectBoxResult = false;
			if (isCameraOrtho)	intersectBoxResult = intersectBoxResult || view.box.Intersects(pointlightBox);
			else				intersectBoxResult = intersectBoxResult || view.frustum.Intersects(pointlightBox);

			if (!intersectBoxResult)
			{ 
				// Skip rendering if the camera frustum doesn't intersect the point light bounds
				_pointlights->SetLightEnabled(i, false);
				continue;
			}

			if (!_sceneHolder.BoxCull(pointlightBox, entitiesToCastShadows))
			{
				ErrMsgF("Failed to perform box culling for pointlight #{}!", i);
				continue;
			}

			pointlightCamera->SetCullCount(static_cast<UINT>(entitiesToCastShadows.size()));

			_pointlights->SetLightEnabled(i, true);

			for (Entity *ent : entitiesToCastShadows)
			{
				dx::BoundingOrientedBox entBounds;
				ent->StoreEntityBounds(entBounds);

				if (!pointlightBox.Intersects(entBounds))
					continue;

				if (!ent->InitialRender(*pointlightCamera, pointlightCamera->GetRendererInfo()))
				{
					ErrMsgF("Failed to render entity for pointlight #{}!", i);
					break;
				}
			}

			pointlightCamera->SortGeometryQueue();
		}
	}
#pragma warning(default: 6993)

	// Calculate light tiles
	{
		ZoneNamedNC(calculateLightTilesZone, "Calculate Light Tiles", RandomUniqueColor(), true);

		_graphics->ResetLightGrid(); // Clear light grid buffer
		const UINT lightTileCount = LIGHT_GRID_RES * LIGHT_GRID_RES;

		const CamBounds *lightGridBounds = _mainCamera.Get()->GetLightGridBounds();
		if (!lightGridBounds)
		{
			ErrMsg("Failed to get light grid bounds!");
			return false;
		}

		// Draw light tiles for debugging
		if (false)
		{
			std::vector<DebugDraw::Line> lines;
			lines.reserve(12);

			for (UINT i = 0; i < 12; i++)
			{
				DebugDraw::Line line{};

				line.start.color = { 1.0f, 0.0f, 0.0f, 1.0f };
				line.start.size = 0.01f;

				line.end.color = { 1.0f, 0.0f, 0.0f, 1.0f };
				line.end.size = 0.01f;

				lines.emplace_back(line);
			}

			//     Near    Far
			//    0----1  4----5
			//    |    |  |    |
			//    |    |  |    |
			//    3----2  7----6

			dx::XMFLOAT3 corners[8];
			for (UINT i = 0; i < lightTileCount; i++)
			{
				if (isCameraOrtho)
					lightGridBounds[i].ortho.GetCorners(corners);
				else
					lightGridBounds[i].perspective.GetCorners(corners);

				lines[0].start.position = corners[0];
				lines[0].end.position = corners[1];

				lines[1].start.position = corners[1];
				lines[1].end.position = corners[2];

				lines[2].start.position = corners[2];
				lines[2].end.position = corners[3];

				lines[3].start.position = corners[3];
				lines[3].end.position = corners[0];

				lines[4].start.position = corners[4];
				lines[4].end.position = corners[5];

				lines[5].start.position = corners[5];
				lines[5].end.position = corners[6];

				lines[6].start.position = corners[6];
				lines[6].end.position = corners[7];

				lines[7].start.position = corners[7];
				lines[7].end.position = corners[4];

				lines[8].start.position = corners[0];
				lines[8].end.position = corners[4];

				lines[9].start.position = corners[1];
				lines[9].end.position = corners[5];

				lines[10].start.position = corners[2];
				lines[10].end.position = corners[6];

				lines[11].start.position = corners[3];
				lines[11].end.position = corners[7];

				DebugDrawer::Instance().DrawLines(lines, false);
			}
		}

		dx::XMFLOAT3A cameraPos = _mainCamera.Get()->GetTransform()->GetPosition(World);

		const int spotlightCount = static_cast<int>(_spotlights->GetNrOfLights());
		const int pointlightCount = static_cast<int>(_pointlights->GetNrOfLights());
		const int simpleSpotlightCount = static_cast<int>(_spotlights->GetNrOfSimpleLights());
		const int simplePointlightCount = static_cast<int>(_pointlights->GetNrOfSimpleLights());

#pragma warning(disable: 6993)
#pragma omp parallel num_threads(PARALLEL_THREADS)
		{
#pragma omp for schedule(dynamic) nowait
			for (int i = 0; i < spotlightCount; i++)
			{
				ZoneNamedXNC(spotlightZone, "Calculate Spotlight Tiles", RandomUniqueColor(), true);

				if (!_spotlights->GetLightEnabled(i))
					continue;

				B_LightSpot *light = _spotlights->GetLightBehaviour(i);

				bool skipIntersectionTests = light->ContainsPoint(cameraPos);

				for (UINT j = 0; j < lightTileCount; j++)
				{
					if (!skipIntersectionTests)
					{
						if (isCameraOrtho)
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].ortho))
								continue;
						}
						else
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].perspective))
								continue;
						}
					}

#pragma omp critical
					{
						_graphics->AddLightToTile(j, i, SPOTLIGHT);
					}
				}
			}

#pragma omp for schedule(dynamic) nowait
			for (int i = 0; i < pointlightCount; i++)
			{
				ZoneNamedXNC(pointlightZone, "Calculate Pointlight Tiles", RandomUniqueColor(), true);

				// Disabled lights can be skipped
				if (!_pointlights->GetLightEnabled(i))
					continue;

				B_LightPoint *light = _pointlights->GetLightBehaviour(i);

				for (UINT j = 0; j < lightTileCount; j++)
				{
					if (isCameraOrtho)
					{
						if (!light->IntersectsLightTile(lightGridBounds[j].ortho))
							continue;
					}
					else
					{
						if (!light->IntersectsLightTile(lightGridBounds[j].perspective))
							continue;
					}

#pragma omp critical
					{
						_graphics->AddLightToTile(j, i, POINTLIGHT);
					}
				}
			}

#pragma omp for schedule(dynamic) nowait
			for (int i = 0; i < simpleSpotlightCount; i++)
			{
				ZoneNamedXNC(simpleSpotlightZone, "Calculate Simple Spotlight Tiles", RandomUniqueColor(), true);

				if (!_spotlights->GetSimpleLightEnabled(i))
					continue;

				B_LightSpotSimple *light = _spotlights->GetSimpleLightBehaviour(i);

				bool skipIntersectionTests = light->ContainsPoint(cameraPos);

				for (UINT j = 0; j < lightTileCount; j++)
				{
					if (!skipIntersectionTests)
					{
						if (isCameraOrtho)
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].ortho))
								continue;
						}
						else
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].perspective))
								continue;
						}
					}

#pragma omp critical
					{
						_graphics->AddLightToTile(j, i, SIMPLE_SPOTLIGHT);
					}
				}
			}

#pragma omp for schedule(dynamic) nowait
			for (int i = 0; i < simplePointlightCount; i++)
			{
				ZoneNamedXNC(simplePointlightZone, "Calculate Simple Pointlight Tiles", RandomUniqueColor(), true);

				// Disabled lights can be skipped
				if (!_pointlights->GetSimpleLightEnabled(i))
					continue;

				B_LightPointSimple *light = _pointlights->GetSimpleLightBehaviour(i);

				bool skipIntersectionTests = light->ContainsPoint(cameraPos);

				for (UINT j = 0; j < lightTileCount; j++)
				{
					if (!skipIntersectionTests)
					{
						if (isCameraOrtho)
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].ortho))
								continue;
						}
						else
						{
							if (!light->IntersectsLightTile(lightGridBounds[j].perspective))
								continue;
						}
					}

#pragma omp critical
					{
						_graphics->AddLightToTile(j, i, SIMPLE_POINTLIGHT);
					}
				}
			}
		}
#pragma warning(default: 6993)
	}

#ifdef DEBUG_BUILD
	if (_debugManager)
	{
		if (!_debugManager.Get()->InitialRender(*(_mainCamera.Get()), _mainCamera.Get()->GetRendererInfo()))
		{
			ErrMsg("Failed to render debug player!");
			return false;
		}
	}
#endif

	// Run BeforeRender on entities
	const UINT entityCount = _sceneHolder.GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		if (!_sceneHolder.GetEntity(i)->InitialBeforeRender())
		{
			ErrMsgF("Failed to run BeforeRender on entity '{}'!", _sceneHolder.GetEntity(i)->GetName());
			return false;
		}
	}

	return true;
}
#pragma endregion


#pragma region Getters & Setters
bool Scene::IsInitialized() const
{
	return _initialized;
}
void Scene::SetInitialized(bool state)
{
	_initialized = state;
}

bool Scene::IsDestroyed() const
{
	return _isDestroyed;
}

bool Scene::IsTransitionScene() const
{
	return _transitionScene;
}
void Scene::SetTransitionScene(bool state)
{
	_transitionScene = state;
}

const std::string &Scene::GetName() const noexcept
{
	return _sceneName;
}
bool Scene::SetName(const std::string &name)
{
	if (_initialized)
	{
		ErrMsg("Cannot change scene name after initialization!");
		return false;
	}

	_sceneName = name;
	return true;
}

void Scene::SetSceneVolume(float volume)
{
	_soundEngine.SetVolume(volume);
}

ID3D11Device *Scene::GetDevice() const
{
	return _device;
}
ID3D11DeviceContext *Scene::GetContext() const
{
	return _context;
}

Game *Scene::GetGame() const
{
	return _game;
}
Graphics *Scene::GetGraphics() const
{
	return _graphics;
}
Content *Scene::GetContent() const
{
	return _content;
}
const Input *Scene::GetInput() const
{
	return _input;
}

SceneHolder *Scene::GetSceneHolder()
{
	return &_sceneHolder;
}
JoltPhysicsInstance *Scene::GetPhysicsInstance()
{
	return &_physInstance;
}
SoundEngine *Scene::GetSoundEngine()
{
	return &_soundEngine;
}

LightSpotCollection *Scene::GetSpotlights() const
{
	return _spotlights.get();
}
LightPointCollection *Scene::GetPointlights() const
{
	return _pointlights.get();
}

#ifdef DEBUG_BUILD
B_DebugManager *Scene::GetDebugManager() const
{
	return _debugManager.Get();
}
void Scene::SetDebugManager(B_DebugManager *debugPlayer)
{
	_debugManager = debugPlayer;
}
void Scene::SetSelection(Entity *ent, bool additive)
{
	if (!_debugManager)
		return;

	_debugManager.Get()->Select(ent, additive);
}
Entity *Scene::GetPrimarySelection() const
{
	if (!_debugManager)
		return nullptr;

	return _debugManager.Get()->GetPrimarySelection();
}
#endif

void Scene::SetMainCamera(B_Camera *camera)
{
	_mainCamera = camera;
#ifdef DEBUG_BUILD
	if (_debugManager)
		_debugManager.Get()->SetCamera(camera);
#endif
}
B_Camera *Scene::GetMainCamera()
{
	return _mainCamera.Get();
}

void Scene::SetMainListener(B_SoundListener *listener)
{
	_mainListener = listener;
}
B_SoundListener *Scene::GetMainListener()
{
	return _mainListener.Get();
}

void Scene::SuspendSceneSound()
{
	_soundEngine.Suspend();
}
void Scene::ResumeSceneSound()
{
	_soundEngine.Resume();
}

const FogSettingsBuffer &Scene::GetFogSettings() const
{
	return _fogSettings;
}
void Scene::SetFogSettings(const FogSettingsBuffer &settings)
{
	_fogSettings = settings;
}

const EmissionSettingsBuffer &Scene::GetEmissionSettings() const
{
	return _emissionSettings;
}
void Scene::SetEmissionSettings(const EmissionSettingsBuffer &settings)
{
	_emissionSettings = settings;
}

const DepthOfFieldSettingsBuffer &Scene::GetDepthOfFieldSettings() const
{
	return _depthOfFieldSettings;
}
void Scene::SetDepthOfFieldSettings(const DepthOfFieldSettingsBuffer &settings)
{
	_depthOfFieldSettings = settings;
}

const dx::XMFLOAT3 &Scene::GetAmbientColor() const
{
	return _ambientColor;
}
void Scene::SetAmbientColor(const dx::XMFLOAT3 &color)
{
	_ambientColor = color;
}

const dx::XMFLOAT4 &Scene::GetSkyboxColor() const
{
	return _skyboxColor;
}
void Scene::SetSkyboxColor(const dx::XMFLOAT4 &color)
{
	_skyboxColor = color;
}
#pragma endregion


#pragma region Entity Creation
bool Scene::CreateEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume)
{
	*out = _sceneHolder.AddEntity(bounds, hasVolume);
	if (!(*out)->Initialize(_device, this, name))
	{
		ErrMsgF("Failed to initialize entity '{}'!", name);
		return false;
	}

	return true;
}

bool Scene::CreateMeshEntity(Entity **out, const std::string &name, UINT meshID, const Material &material, bool isTransparent, bool shadowCaster, bool isOverlay)
{
	const dx::BoundingOrientedBox bounds = _content->GetMesh(meshID)->GetBoundingOrientedBox();

	if (!CreateEntity(out, name, bounds, true))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	B_Mesh *mesh = new B_Mesh(bounds, meshID, &material, isTransparent, shadowCaster, isOverlay);

	if (!mesh->Initialize(*out))
	{
		ErrMsg("Failed to bind mesh to entity '" + name + "'!");
		return false;
	}

	return true;
}
#pragma endregion
