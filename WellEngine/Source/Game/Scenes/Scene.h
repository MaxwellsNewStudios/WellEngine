#pragma once

#pragma region Includes, Usings & Defines
#include <d3d11.h>

#include "rapidjson/document.h"
#include "SceneHolder.h"
#include "Entity.h"
#include "Rendering/Graphics.h"
#include "Rendering/Lighting/SpotLightCollection.h"
#include "Rendering/Lighting/PointLightCollection.h"
#include "Content/Material.h"
#include "Audio/SoundEngine.h"
#include "Behaviours/CameraBehaviour.h"
#include "Collision/CollisionHandler.h"
#include "Debug/DebugDrawer.h"
#include "GraphManager.h"
#include "Timing/TimelineManager.h"

namespace json = rapidjson;

// Forward declarations
class Game;
class GraphNodeBehaviour;
class MonsterBehaviour;
#ifdef DEBUG_BUILD
class DebugPlayerBehaviour;
#endif
#pragma endregion


// Contains and manages entities, cameras and lights. Also handles queueing entities for rendering.
class Scene : public IRefTarget<Scene>, public Identifiable
{
private:
	std::vector<std::unique_ptr<Entity>> _globalEntities = {};
	std::unique_ptr<SpotLightCollection> _spotlights;
	std::unique_ptr<PointLightCollection> _pointlights;

	bool _initialized = false;
	bool _isDestroyed = false;
	bool _unentered = true;
	bool _transitionScene = false;

	Game *_game = nullptr;
	ID3D11Device *_device = nullptr;
	ID3D11DeviceContext *_context = nullptr;
	Content *_content = nullptr;
	Graphics *_graphics = nullptr;
	GraphManager _graphManager = {};
	SceneHolder _sceneHolder;
	const Input *_input = nullptr;

	Ref<CameraBehaviour>
		_viewCamera = nullptr,
		_playerCamera = nullptr,
		_animationCamera = nullptr;

#ifdef DEBUG_BUILD
	Ref<DebugPlayerBehaviour> _debugPlayer = nullptr;
#endif
	Ref<Entity> _player = nullptr;
	Ref<Behaviour> _monster = nullptr;
	Ref<Behaviour> _terrainBehaviour = nullptr;
	const Collisions::Terrain *_terrain = nullptr;

	CollisionHandler _collisionHandler;
	SoundEngine _soundEngine;
	TimelineManager _timelineManager;

	dx::XMFLOAT3 _ambientColor = { 0.01f, 0.01f, 0.01f };
	UINT _envCubemapID = CONTENT_NULL;
	UINT _skyboxShaderID = CONTENT_NULL;
	FogSettingsBuffer _fogSettings = { };
	EmissionSettingsBuffer _emissionSettings = { };

#ifdef DEBUG_BUILD
	bool _isGeneratingEntityBounds = false;
	bool _isGeneratingVolumeTree = false;
	bool _isGeneratingCameraCulling = false;
	bool _rayCastFromMouse = false;
	int _cameraCubeSide = 0;
#endif

#ifdef USE_IMGUI
	bool _undockSceneHierarchy = false;
	bool _undockEntityHierarchy = false;

	std::vector<Ref<Entity>> _collapsedEntities = {};
#endif

	std::string _sceneName = "";

	std::vector<Behaviour *> _updateCallbacks;
	std::vector<Behaviour *> _parallelUpdateCallbacks;
	std::vector<Behaviour *> _lateUpdateCallbacks;
	std::vector<Behaviour *> _fixedUpdateCallbacks;

	std::vector<Behaviour *> _postDeserializeCallbacks;

	[[nodiscard]] bool UpdateSound();
	[[nodiscard]] bool MergeStaticEntities();

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderEntityCreatorUI();
	[[nodiscard]] bool RenderSceneHierarchyUI();
	[[nodiscard]] bool RenderSelectionHierarchyUI();
	[[nodiscard]] bool RenderEntityHierarchyUI(Entity *root, UINT depth, const std::string &search = "");
#endif

public:
#pragma region Initialization & Destruction
	Scene(std::string name, bool transitional = false);
	~Scene();
	Scene(const Scene &other) = default;
	Scene &operator=(const Scene &other) = default;
	Scene(Scene &&other) = default;
	Scene &operator=(Scene &&other) = default;

	[[nodiscard]] bool InitializeBase(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);
	[[nodiscard]] bool InitializeMenu(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);
	[[nodiscard]] bool InitializeEntr(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);
	[[nodiscard]] bool InitializeCave(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);
	[[nodiscard]] bool InitializeCred(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);

	void EnterScene();
	void ExitScene();
	void ResetScene();
#pragma endregion


#pragma region Update
	void AddUpdateCallback(Behaviour *beh);
	void RemoveUpdateCallback(Behaviour *beh);
	void AddParallelUpdateCallback(Behaviour *beh);
	void RemoveParallelUpdateCallback(Behaviour *beh);
	void AddLateUpdateCallback(Behaviour *beh);
	void RemoveLateUpdateCallback(Behaviour *beh);
	void AddFixedUpdateCallback(Behaviour *beh);
	void RemoveFixedUpdateCallback(Behaviour *beh);

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input);
	[[nodiscard]] bool LateUpdate(TimeUtils &time, const Input &input);
	[[nodiscard]] bool FixedUpdate(float deltaTime, const Input &input);

	[[nodiscard]] bool UpdateCullingTree();

#ifdef DEBUG_BUILD
	void UpdateBillboardGizmos();
#endif
#pragma endregion


#pragma region Render
	[[nodiscard]] bool Render(TimeUtils &time, const Input &input);

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI();
	[[nodiscard]] bool IsUndocked() const;
#endif
#ifdef USE_IMGUIZMO
	[[nodiscard]] bool RenderGizmoUI();
#endif
#pragma endregion


#pragma region Serialization
	[[nodiscard]] bool Serialize(bool asSaveFile);
	[[nodiscard]] bool SerializeEntity(json::Document::AllocatorType &docAlloc, json::Value &obj, Entity *entity, bool forceSerialize = false);

	[[nodiscard]] bool Deserialize(bool sceneReload = false);
	[[nodiscard]] bool DeserializeEntity(const json::Value &obj, Entity **out = nullptr);

	void AddPostDeserializeCallback(Behaviour *beh);
	void RunPostDeserializeCallbacks();
	void PostDeserialize();

	void GetPrefabNames(std::vector<std::string> &prefabs) const;
	[[nodiscard]] bool SaveAsPrefab(const std::string &name, Entity *entity);
	[[nodiscard]] bool DeletePrefab(const std::string &name);
	[[nodiscard]] Entity *SpawnPrefab(const std::string &name);
#pragma endregion


#pragma region Getters & Setters
	[[nodiscard]] bool IsInitialized() const;
	[[nodiscard]] bool IsDestroyed() const;
	[[nodiscard]] bool IsTransitionScene() const;
	void SetTransitionScene(bool state);
	void SetInitialized(bool state);
	void SetSceneVolume(float volume);
	void SuspendSceneSound();
	void ResumeSceneSound();
	[[nodiscard]] ID3D11Device *GetDevice() const;
	[[nodiscard]] ID3D11DeviceContext *GetContext() const;
	[[nodiscard]] Content *GetContent() const;
	[[nodiscard]] SceneHolder *GetSceneHolder();
	[[nodiscard]] Graphics *GetGraphics() const;
	[[nodiscard]] GraphManager *GetGraphManager();
	[[nodiscard]] const Input *GetInput() const;
	[[nodiscard]] CollisionHandler *GetCollisionHandler();
	[[nodiscard]] std::vector<std::unique_ptr<Entity>> *GetGlobalEntities();
	[[nodiscard]] SpotLightCollection *GetSpotlights() const;
	[[nodiscard]] PointLightCollection *GetPointlights() const;
#ifdef DEBUG_BUILD
	[[nodiscard]] DebugPlayerBehaviour *GetDebugPlayer() const;
	void SetDebugPlayer(DebugPlayerBehaviour *debugPlayer);
	void SetSelection(Entity *ent, bool additive = false);
#endif
	[[nodiscard]] TimelineManager* GetTimelineManager();
	[[nodiscard]] SoundEngine *GetSoundEngine();
	[[nodiscard]] Game *GetGame() const;
	[[nodiscard]] const std::string &GetName() const noexcept;
	[[nodiscard]] bool SetName(const std::string &name);
	[[nodiscard]] Entity *GetPlayer() const;
	void SetPlayer(Entity *player);
	[[nodiscard]] MonsterBehaviour *GetMonster() const;
	void SetMonster(MonsterBehaviour *monster);
	[[nodiscard]] ColliderBehaviour *GetTerrainBehaviour() const;
	[[nodiscard]] const Collisions::Terrain *GetTerrain() const;
	void SetViewCamera(CameraBehaviour *camera);
	[[nodiscard]] CameraBehaviour *GetViewCamera();
	[[nodiscard]] CameraBehaviour *GetPlayerCamera();
	[[nodiscard]] CameraBehaviour *GetAnimationCamera();
	[[nodiscard]] const FogSettingsBuffer &GetFogSettings() const;
	void SetFogSettings(const FogSettingsBuffer &settings);
	[[nodiscard]] const EmissionSettingsBuffer &GetEmissionSettings() const;
	void SetEmissionSettings(const EmissionSettingsBuffer &settings);
	[[nodiscard]] const dx::XMFLOAT3 &GetAmbientColor() const;
	void SetAmbientColor(const dx::XMFLOAT3 &color);
#pragma endregion


#pragma region Entity Creation
	[[nodiscard]] bool CreateEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume);
	[[nodiscard]] bool CreateGlobalEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume);

	[[nodiscard]] bool CreateMeshEntity(Entity **out, const std::string &name, UINT meshID, const Material &material, bool isTransparent = false, bool shadowCaster = true, bool isOverlay = false);
	[[nodiscard]] bool CreateBillboardMeshEntity(Entity **out, const std::string &name, const Material &material, float rotation = 0.0f, float normalOffset = 0.0f, float size = 1.0f, bool keepUpright = true, bool isTransparent = true, bool shadowCaster = false, bool isOverlay = false);
	[[nodiscard]] bool CreateGlobalMeshEntity(Entity **out, const std::string &name, UINT meshID, const Material &material, bool isTransparent = false, bool shadowCaster = true, bool isOverlay = false);

	[[nodiscard]] bool CreateCameraEntity(Entity **out, const std::string &name, float fov, float aspect, float nearZ, float farZ);
	[[nodiscard]] bool CreateAnimationCamera();

	[[nodiscard]] bool CreatePlayerEntity(Entity **out);
	[[nodiscard]] bool CreateMonsterEntity(Entity **out);

	[[nodiscard]] bool CreateLanternEntity(Entity **out);

	[[nodiscard]] bool CreateSpotLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float angle, bool ortho = false, float nearZ = 0.1f, UINT updateFrequency = 2, float fogStrength = 1.0f);
	[[nodiscard]] bool CreatePointLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float nearZ = 0.1f, UINT updateFrequency = 3, float fogStrength = 1.0f);
	[[nodiscard]] bool CreateSimpleSpotLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float angle, bool ortho = false, float fogStrength = 1.0f);
	[[nodiscard]] bool CreateSimplePointLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float fogStrength = 1.0f);

	[[nodiscard]] bool CreateGraphNodeEntity(Entity **out, GraphNodeBehaviour **node, dx::XMFLOAT3 pos);
	[[nodiscard]] bool CreateSoundEmitterEntity(Entity** out, const std::string& name, const std::string& fileName, bool loop = false, float volume = 1.0f, float distanceScaler = 75.0f, float reverbScaler = 1.0f, float minimumDelay = 2.0f, float maximumDelay = 10.0f);
#pragma endregion

	TESTABLE()
};
