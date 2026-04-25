#pragma once

#include <d3d11.h>

#include "rapidjson/document.h"
#include "SceneHolder.h"
#include "Game/Entity.h"
#include "Engine/Rendering/Graphics/Graphics.h"
#include "Engine/Rendering/Lighting/LightSpotCollection.h"
#include "Engine/Rendering/Lighting/LightPointCollection.h"
#include "Engine/Content/Material.h"
#include "Engine/Audio/SoundEngine.h"
#include "Engine/Debug/DebugDrawer.h"
#include "Engine/Utils/UIDHelper.h"
#include "Engine/Physics/JoltPhysicsInstance.h"

namespace WellEngine
{
	namespace json = rapidjson;

	// Forward declarations
	class Game;
	class B_Camera;
	class B_SoundListener;
	#ifdef DEBUG_BUILD
	class B_DebugManager;
	#endif

	// Contains and manages entities, cameras and lights. Also handles queueing entities for rendering.
	class Scene : public IRefTarget<Scene>, public Identifiable
	{
	private:
		std::string _sceneName = "";

		SceneHolder _sceneHolder;
		JoltPhysicsInstance _physInstance;
		SoundEngine _soundEngine;

		std::unique_ptr<LightSpotCollection> _spotlights;
		std::unique_ptr<LightPointCollection> _pointlights;

		bool _initialized = false;
		bool _isDestroyed = false;
		bool _unentered = true;
		bool _transitionScene = false;

		ID3D11Device *_device = nullptr;
		ID3D11DeviceContext *_context = nullptr;
		Game *_game = nullptr;
		Content *_content = nullptr;
		Graphics *_graphics = nullptr;
		const Input *_input = nullptr;

		Ref<B_Camera> _mainCamera = nullptr;
		Ref<B_SoundListener> _mainListener = nullptr;
	#ifdef DEBUG_BUILD
		Ref<B_DebugManager> _debugManager = nullptr;
	#endif

		dx::XMFLOAT3 _ambientColor = { 0.01f, 0.01f, 0.01f };
		dx::XMFLOAT4 _skyboxColor = { 0, 0, 0, 0 };

		UINT _envCubemapID = CONTENT_NULL;
		UINT _skyboxShaderID = CONTENT_NULL;

		UINT _fogBlurIterations = 2;
		UINT _emissionBlurIterations = 2;
		std::vector<float> _fogGaussWeights = {};
		std::vector<float> _emissionGaussWeights = {};
		FogSettingsBuffer _fogSettings = { };
		EmissionSettingsBuffer _emissionSettings = { };
		DepthOfFieldSettingsBuffer _depthOfFieldSettings = {};

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
		bool _isHoveringHierarchyItem = false;
		bool _isHoveringHierarchy = false;

		std::vector<Ref<Entity>> _collapsedEntities = {};
	#endif

		std::vector<Behaviour *> _updateCallbacks;
		std::vector<Behaviour *> _parallelUpdateCallbacks;
		std::vector<Behaviour *> _lateUpdateCallbacks;
		std::vector<Behaviour *> _fixedUpdateCallbacks;
		std::vector<Behaviour *> _physicsUpdateCallbacks;
		std::vector<Behaviour *> _postDeserializeCallbacks;

		[[nodiscard]] bool InitCommon();

		[[nodiscard]] bool UpdateSound();

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderEntityCreatorUI();
		[[nodiscard]] bool RenderSceneHierarchyUI(bool skipCulling);
		[[nodiscard]] bool RenderSelectionHierarchyUI(bool skipCulling);
		[[nodiscard]] bool RenderEntityHierarchyUI(Entity *root, UINT depth, bool skipCulling, const std::string &search = "");
	#endif

	public:
	#pragma region Initialization & Destruction
		Scene(std::string name, bool transitional = false);
		~Scene();
		Scene(const Scene &other) = default;
		Scene &operator=(const Scene &other) = default;
		Scene(Scene &&other) = default;
		Scene &operator=(Scene &&other) = default;

		[[nodiscard]] bool InitializeNull(ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics);
		[[nodiscard]] bool InitializeBase(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume);

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

		void AddPhysicsUpdateCallback(Behaviour *beh);
		void RemovePhysicsUpdateCallback(Behaviour *beh);

		[[nodiscard]] bool Update(TimeUtils &time, const Input &input);
		[[nodiscard]] bool LateUpdate(TimeUtils &time, const Input &input);
		[[nodiscard]] bool FixedUpdate(float deltaTime, const Input &input);
		[[nodiscard]] bool PhysUpdate(float deltaTime);

		[[nodiscard]] bool UpdateCullingTree();

	#ifdef DEBUG_BUILD
		void UpdateBillboardGizmos();
	#endif
	#pragma endregion


	#pragma region Render
		[[nodiscard]] bool Render(TimeUtils &time, const Input &input);

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderSelectionUI();
		[[nodiscard]] bool RenderHierarchyMenuBarUI();
		[[nodiscard]] bool RenderHierarchyContextMenuUI();
		[[nodiscard]] bool RenderHierarchyUI(bool skipCulling = false);
		[[nodiscard]] bool RenderSceneUI();
	#endif
	#ifdef USE_IMGUIZMO
		[[nodiscard]] bool RenderGizmoUI();
	#endif
	#pragma endregion


	#pragma region Serialization
	private:
		[[nodiscard]] bool SerializeSceneSettings(json::Value &sceneSettingsObj, json::Document::AllocatorType &docAlloc);
		[[nodiscard]] bool DeserializeSceneSettings(const json::Value &sceneSettingsObj);

	public:
		[[nodiscard]] bool Serialize(bool asSaveFile);

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

		[[nodiscard]] SceneHolder *GetSceneHolder();
		[[nodiscard]] JoltPhysicsInstance *GetPhysicsInstance();
		[[nodiscard]] SoundEngine *GetSoundEngine();
		[[nodiscard]] LightSpotCollection *GetSpotlights() const;
		[[nodiscard]] LightPointCollection *GetPointlights() const;

		[[nodiscard]] ID3D11Device *GetDevice() const;
		[[nodiscard]] ID3D11DeviceContext *GetContext() const;
		[[nodiscard]] Game *GetGame() const;
		[[nodiscard]] Content *GetContent() const;
		[[nodiscard]] Graphics *GetGraphics() const;
		[[nodiscard]] const Input *GetInput() const;

	#ifdef DEBUG_BUILD
		[[nodiscard]] B_DebugManager *GetDebugManager() const;
		void SetDebugManager(B_DebugManager *debugPlayer);
		void SetSelection(Entity *ent, bool additive = false);
		[[nodiscard]] Entity *GetPrimarySelection() const;
	#endif

		[[nodiscard]] const std::string &GetName() const noexcept;
		[[nodiscard]] bool SetName(const std::string &name);

		void SetMainCamera(B_Camera *camera);
		[[nodiscard]] B_Camera *GetMainCamera();

		void SetMainListener(B_SoundListener *listener);
		[[nodiscard]] B_SoundListener *GetMainListener();

		[[nodiscard]] const FogSettingsBuffer &GetFogSettings() const;
		void SetFogSettings(const FogSettingsBuffer &settings);
		[[nodiscard]] const EmissionSettingsBuffer &GetEmissionSettings() const;
		void SetEmissionSettings(const EmissionSettingsBuffer &settings);
		[[nodiscard]] const DepthOfFieldSettingsBuffer &GetDepthOfFieldSettings() const;
		void SetDepthOfFieldSettings(const DepthOfFieldSettingsBuffer &settings);
		[[nodiscard]] const dx::XMFLOAT3 &GetAmbientColor() const;
		void SetAmbientColor(const dx::XMFLOAT3 &color);
		[[nodiscard]] const dx::XMFLOAT4 &GetSkyboxColor() const;
		void SetSkyboxColor(const dx::XMFLOAT4 &color);
	#pragma endregion


	#pragma region Entity Creation
		[[nodiscard]] bool CreateEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume);

		[[nodiscard]] bool CreateMeshEntity(Entity **out, const std::string &name, UINT meshID, const Material &material, bool isTransparent = false, bool shadowCaster = true, bool isOverlay = false);
	#pragma endregion

		TESTABLE
	};
}

