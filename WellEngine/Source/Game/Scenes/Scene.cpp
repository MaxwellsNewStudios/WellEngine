#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "Scenes/Scene.h"
#include "Game.h"
#include "GraphManager.h"
#include "Audio/SoundEngine.h"

#include "Behaviours/BreadcrumbPileBehaviour.h"
#include "Behaviours/RestrictedViewBehaviour.h"
#include "Behaviours/PlayerCutsceneBehaviour.h"
#include "Behaviours/FlashlightPropBehaviour.h"
#include "Behaviours/AmbientSoundBehaviour.h"
#include "Behaviours/MonsterHintBehaviour.h"
#include "Behaviours/PlayerViewBehaviour.h"
#include "Behaviours/MenuCameraBehaviour.h"
#include "Behaviours/InventoryBehaviour.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "Behaviours/ButtonBehaviours.h"
#include "Behaviours/CreditsBehaviour.h"
#include "Behaviours/ExampleBehaviour.h"

#ifdef DEBUG_BUILD
#include "Behaviours/ExampleCollisionBehaviour.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#endif

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion


#pragma region Initialization & Destruction
Scene::Scene(std::string name, bool transitional) 
	: _sceneName(std::move(name)), _transitionScene(transitional)
{
	_spotlights = std::make_unique<SpotLightCollection>();
	_pointlights = std::make_unique<PointLightCollection>();
}
Scene::~Scene()
{
	_isDestroyed = true;
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
		_fogSettings.stepSize = 0.0f;
		_fogSettings.minSteps = 0;
		_fogSettings.maxSteps = 64;

		_emissionSettings.strength = 1.0f;
		_emissionSettings.exponent = 0.5f;
		_emissionSettings.threshold = 1.0f;
	}

#ifdef DEBUG_BUILD
	// Create global pointer gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("Sphere");
		Material mat;
		mat.textureID = _content->GetTextureID("White");
		mat.ambientID = _content->GetTextureID("White");
		mat.vsID = _content->GetShaderID("VS_Geometry");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Pointer Gizmo"))
		{
			ErrMsg("Failed to initialize pointer gizmo object!");
			return false;
		}

		MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
			ErrMsg("Failed to bind mesh to pointer gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create debug player
	{
		Entity *player = nullptr;
		if (!CreateEntity(&player, "DebugPlayer", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}
		player->SetSerialization(false);

		DebugPlayerBehaviour *behaviour = new DebugPlayerBehaviour();
		if (!behaviour->Initialize(player))
		{
			ErrMsg("Failed to initialize debug player behaviour!");
			return false;
		}

		_debugPlayer = behaviour;
	}
#endif

	// Deserialize scene
	if (!Deserialize())
	{
		Warn("Could not deserialize scene!");
		return false;
	}

	if (!MergeStaticEntities())
	{
		ErrMsg("Failed to merge static entities!");
		return false;
	}

	_collisionHandler.Initialize(this);

	_initialized = true;
	return true;
}
bool Scene::InitializeMenu(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume)
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

	if (!_soundEngine.Initialize(dx::AudioEngine_EnvironmentalReverb | dx::AudioEngine_ReverbUseFilters, dx::Reverb_Cave, gameVolume))
	{
		ErrMsg("Failed to initialize sound engine!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(50.0f, 50.0f, 50.0f));
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

	if (!_spotlights->Initialize(device, 128))
	{
		ErrMsg("Failed to initialize spotlight collection!");
		return false;
	}

	// Set visual effect parameters
	{
		_ambientColor = { 0.05f, 0.059f, 0.068f };

		_fogSettings.thickness = 0.04f;
		_fogSettings.stepSize = 0.0f;
		_fogSettings.minSteps = 0;
		_fogSettings.maxSteps = 64;

		_emissionSettings.strength = 1.0f;
		_emissionSettings.exponent = 0.5f;
		_emissionSettings.threshold = 1.0f;
	}

#ifdef DEBUG_BUILD
	// Create global transform gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("TranslationGizmo");
		Material mat;
		mat.textureID = _content->GetTextureID("TransformGizmo");
		mat.ambientID = _content->GetTextureID("TransformGizmo");
		mat.samplerID = _content->GetSamplerID("Point");
		mat.vsID = _content->GetShaderID("VS_Geometry");
		mat.psID = _content->GetShaderID("PS_DebugViewDiffuse");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Transform Gizmo"))
		{
			ErrMsg("Failed to initialize transform gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.5f, 0.5f, 0.5f });

		/*MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
		ErrMsg("Failed to initialize transform gizmo object!");
		return false;
		}*/

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create global pointer gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("Sphere");
		Material mat;
		mat.textureID = _content->GetTextureID("White");
		mat.ambientID = _content->GetTextureID("White");
		mat.vsID = _content->GetShaderID("VS_Geometry");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Pointer Gizmo"))
		{
			ErrMsg("Failed to initialize pointer gizmo object!");
			return false;
		}

		MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
			ErrMsg("Failed to bind mesh to pointer gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create debug player
	{
		Entity* player = nullptr;
		if (!CreateEntity(&player, "DebugPlayer", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		player->SetSerialization(false);

		_debugPlayer = new DebugPlayerBehaviour();
		if (!_debugPlayer.Get()->Initialize(player))
		{
			ErrMsg("Failed to initialize debug player behaviour!");
			return false;
		}

		_debugPlayer.Get()->SetEnabled(false);
		_debugPlayer.Get()->UpdateGizmoBillboards();
	}
#endif

	if (false)
	{
		// Cave Wall
		{
			UINT meshID = content->GetMeshID("Cave_Wall");
			Material mat;
			mat.textureID = content->GetTextureID("MineSection");
			mat.normalID = content->GetTextureID("MineSection_Normal");

			Entity* ent = nullptr;
			if (!CreateMeshEntity(&ent, "Cave Wall", meshID, mat, false, false))
			{
				ErrMsg("Failed to create object!");
				return false;
			}
			auto entTransform = ent->GetTransform();
			entTransform->SetPosition({ 0.0f, 0.0f, 10.0f });
			entTransform->SetScale({ 5.0f, 5.0f, 5.0f });
		}

		// Buttons
		{
			UINT meshID = content->GetMeshID("GraniteRock");
			Material mat;
			mat.normalID = content->GetTextureID("Cave_Normal");

			dx::XMFLOAT3A buttonPos = { -3.5f, 1.5f, 5.0f };

			// Start button
			{
				std::string path = PATH_FILE_EXT(ASSET_PATH_SAVES, _sceneName, ASSET_EXT_SAVE);
				std::ifstream file(path);
				uintmax_t fileSize = 0;
				if (file.is_open())
				{
					fileSize = std::filesystem::file_size(path);
				}

				if (fileSize == 0)
				{
					mat.textureID = content->GetTextureID("Button_Start_Texture");
				}
				else
				{
					mat.textureID = content->GetTextureID("Button_Load_Texture");
				}

				Entity* ent = nullptr;
				if (!CreateMeshEntity(&ent, "StartButton", meshID, mat, false, false))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				Transform *entT = ent->GetTransform();
				entT->SetPosition(buttonPos);
				entT->SetScale({ 0.7f, 0.2f, 0.2f });

				PlayButtonBehaviour* play = new PlayButtonBehaviour();
				if (!play->Initialize(ent))
				{
					ErrMsg("Failed to initialize play button behaviour!");
					return false;
				}
			}

			// Save button
			{
				mat.textureID = content->GetTextureID("Button_Save_Texture");

				Entity* ent = nullptr;
				if (!CreateMeshEntity(&ent, "SaveButton", meshID, mat, false, false))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				buttonPos.y = 0.75f;
				ent->GetTransform()->SetPosition(buttonPos);
				ent->GetTransform()->SetScale({ 0.7f, 0.2f, 0.2f });

				SaveButtonBehaviour* save = new SaveButtonBehaviour();
				if (!save->Initialize(ent))
				{
					ErrMsg("Failed to initialize save button behaviour!");
					return false;
				}
			}

			// NewSave button
			{
				mat.textureID = content->GetTextureID("Button_NewSave_Texture");

				Entity* ent = nullptr;
				if (!CreateMeshEntity(&ent, "NewSaveButton", meshID, mat, false, false))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				buttonPos.y = 0.0f;
				ent->GetTransform()->SetPosition(buttonPos);
				ent->GetTransform()->SetScale({ 0.7f, 0.2f, 0.2f });

				NewSaveButtonBehaviour* save = new NewSaveButtonBehaviour();
				if (!save->Initialize(ent))
				{
					ErrMsg("Failed to initialize new save button behaviour!");
					return false;
				}
			}

			// Credits button
			{
				mat.textureID = content->GetTextureID("Button_Credits_Texture");

				Entity *ent = nullptr;
				if (!CreateMeshEntity(&ent, "CreditsButton", meshID, mat, false, false))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				buttonPos.y = -0.75f;
				ent->GetTransform()->SetPosition(buttonPos);
				ent->GetTransform()->SetScale({ 0.7f, 0.2f, 0.2f });

				CreditsButtonBehaviour *save = new CreditsButtonBehaviour();
				if (!save->Initialize(ent))
				{
					ErrMsg("Failed to initialize new save button behaviour!");
					return false;
				}
			}

			// Exit button
			{
				mat.textureID = content->GetTextureID("Button_Exit_Texture");

				Entity* ent = nullptr;
				if (!CreateMeshEntity(&ent, "ExitButton", meshID, mat, false, false))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				buttonPos.y = -1.5f;
				ent->GetTransform()->SetPosition(buttonPos);
				ent->GetTransform()->SetScale({ 0.7f, 0.2f, 0.2f });

				ExitButtonBehaviour* exit = new ExitButtonBehaviour();
				if (!exit->Initialize(ent))
				{
					ErrMsg("Failed to initialize exit button behaviour!");
					return false;
				}
			}
		}

		// Menu camera
		{
			Entity* menuCam = nullptr;
			if (!CreateEntity(&menuCam, "MenuCamera", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
			{
				ErrMsg("Failed to create menu camera!");
				return false;
			}

			MenuCameraBehaviour* camera = new MenuCameraBehaviour();
			if (!camera->Initialize(menuCam))
			{
				ErrMsg("Failed to initialize menu camera behaviour!");
				return false;
			}
		}

		// Props
		{
			// Flashlight
			{
				UINT meshID = content->GetMeshID("FlashlightBody");
				Material mat{};
				mat.textureID =	content->GetTextureID("FlashlightBody");
				mat.normalID = content->GetTextureID("FlashlightBody_Normal");
				mat.specularID = content->GetTextureID("FlashlightBody_Specular");
				mat.glossinessID = content->GetTextureID("FlashlightBody_Glossiness");

				Entity *flashlight = nullptr;
				if (!CreateMeshEntity(&flashlight, "FlashlightProp", meshID, mat))
				{
					ErrMsg("Failed to create object!");
					return false;
				}
				auto flashlightTransform = flashlight->GetTransform();
				flashlightTransform->SetPosition({ -10.74f, -9.51f, 18.36f });
				flashlightTransform->Rotate({ -12.69f * DEG_TO_RAD, 60.0f * DEG_TO_RAD, 336.75f * DEG_TO_RAD });
				flashlightTransform->SetScale({ 0.5f, 0.5f, 0.5f });

				MeshBehaviour *meshBehaviour = nullptr;
				flashlight->GetBehaviourByType<MeshBehaviour>(meshBehaviour);
				meshBehaviour->SetCastShadows(false);

				meshID = content->GetMeshID("FlashlightLever");
				mat = {};
				mat.textureID =	content->GetTextureID("FlashlightLever"),
				mat.normalID = content->GetTextureID("FlashlightLever_Normal"),
				mat.specularID = content->GetTextureID("FlashlightLever_Specular"),
				mat.glossinessID = content->GetTextureID("FlashlightLever_Glossiness");

				Entity *flashlightLever = nullptr;
				if (!CreateMeshEntity(&flashlightLever, "FlashlightLever", meshID, mat))
				{
					ErrMsg("Failed to initialize flashlight lever");
					return false;
				}
				flashlightLever->SetParent(_sceneHolder.GetEntityByName("FlashlightProp"));

				FlashlightPropBehaviour *flpb = new FlashlightPropBehaviour();
				if (!flpb->Initialize(flashlight))
				{
					ErrMsg("Failed to initialize flashlight prop behaviour!");
					return false;
				}
			}

			// Breadcrumb
			{
				const dx::BoundingOrientedBox bounds = { {0,0,0}, {0.2f, 0.2f, 0.2f}, {0,0,0,1} };

				Entity *crumbPile = nullptr;
				if (!CreateEntity(&crumbPile, "Breadcrumb Pile", bounds, true))
				{
					ErrMsg("Failed to create object!");
					return false;
				}

				crumbPile->GetTransform()->SetPosition({ 10.74f, -11.5f, 21.36f });
				crumbPile->GetTransform()->SetScale({ 5.0f, 5.0f, 5.0f });

				BreadcrumbPileBehaviour *behaviour = new BreadcrumbPileBehaviour();
				if (!behaviour->Initialize(crumbPile))
				{
					ErrMsg("Failed to initialize breadcrumb pile behaviour!");
					return false;
				}
			}

			// Controls sign
			{
				UINT meshID = content->GetMeshID("minesign2legsflipped");
				Material mat;
				mat.textureID = content->GetTextureID("minesign2legs_ControllsSign_Diffuse");
				mat.normalID = content->GetTextureID("minesign2legs_ControllsSign_Normal");
				mat.specularID = content->GetTextureID("minesign2legs_ControllsSign_Specular");
				mat.glossinessID = content->GetTextureID("minesign2legs_ControllsSign_Glossiness");

				Entity *sign = nullptr;
				if (!CreateMeshEntity(&sign, "Controls sign", meshID, mat))
				{
					ErrMsg("Failed to create controlls sign mesh!");
					return false;
				}

				auto signTransform = sign->GetTransform();
				signTransform->SetPosition({ 9.74f, -12.2f, 24.36f });
				signTransform->SetRotation({ 0.0f, 95.0f * DEG_TO_RAD, 0.0f, 1.0f });
				signTransform->SetScale({ 1.3f, 1.3f, 1.3f });
			}
		}
	}

	// Deserialize scene
	if (!Deserialize())
	{
		Warn("Could not deserialize scene!");
		return false;
	}

	if (!MergeStaticEntities())
	{
		ErrMsg("Failed to merge static entities!");
		return false;
	}

	_collisionHandler.Initialize(this);

	_initialized = true;
	return true;
}
bool Scene::InitializeEntr(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume)
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

	if (!_soundEngine.Initialize(
		dx::AudioEngine_EnvironmentalReverb | dx::AudioEngine_ReverbUseFilters /*| dx::AudioEngine_UseMasteringLimiter*/,
		dx::Reverb_Cave, gameVolume))
	{
		ErrMsg("Failed to initialize sound engine!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(300.0f, 55.0f, 300.0f));
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

	if (!_spotlights->Initialize(device, 256))
	{
		ErrMsg("Failed to initialize spotlight collection!");
		return false;
	}

	// Set visual effect parameters
	{
		_ambientColor = { 0.1f, 0.1f, 0.1f };

		_fogSettings.thickness = 0.25f;
		_fogSettings.stepSize = 0.0f;
		_fogSettings.minSteps = 0;
		_fogSettings.maxSteps = 96;

		_emissionSettings.strength = 1.25f;
		_emissionSettings.exponent = 0.5f;
		_emissionSettings.threshold = 1.0f;
	}

#ifdef DEBUG_BUILD
	// Create global transform gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("TranslationGizmo");
		Material mat;
		mat.textureID = _content->GetTextureID("TransformGizmo");
		mat.ambientID = _content->GetTextureID("TransformGizmo");
		mat.samplerID = _content->GetSamplerID("Point");
		mat.psID = _content->GetShaderID("PS_DebugViewDiffuse");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Transform Gizmo"))
		{
			ErrMsg("Failed to initialize transform gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.5f, 0.5f, 0.5f });

		/*MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
		ErrMsg("Failed to initialize transform gizmo object!");
		return false;
		}*/

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create global pointer gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("Sphere");
		Material mat;
		mat.textureID = _content->GetTextureID("White");
		mat.ambientID = _content->GetTextureID("White");
		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Pointer Gizmo"))
		{
			ErrMsg("Failed to initialize pointer gizmo object!");
			return false;
		}

		MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
			ErrMsg("Failed to bind mesh to pointer gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

		_globalEntities.emplace_back(std::move(entPtr));
	}
#endif

	// Floor
	{
		dx::BoundingOrientedBox bounds = { {0, 0.444183499f, 0}, {0.5f, 0.444177479f, 0.5f}, {0, 0, 0, 1} };

		Entity *terrainEnt = nullptr;
		if (!CreateEntity(&terrainEnt, "Terrain Floor", bounds, true))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		terrainEnt->SetSerialization(false);
		terrainEnt->GetTransform()->SetPosition({0.0f, -33.125f, 0.0f });
		terrainEnt->GetTransform()->SetScale({ 600.0f, 112.5f, 600.0f });

		ColliderBehaviour *colB = new ColliderBehaviour();
		if (!colB->Initialize(terrainEnt))
		{
			ErrMsg("Failed to initialize cave floor terrain!");
			return false;
		}

		Collisions::Terrain *terrainCol = new Collisions::Terrain(bounds.Center, bounds.Extents, content->GetHeightMap("CaveHeightmap"));
		colB->SetCollider(terrainCol);

		_terrainBehaviour = colB;
		_terrain = dynamic_cast<const Collisions::Terrain *>(colB->GetCollider());
	}

	// Create Player
	if (!_sceneHolder.GetEntityByName("Player Holder"))
	{
		Entity *player = nullptr;
		if (!CreatePlayerEntity(&player))
		{
			ErrMsg("Failed to create player entity!");
			return false;
		}

		PlayerCutsceneBehaviour *pcb = new PlayerCutsceneBehaviour();
		if (!pcb->Initialize(player, "Player Cutscene Controller"))
		{
			ErrMsg("Failed to initialize player cutscene behaviour!");
			return false;
		}

		_player.Get()->SetSerialization(false);
	}

	// Deserialize scene
	if (!Deserialize())
	{
		Warn("Could not deserialize scene!");
		return false;
	}

#if defined(DEBUG_BUILD) && !defined(USE_IMGUIZMO)
	// Create transform gizmo controller
	{
		Entity *ent = nullptr;
		if (!CreateEntity(&ent, "Transform Controller", BoundingOrientedBox({ 0,0,0 }, { 0.01f, 0.01f, 0.01f }, { 0,0,0,1 }), false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		ent->GetTransform()->SetScale({ 0.5f, 0.5f, 0.5f });

		_transformGizmo = new TransformGizmoBehaviour();
		if (!_transformGizmo->Initialize(ent))
		{
			ErrMsg("Failed to initialize transform gizmo behaviour!");
			return false;
		}
		ent->SetSerialization(false);
	}
#endif

	_collisionHandler.Initialize(this);

	_initialized = true;

	// Set view to player camera
	Entity *cam = _sceneHolder.GetEntityByName("playerCamera");
	CameraBehaviour *camBehaviour;
	if (cam->GetBehaviourByType<CameraBehaviour>(camBehaviour))
		SetViewCamera(camBehaviour);
	else
	{
		ErrMsg("Failed to get player camera behaviour!");
		return false;
	}

	return true;
}
bool Scene::InitializeCave(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume)
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

	if (!_soundEngine.Initialize(
		dx::AudioEngine_EnvironmentalReverb | dx::AudioEngine_ReverbUseFilters, dx::Reverb_Cave, gameVolume))
	{
		ErrMsg("Failed to initialize sound engine!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(300.0f, 55.0f, 300.0f));
	if (!_sceneHolder.Initialize(sceneBounds))
	{
		ErrMsg("Failed to initialize scene holder!");
		return false;
	}

	if (!_pointlights->Initialize(device, 128))
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
		_fogSettings.thickness = 0.2f;
		_fogSettings.stepSize = 0.0f;
		_fogSettings.minSteps = 0;
		_fogSettings.maxSteps = 96;

		_emissionSettings.strength = 1.2f;
		_emissionSettings.exponent = 0.5f;
		_emissionSettings.threshold = 1.0f;
	}
	
#ifdef DEBUG_BUILD
	// Create global pointer gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("Sphere");
		Material mat;
		mat.textureID = _content->GetTextureID("White");
		mat.ambientID = _content->GetTextureID("White");
		mat.vsID = _content->GetShaderID("VS_Geometry");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Pointer Gizmo"))
		{
			ErrMsg("Failed to initialize pointer gizmo object!");
			return false;
		}

		MeshBehaviour *mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
			ErrMsg("Failed to bind mesh to pointer gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create debug player
	{
		Entity *player = nullptr;
		if (!CreateEntity(&player, "DebugPlayer", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}
		player->SetSerialization(false);

		DebugPlayerBehaviour *behaviour = new DebugPlayerBehaviour();
		if (!behaviour->Initialize(player))
		{
			ErrMsg("Failed to initialize debug player behaviour!");
			return false;
		}

		_debugPlayer = behaviour;
	}
#endif

	// Deserialize scene
	if (!Deserialize())
	{
		Warn("Could not deserialize scene!");
		return false;
	}
	
#ifndef EDIT_MODE
	// HACK: Until a beter solution has been worked out
	_ambientColor = { 0.0185f, 0.0195f, 0.02f };

	// Create Player
	if (!_sceneHolder.GetEntityByName("Player Holder"))
	{
		Entity *player = nullptr;
		if (!CreatePlayerEntity(&player))
		{
			ErrMsg("Failed to create player entity!");
			return false;
		}
	}
	
	// Create Monster
	bool spawnMonster = true;
	Entity *monsterEnt = _sceneHolder.GetEntityByName("Monster");
	if (monsterEnt)
	{
		MonsterBehaviour *monster = nullptr;
		if (monsterEnt->GetBehaviourByType<MonsterBehaviour>(monster))
		{
			spawnMonster = false;
		}
		else if (!_sceneHolder.RemoveEntity(monsterEnt))
		{
			ErrMsg("Failed to remove monster entity!");
			return false;
		}
	}

	if (spawnMonster)
	{
		Entity *monster = nullptr;
		if (!CreateMonsterEntity(&monster))
		{
			ErrMsg("Failed to create monster entity!");
			return false;
		}

		monster->GetTransform()->SetScale({ 0.2f, 0.2f, 0.2f });
	}

#ifdef DISABLE_MONSTER
	monsterEnt = _sceneHolder.GetEntityByName("Monster");
	monsterEnt->Disable();
#endif
#endif

	// Terrian Colldier
	{
		// Floor
		dx::BoundingOrientedBox bounds = { {0, 0.444183499f, 0}, {0.5f, 0.444177479f, 0.5f}, {0, 0, 0, 1} };

		Entity *terrainEnt = nullptr;
		if (!CreateEntity(&terrainEnt, "Terrain Floor", bounds, true))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		terrainEnt->SetSerialization(false);
		terrainEnt->GetTransform()->SetPosition({ -5.505f, -33.125f, 0.0f });
		terrainEnt->GetTransform()->SetScale({ 600.0f, 112.5f, 600.0f });

		if (!_sceneHolder.ExcludeEntityFromTree(terrainEnt))
		{
			ErrMsg("Failed to exclude terrain entity from scene tree!");
			return false;
		}

		ColliderBehaviour *colB = new ColliderBehaviour();
		if (!colB->Initialize(terrainEnt))
		{
			ErrMsg("Failed to initialize cave floor terrain!");
			return false;
		}

		Collisions::Terrain *terrainCol = new Collisions::Terrain(bounds.Center, bounds.Extents, content->GetHeightMap("CaveHeightmap"));
		colB->SetCollider(terrainCol);

		_terrainBehaviour = colB;
		_terrain = dynamic_cast<const Collisions::Terrain*>(colB->GetCollider());

		// Roof
		if (!CreateEntity(&terrainEnt, "Terrain Roof", bounds, true))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		terrainEnt->SetSerialization(false);
		terrainEnt->GetTransform()->SetPosition({ -5.0f, -33.125f, 0.0f });
		terrainEnt->GetTransform()->SetScale({ 600.0f, 112.5f, 600.0f });

		if (!_sceneHolder.ExcludeEntityFromTree(terrainEnt))
		{
			ErrMsg("Failed to exclude terrain entity from scene tree!");
			return false;
		}

		colB = new ColliderBehaviour();
		if (!colB->Initialize(terrainEnt))
		{
			ErrMsg("Failed to initialize cave roof terrain!");
			return false;
		}

		terrainCol = new Collisions::Terrain(bounds.Center, bounds.Extents, content->GetHeightMap("CaveRoofHeightmap"), Collisions::NULL_TAG, true);
		colB->SetCollider(terrainCol);

		// Walls
		if (!CreateEntity(&terrainEnt, "Terrain Walls", bounds, true))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		terrainEnt->SetSerialization(false);
		terrainEnt->GetTransform()->SetPosition({ -5.64f, -33.125f, 0.0f });
		terrainEnt->GetTransform()->SetScale({ 601.0f, 112.5f, 600.0f });

		if (!_sceneHolder.ExcludeEntityFromTree(terrainEnt))
		{
			ErrMsg("Failed to exclude terrain entity from scene tree!");
			return false;
		}

		colB = new ColliderBehaviour();
		if (!colB->Initialize(terrainEnt))
		{
			ErrMsg("Failed to initialize cave roof terrain!");
			return false;
		}

		terrainCol = new Collisions::Terrain(bounds.Center, bounds.Extents, content->GetHeightMap("CaveWallsHeightmap"), Collisions::NULL_TAG, false, true);
		colB->SetCollider(terrainCol);


#ifdef DEBUG_BUILD
		// Maxwell (for testing purposes)
		{
			UINT meshID = content->GetMeshID("Maxwell");
			Material mat{};
			mat.textureID = content->GetTextureID("Maxwell");
			mat.ambientID = content->GetTextureID("AmbientBright");

			Entity *maxwell = nullptr;
			if (!CreateMeshEntity(&maxwell, "Maxwell [labrat]", meshID, mat))
			{
				ErrMsg("Failed to create object!");
				return false;
			}
			maxwell->SetSerialization(false);

			dx::BoundingOrientedBox bounds = content->GetMesh(meshID)->GetBoundingOrientedBox();
			ColliderBehaviour *colB = new ColliderBehaviour();
			if (!colB->Initialize(maxwell))
			{
				ErrMsg("Failed to initialize Maxwell the labrat!");
				return false;
			}

			auto col = new Collisions::Sphere(bounds.Center, bounds.Extents.x, Collisions::OBJECT_TAG);
			colB->SetCollider(col);

			ExampleCollisionBehaviour *behaviour = new ExampleCollisionBehaviour();
			if (!behaviour->Initialize(maxwell))
			{
				ErrMsg("Failed to initialize example behaviour!");
				return false;
			}

			meshID = content->GetMeshID("Whiskers");
			mat.textureID = content->GetTextureID("Whiskers");
			mat.specularID = content->GetTextureID("Black_Specular");

			Entity *whiskers = nullptr;
			if (!CreateMeshEntity(&whiskers, "Whiskers", meshID, mat, true))
			{
				ErrMsg("Failed to create object!");
				return false;
			}

			whiskers->SetSerialization(false);
			whiskers->SetParent(maxwell);
		}
#endif
	}

	if (!CreateAnimationCamera())
	{
		ErrMsg("Failed to create animation camera!");
		return false;
	}

	_collisionHandler.Initialize(this);
	_initialized = true;

#ifndef EDIT_MODE
	// Set view to player camera
	Entity *cam = _sceneHolder.GetEntityByName("playerCamera");
	CameraBehaviour *camBehaviour;
	if (cam->GetBehaviourByType<CameraBehaviour>(camBehaviour))
	{
		SetViewCamera(camBehaviour);
#ifdef DEBUG_BUILD
		_debugPlayer.Get()->SetCamera(camBehaviour);
#endif
	}
#endif

	if (!MergeStaticEntities())
	{
		ErrMsg("Failed to merge static entities!");
		return false;
	}

	return true;
}
bool Scene::InitializeCred(std::string sceneName, ID3D11Device *device, ID3D11DeviceContext *context, Game *game, Content *content, Graphics *graphics, float gameVolume)
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

	if (!_soundEngine.Initialize(
		dx::AudioEngine_EnvironmentalReverb | dx::AudioEngine_ReverbUseFilters /*| dx::AudioEngine_UseMasteringLimiter*/,
		dx::Reverb_Cave, gameVolume))
	{
		ErrMsg("Failed to initialize sound engine!");
		return false;
	}

	// Create scene content holder
	constexpr dx::BoundingBox sceneBounds = dx::BoundingBox(dx::XMFLOAT3(0, 0, 0), dx::XMFLOAT3(50.0f, 50.0f, 50.0f));
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

	// Set visual effect parameters
	{
		_ambientColor = { 0.25f, 0.25f, 0.25f };

		_fogSettings.thickness = 0.0f;
		_fogSettings.stepSize = 0.0f;
		_fogSettings.minSteps = 0;
		_fogSettings.maxSteps = 0;

		_emissionSettings.strength = 1.0f;
		_emissionSettings.exponent = 1.0f;
		_emissionSettings.threshold = 1.0f;
	}

#ifdef DEBUG_BUILD
	// Create global transform gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("TranslationGizmo");
		Material mat;
		mat.textureID = _content->GetTextureID("TransformGizmo");
		mat.ambientID = _content->GetTextureID("TransformGizmo");
		mat.samplerID = _content->GetSamplerID("Point");
		mat.vsID = _content->GetShaderID("VS_Geometry");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Transform Gizmo"))
		{
			ErrMsg("Failed to initialize transform gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.5f, 0.5f, 0.5f });

		/*MeshBehaviour* mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
		ErrMsg("Failed to initialize transform gizmo object!");
		return false;
		}*/

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create global pointer gizmo
	{
		dx::BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
		UINT meshID = _content->GetMeshID("Sphere");
		Material mat;
		mat.textureID = _content->GetTextureID("White");
		mat.ambientID = _content->GetTextureID("White");
		mat.vsID = _content->GetShaderID("VS_Geometry");
		mat.psID = _content->GetShaderID("PS_DebugViewDiffuse");

		std::unique_ptr<Entity> entPtr = std::make_unique<Entity>(-1, defaultBox);
		if (!entPtr->Initialize(_device, this, "Pointer Gizmo"))
		{
			ErrMsg("Failed to initialize pointer gizmo object!");
			return false;
		}

		MeshBehaviour *mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);

		if (!mesh->Initialize(entPtr.get()))
		{
			ErrMsg("Failed to bind mesh to pointer gizmo object!");
			return false;
		}
		entPtr.get()->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

		_globalEntities.emplace_back(std::move(entPtr));
	}

	// Create debug player
	{
		Entity *player = nullptr;
		if (!CreateEntity(&player, "DebugPlayer", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		DebugPlayerBehaviour *behaviour = new DebugPlayerBehaviour();
		if (!behaviour->Initialize(player))
		{
			ErrMsg("Failed to initialize debug player behaviour!");
			return false;
		}

		_debugPlayer = behaviour;
		behaviour->SetEnabled(false);
	}
#endif

	{
		// Credits mesh
		{
			UINT meshID = content->GetMeshID("Plane");
			Material mat = {};
			mat.textureID = content->GetTextureID("Credit_Logo_Texture");
			mat.ambientID = content->GetTextureID("White");
			mat.vsID = content->GetShaderID("VS_Geometry");

			Entity *credit;
			if (!CreateMeshEntity(&credit, "Credits Mesh", meshID, mat))
			{
				ErrMsg("Failed to create credit billboard mesh entity!");
				return false;
			}

			const float zLength = 6.0f;
			const float scale = zLength * 0.70625f;
			Transform *creditT = credit->GetTransform();
			creditT->SetPosition({ 0.0f, 0.0f, zLength });
			creditT->Rotate({ 90.0f * DEG_TO_RAD, 0.0f, 0.0f, 1.0f });
			creditT->SetScale({ -1.6f * scale, 1.0f, 0.9f * scale });
		}

		// Credits behaviour
		{
			Entity *ent;
			if (!CreateEntity(&ent, "Credits Manager", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
			{
				ErrMsg("Failed to create Credits Manager!");
				return false;
			}

			CreditsBehaviour *credits = new CreditsBehaviour();
			if (!credits->Initialize(ent))
			{
				ErrMsg("Failed to initialize credits behaviour!");
				return false;
			}
			credits->SetEnabled(false);
		}

		// Maxwell
		{
			UINT meshID = content->GetMeshID("Maxwell");
			Material mat{};
			mat.textureID = content->GetTextureID("Maxwell");
			mat.ambientID = content->GetTextureID("White");
			mat.vsID = content->GetShaderID("VS_Geometry");

			Entity *maxwell = nullptr;
			if (!CreateMeshEntity(&maxwell, "Maxwell", meshID, mat))
			{
				ErrMsg("Failed to create object!");
				return false;
			}

			maxwell->GetTransform()->SetPosition({ 0.0f, -0.5f, 3.0f });

			meshID = content->GetMeshID("Whiskers");
			mat.textureID = content->GetTextureID("Whiskers");
			mat.specularID = content->GetTextureID("Black_Specular");

			Entity *whiskers = nullptr;
			if (!CreateMeshEntity(&whiskers, "Whiskers", meshID, mat, true))
			{
				ErrMsg("Failed to create object!");
				return false;
			}
			whiskers->SetParent(maxwell);

			ExampleBehaviour *behaviour = new ExampleBehaviour();
			if (!behaviour->Initialize(maxwell))
			{
				ErrMsg("Failed to initialize example behaviour!");
				return false;
			}

			maxwell->Disable();
		}
	}

#if defined(DEBUG_BUILD) && !defined(USE_IMGUIZMO)
	// Create transform gizmo controller
	{
		Entity *ent = nullptr;
		if (!CreateEntity(&ent, "Transform Controller", BoundingOrientedBox({ 0,0,0 }, { 0.01f, 0.01f, 0.01f }, { 0,0,0,1 }), false))
		{
			ErrMsg("Failed to create object!");
			return false;
		}

		ent->GetTransform()->SetScale({ 0.5f, 0.5f, 0.5f });

		_transformGizmo = new TransformGizmoBehaviour();
		if (!_transformGizmo->Initialize(ent))
		{
			ErrMsg("Failed to initialize transform gizmo behaviour!");
			return false;
		}
	}
#endif

	_collisionHandler.Initialize(this);

	_initialized = true;
	return true;
}

bool Scene::MergeStaticEntities()
{
#if defined(MERGE_STATIC) //&& !defined(DEBUG_BUILD)
	ZoneScopedXC(RandomUniqueColor());

	std::vector<MeshBehaviour *> staticMeshes;

	// Find static mesh behaviours
	{
		std::vector<Entity *> entities;
		_sceneHolder.GetEntities(entities);

		for (int i = 0; i < entities.size(); i++)
		{
			Entity *ent = entities[i];

			MeshBehaviour *mesh = nullptr;
			if (ent->IsStatic() && ent->GetBehaviourByType<MeshBehaviour>(mesh))
				staticMeshes.emplace_back(mesh);
		}
	}

	while (staticMeshes.size() > 0)
	{
		std::vector<MeshBehaviour *> toMerge;

		MeshBehaviour *topMesh = staticMeshes[0];
		toMerge.emplace_back(topMesh);

		staticMeshes.erase(staticMeshes.begin());

		//const UINT topMeshID = topMesh->GetMeshID();
		const Material *topMat = topMesh->GetMaterial();

		for (int i = 0; i < staticMeshes.size(); i++)
		{
			MeshBehaviour *comparedMesh = staticMeshes[i];

			//if (comparedMesh->GetMeshID() != topMeshID)
			//	continue;

			if (comparedMesh->GetMaterial() != topMat)
				continue;

			toMerge.emplace_back(comparedMesh);
			staticMeshes.erase(staticMeshes.begin() + i);
			i--;
		}

		// Skip merging if only one mesh of this type was found
		if (toMerge.size() <= 1)
			continue;

		//const MeshD3D11 *mergingMesh = _content->GetMesh(topMeshID);
		//const MeshData *mergingMeshData = mergingMesh->GetMeshData();

		// Merge
		{
			MeshData *mergedMeshData = new MeshData();

			for (int i = 0; i < toMerge.size(); i++)
			{
				MeshBehaviour *mergingMeshBehaviour = toMerge[i];

				const UINT mergingMeshID = mergingMeshBehaviour->GetMeshID();
				const MeshD3D11 *mergingMesh = _content->GetMesh(mergingMeshID);
				const MeshData *mergingMeshData = mergingMesh->GetMeshData();

				mergedMeshData->MergeWithMesh(
					mergingMeshData, 
					&(toMerge[i]->GetTransform()->GetWorldMatrix())
				);
			}

			std::ostringstream matAddressStream; 
			matAddressStream << topMat;
			const std::string matAddressString =  matAddressStream.str();

			const std::string mergeName = std::format("Merged_#{}-{}", toMerge.size(), matAddressString);

			Entity *entMerged = nullptr;
			UINT mergedMeshID = _content->AddMesh(_device, std::format("{}", mergeName), &mergedMeshData);
			if (mergedMeshID == CONTENT_NULL)
			{
				ErrMsgF("Failed to add Mesh {}!", mergeName);
				return false;
			}

			if (!CreateMeshEntity(&entMerged, mergeName, mergedMeshID, *topMat))
			{
				ErrMsgF("Failed to create merged mesh entity '{}'!", mergeName);
				return false;
			}
			entMerged->SetSerialization(false);
			entMerged->SetStatic(true);
			entMerged->SetDebugSelectable(false);
		}

		// Remove merged mesh behaviours
		while (toMerge.size() > 0)
		{
			MeshBehaviour *mb = toMerge[0];
			Entity *ent = mb->GetEntity();

			ent->RemoveBehaviour(mb);
			toMerge.erase(toMerge.begin());

			// Remove entity if it has no children and no behaviours left
			if (ent->GetBehaviourCount() <= 0 && ent->GetChildCount() <= 0)
			{
				if (!_sceneHolder.RemoveEntity(ent))
				{
					ErrMsgF("Failed to remove entity '{}'!", ent->GetName());
					return false;
				}
			}
		}
	}
#endif
	return true;
}

void Scene::EnterScene()
{
	_graphics->SetAmbientColor(_ambientColor);
	_graphics->SetFogSettings(_fogSettings);
	_graphics->SetEmissionSettings(_emissionSettings);

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
	_fogSettings = _graphics->GetFogSettings();
	_emissionSettings = _graphics->GetEmissionSettings();
	_envCubemapID = _graphics->GetEnvironmentCubemapID();
	_skyboxShaderID = _graphics->GetSkyboxShaderID();

	SuspendSceneSound();
}
void Scene::ResetScene()
{
	_globalEntities.clear();

	_initialized = false;
	_game = nullptr;
	_device = nullptr;
	_context = nullptr;
	_content = nullptr;
	_graphics = nullptr;
	_graphManager = {};
	_sceneHolder.ResetSceneHolder();
#ifdef DEBUG_BUILD
	_debugPlayer = nullptr;
#endif
	_input = nullptr;

	_viewCamera = nullptr;

	_player = nullptr;
	_monster = nullptr;
	_terrainBehaviour = nullptr;
	_terrain = nullptr;

	_collisionHandler = {};
	_soundEngine.ResetSoundEngine();

	_timelineManager = {};

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

	_spotlights = std::make_unique<SpotLightCollection>();
	_pointlights = std::make_unique<PointLightCollection>();
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

#pragma omp parallel for num_threads(PARALLEL_THREADS)
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

	if (!_collisionHandler.CheckCollisions(time, this, _context))
	{
		ErrMsg("Failed to performed collision checks!");
		return false;
	}

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

	if (!_graphics->SetPointlightCollection(_pointlights.get()))
	{
		ErrMsg("Failed to set pointlight collection!");
		return false;
	}

	if (_viewCamera)
	{
		if (!_viewCamera.Get()->UpdateBuffers())
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

	_timelineManager.Update(time);

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
	if (!_debugPlayer)
		return;

	_debugPlayer.Get()->UpdateGizmoBillboards();
}
#endif
#pragma endregion


#pragma region Render
bool Scene::Render(TimeUtils &time, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_initialized)
		return false;

	if (!_graphics->SetCamera(_viewCamera.Get()))
	{
		ErrMsg("Failed to set camera!");
		return false;
	}

	DebugDrawer::Instance().SetCamera(_viewCamera.Get());

	std::vector<Entity *> entitiesToRender;
	entitiesToRender.reserve(_viewCamera.Get()->GetCullCount());

	union {
		dx::BoundingFrustum frustum = {};
		dx::BoundingOrientedBox box;
	} view;
	bool isCameraOrtho = _viewCamera.Get()->GetOrtho();

	time.TakeSnapshot("FrustumCull");
	if (isCameraOrtho)
	{
		if (!_viewCamera.Get()->StoreBounds(view.box, false))
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
		if (!_viewCamera.Get()->StoreBounds(view.frustum, false))

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

		CamRenderQueuer queuer = { _viewCamera.Get() };
		if (!ent->InitialRender(queuer, _viewCamera.Get()->GetRendererInfo()))
		{
			ErrMsg("Failed to render entity!");
			return false;
		}
	}

	_viewCamera.Get()->SortGeometryQueue();
	if (_graphics->GetRenderTransparent())
		_viewCamera.Get()->SortTransparentQueue();
	if (_graphics->GetRenderOverlay())
		_viewCamera.Get()->SortOverlayQueue();

	time.TakeSnapshot("FrustumCull");

	const int spotlightCount = static_cast<int>(_spotlights->GetNrOfLights());
	const int pointlightCount = static_cast<int>(_pointlights->GetNrOfLights());

#pragma warning(disable: 6993)
#pragma omp parallel num_threads(PARALLEL_THREADS)
	{
#pragma omp for nowait
		for (int i = 0; i < spotlightCount; i++)
		{
			ZoneNamedXNC(cullSpotlightZone, "Cull Spotlight", RandomUniqueColor(), true);

			if (!_spotlights.get()->GetLightBehaviour(i)->DoUpdate())
				continue;

			CameraBehaviour *spotlightCamera = _spotlights.get()->GetLightBehaviour(i)->GetShadowCamera();

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
				CamRenderQueuer queuer = { spotlightCamera };
				if (!ent->InitialRender(queuer, spotlightCamera->GetRendererInfo()))
				{
					ErrMsgF("Failed to render entity for spotlight #{}!", i);
					break;
				}
			}

			spotlightCamera->SortGeometryQueue();
		}

#pragma omp for nowait
		for (int i = 0; i < pointlightCount; i++)
		{
			ZoneNamedXNC(cullPointlightZone, "Cull Pointlight", RandomUniqueColor(), true);

			if (!_pointlights.get()->GetLightBehaviour(i)->DoUpdate())
				continue;

			CameraCubeBehaviour *pointlightCamera = _pointlights.get()->GetLightBehaviour(i)->GetShadowCameraCube();

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

				CubeRenderQueuer queuer = { pointlightCamera };
				if (!ent->InitialRender(queuer, pointlightCamera->GetRendererInfo()))
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

		const CamBounds *lightGridBounds = _viewCamera.Get()->GetLightGridBounds();
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

		dx::XMFLOAT3A cameraPos = _viewCamera.Get()->GetTransform()->GetPosition(World);

		const int spotlightCount = static_cast<int>(_spotlights->GetNrOfLights());
		const int pointlightCount = static_cast<int>(_pointlights->GetNrOfLights());
		const int simpleSpotlightCount = static_cast<int>(_spotlights->GetNrOfSimpleLights());
		const int simplePointlightCount = static_cast<int>(_pointlights->GetNrOfSimpleLights());

#pragma warning(disable: 6993)
#pragma omp parallel num_threads(PARALLEL_THREADS)
		{
#pragma omp for nowait
			for (int i = 0; i < spotlightCount; i++)
			{
				ZoneNamedXNC(spotlightZone, "Calculate Spotlight Tiles", RandomUniqueColor(), true);

				if (!_spotlights->GetLightEnabled(i))
					continue;

				SpotLightBehaviour *light = _spotlights->GetLightBehaviour(i);

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

#pragma omp for nowait
			for (int i = 0; i < pointlightCount; i++)
			{
				ZoneNamedXNC(pointlightZone, "Calculate Pointlight Tiles", RandomUniqueColor(), true);

				// Disabled lights can be skipped
				if (!_pointlights->GetLightEnabled(i))
					continue;

				PointLightBehaviour *light = _pointlights->GetLightBehaviour(i);

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

#pragma omp for nowait
			for (int i = 0; i < simpleSpotlightCount; i++)
			{
				ZoneNamedXNC(simpleSpotlightZone, "Calculate Simple Spotlight Tiles", RandomUniqueColor(), true);

				if (!_spotlights->GetSimpleLightEnabled(i))
					continue;

				SimpleSpotLightBehaviour *light = _spotlights->GetSimpleLightBehaviour(i);

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

#pragma omp for nowait
			for (int i = 0; i < simplePointlightCount; i++)
			{
				ZoneNamedXNC(simplePointlightZone, "Calculate Simple Pointlight Tiles", RandomUniqueColor(), true);

				// Disabled lights can be skipped
				if (!_pointlights->GetSimpleLightEnabled(i))
					continue;

				SimplePointLightBehaviour *light = _pointlights->GetSimpleLightBehaviour(i);

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
	if (_debugPlayer)
	{
		CamRenderQueuer queuer = { _viewCamera.Get() };
		if (!_debugPlayer.Get()->InitialRender(queuer, _viewCamera.Get()->GetRendererInfo()))
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

	const UINT globalEntityCount = static_cast<UINT>(_globalEntities.size());
	for (UINT i = 0; i < globalEntityCount; i++)
	{
		if (!_globalEntities[i]->InitialBeforeRender())
		{
			ErrMsgF("Failed to run BeforeRender on global entity '{}'!", _globalEntities[i]->GetName());
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
SoundEngine *Scene::GetSoundEngine()
{
	return &_soundEngine;
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
CollisionHandler *Scene::GetCollisionHandler()
{
	return &_collisionHandler;
}
TimelineManager* Scene::GetTimelineManager()
{
	return &_timelineManager;
}
GraphManager *Scene::GetGraphManager()
{
	return &_graphManager;
}

std::vector<std::unique_ptr<Entity>> *Scene::GetGlobalEntities()
{
	return &_globalEntities;
}
SpotLightCollection *Scene::GetSpotlights() const
{
	return _spotlights.get();
}
PointLightCollection *Scene::GetPointlights() const
{
	return _pointlights.get();
}

Entity *Scene::GetPlayer() const
{
	return _player.Get();
}
void Scene::SetPlayer(Entity *player)
{
	_player = player;
}

MonsterBehaviour *Scene::GetMonster() const
{
	return _monster.GetAs<MonsterBehaviour>();
}
void Scene::SetMonster(MonsterBehaviour *monster)
{
	_monster = monster;
}

ColliderBehaviour *Scene::GetTerrainBehaviour() const
{
	return _terrainBehaviour.GetAs<ColliderBehaviour>();
}
const Collisions::Terrain *Scene::GetTerrain() const
{
	return _terrain;
}

#ifdef DEBUG_BUILD
DebugPlayerBehaviour *Scene::GetDebugPlayer() const
{
	return _debugPlayer.Get();
}
void Scene::SetDebugPlayer(DebugPlayerBehaviour *debugPlayer)
{
	_debugPlayer = debugPlayer;
}
void Scene::SetSelection(Entity *ent, bool additive)
{
	if (!_debugPlayer)
		return;

	_debugPlayer.Get()->Select(ent, additive);
}
#endif

void Scene::SetViewCamera(CameraBehaviour *camera)
{
	_viewCamera = camera;
#ifdef DEBUG_BUILD
	if (_debugPlayer)
		_debugPlayer.Get()->SetCamera(camera);
#endif
}
CameraBehaviour *Scene::GetViewCamera()
{
	return _viewCamera.Get();
}
CameraBehaviour *Scene::GetPlayerCamera()
{
	return _playerCamera.Get();
}
CameraBehaviour *Scene::GetAnimationCamera()
{
	return _animationCamera.Get();
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
const dx::XMFLOAT3 &Scene::GetAmbientColor() const
{
	return _ambientColor;
}
void Scene::SetAmbientColor(const dx::XMFLOAT3 &color)
{
	_ambientColor = color;
}
#pragma endregion


#pragma region Entity Creation
bool Scene::CreateGlobalEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume)
{
	*out = new Entity(0, bounds);
	if (!(*out)->Initialize(_device, this, name))
	{
		ErrMsg("Failed to initialize entity '" + name + "'!");
		return false;
	}

	return true;
}
bool Scene::CreateGlobalMeshEntity(Entity **out, const std::string &name, UINT meshID, const Material &material, bool isTransparent, bool shadowCaster, bool isOverlay)
{
	const dx::BoundingOrientedBox bounds = _content->GetMesh(meshID)->GetBoundingOrientedBox();

	if (!CreateGlobalEntity(out, name, bounds, true))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	MeshBehaviour *mesh = new MeshBehaviour(bounds, meshID, &material, isTransparent, shadowCaster, isOverlay);

	if (!mesh->Initialize(*out))
	{
		ErrMsg("Failed to bind mesh to entity '" + name + "'!");
		return false;
	}

	return true;
}

bool Scene::CreateEntity(Entity **out, const std::string &name, const dx::BoundingOrientedBox &bounds, bool hasVolume)
{
	*out = _sceneHolder.AddEntity(bounds, hasVolume);
	if (!(*out)->Initialize(_device, this, name))
	{
		ErrMsg("Failed to initialize entity '" + name + "'!");
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

	MeshBehaviour *mesh = new MeshBehaviour(bounds, meshID, &material, isTransparent, shadowCaster, isOverlay);

	if (!mesh->Initialize(*out))
	{
		ErrMsg("Failed to bind mesh to entity '" + name + "'!");
		return false;
	}

	return true;
}
bool Scene::CreateBillboardMeshEntity(
	Entity **out, const std::string &name, const Material &material, 
	float rotation, float normalOffset, float size, bool keepUpright,
	bool isTransparent, bool castShadows, bool isOverlay)
{
	const dx::BoundingOrientedBox bounds = { {0,0,0}, {0.1f, 0.1f, 0.1f}, {0,0,0,1} };

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	BillboardMeshBehaviour *mesh = new BillboardMeshBehaviour(material, rotation, normalOffset, size, keepUpright, isTransparent, castShadows, isOverlay);

	if (!mesh->Initialize(*out))
	{
		ErrMsg("Failed to bind billboard mesh to entity '" + name + "'!");
		return false;
	}

	return true;
}

bool Scene::CreateCameraEntity(Entity **out, const std::string &name, float fov, float aspect, float nearZ, float farZ)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .1f,.1f,.1f }, { 0,0,0,1 });

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	CameraBehaviour *camera = new CameraBehaviour(ProjectionInfo(fov * DEG_TO_RAD, aspect, { nearZ, farZ }));

	if (!camera->Initialize(*out))
	{
		ErrMsg("Failed to bind camera to entity '" + name + "'!");
		return false;
	}
	return true;
}
bool Scene::CreateAnimationCamera()
{
	Entity *animationCameraEnt = nullptr;
	if (!CreateEntity(&animationCameraEnt, "AnimationCamera", { {}, {.1f,.1f,.1f}, {0,0,0,1} }, false))
	{
		ErrMsg("Failed to create animation camera!");
		return false;
	}
	animationCameraEnt->SetSerialization(false);

	ProjectionInfo camInfo = ProjectionInfo(80.0f * DEG_TO_RAD, 16.0f / 9.0f, { 0.05f, 75.0f });
	_animationCamera = new CameraBehaviour(camInfo);
	if (!_animationCamera.Get()->Initialize(animationCameraEnt))
	{
		ErrMsg("Failed to initialize animation camera behaviour!");
		return false;
	}

	RestrictedViewBehaviour *viewBehaviour = new RestrictedViewBehaviour();
	if (!viewBehaviour->Initialize(animationCameraEnt))
	{
		ErrMsg("Failed to initialize animation camera restricted view behaviour!");
		return false;
	}
	viewBehaviour->SetEnabled(false);

	SimplePointLightBehaviour *lightBehaviour = new SimplePointLightBehaviour({ 0.09f, 0.10f, 0.13f }, 0.115f, 1.0f);
	if (!lightBehaviour->Initialize(animationCameraEnt))
	{
		ErrMsg("Failed to initialize animation camera darkness light behaviour!");
		return false;
	}
	lightBehaviour->SetEnabled(false);

	SoundBehaviour *dragSound = new SoundBehaviour("DragBody", (dx::SOUND_EFFECT_INSTANCE_FLAGS)(0x3), false, 50.0f, 0.25f);
	if (!dragSound->Initialize(animationCameraEnt))
	{
		ErrMsg("Failed to initialize drag sound behaviour!");
		return false;
	}
	dragSound->SetVolume(1.2f);
	dragSound->SetEnabled(false);

	animationCameraEnt->Disable();
	return true;
}

bool Scene::CreatePlayerEntity(Entity **out)
{
	float height = 1.85f;
	float width = 0.55f;
	float halfHeight = height * 0.5f;
	float halfWidth = width * 0.5f;
	float eyeHeight = height * 0.9f;

	Entity *playerHolderEntity = nullptr;
	if (!CreateEntity(&playerHolderEntity, "Player Holder", { {0,0,0}, {0.1f,0.1f,0.1f}, {0,0,0,1} }, false))
	{
		ErrMsg("Failed to initialize player holder!");
		return false;
	}

	Entity *playerEntity = nullptr;
	if (!CreateEntity(&playerEntity, "Player Entity", { {0, halfHeight, 0}, {halfWidth, halfHeight, halfWidth}, {0,0,0,1} }, false))
	{
		ErrMsg("Failed to initialize player!");
		return false;
	}

	playerEntity->SetParent(playerHolderEntity);
	playerEntity->GetTransform()->SetPosition({ -122.6f, 5.0f, -277.0f });

	PlayerMovementBehaviour *movementBehaviour = new PlayerMovementBehaviour();
	if (!movementBehaviour->Initialize(playerEntity))
	{
		ErrMsg("Failed to initialize movement example behaviour!");
		return false;
	}

	InteractorBehaviour *interactorBehaviour = new InteractorBehaviour();
	if (!interactorBehaviour->Initialize(playerEntity))
	{
		ErrMsg("Failed to initialize interactor behaviour!");
		return false;
	}

	InventoryBehaviour *inventoryBehaviour = new InventoryBehaviour();
	if (!inventoryBehaviour->Initialize(playerEntity))
	{
		ErrMsg("Failed to initialize player inventory behaviour!");
		return false;
	}

	MonsterHintBehaviour* monsterHintBehaviour = new MonsterHintBehaviour();
	if (!monsterHintBehaviour->Initialize(playerEntity))
	{
		ErrMsg("Failed to initialize monster hint behaviour!");
		return false;
	}

	ColliderBehaviour *collision = new ColliderBehaviour();
	if (!collision->Initialize(playerEntity))
	{
		ErrMsg("Failed to initialize collision behaviour!");
		return false;
	}

	auto col = new Collisions::Capsule({ 0.0f, halfHeight, 0.0f }, { 0, 1, 0 }, width, height, Collisions::SKIP_TERRAIN_TAG);
	collision->SetCollider(col);

	Entity *playerHeadTracker = nullptr;
	if (!CreateEntity(&playerHeadTracker, "Player Head Tracker", { {0,0,0},{.1f,.1f,.1f},{0,0,0,1} }, false))
	{
		ErrMsg("Failed to initialize player head tracker!");
		return false;
	}

	Transform *headTrackerTranform = playerHeadTracker->GetTransform();
	playerHeadTracker->SetParent(playerEntity);
	headTrackerTranform->SetPosition({ 0.0f, eyeHeight, 0.0f });

	movementBehaviour->SetHeadTracker(playerHeadTracker);


	Entity *cam = nullptr;
	if (!CreateEntity(&cam, "playerCamera", { {0,0,0},{.1f,.1f,.1f},{0,0,0,1} }, false))
	{
		ErrMsg("Failed to create playerCamera entity!");
		return false;
	}

	cam->SetParent(playerHolderEntity);
	Transform *camTranform = cam->GetTransform();
	camTranform->SetPosition(headTrackerTranform->GetPosition(World), World);
	camTranform->SetRotation(headTrackerTranform->GetRotation(World), World);

	ProjectionInfo camInfo = ProjectionInfo(80.0f * DEG_TO_RAD, 16.0f / 9.0f, { 0.05f, 100.0f });
	_playerCamera = new CameraBehaviour(camInfo);
	if (!_playerCamera.Get()->Initialize(cam))
	{
		ErrMsg("Failed to initialize UI example behaviour!");
		return false;
	}

	movementBehaviour->SetPlayerCamera(_playerCamera.Get());
	inventoryBehaviour->SetInventoryItemParents(cam);

	PlayerViewBehaviour *viewBehaviour = new PlayerViewBehaviour(movementBehaviour);
	if (!viewBehaviour->Initialize(cam))
	{
		ErrMsg("Failed to initialize player view behaviour!");
		return false;
	}
	
	float ambLightStr = 0.045f;
	SimplePointLightBehaviour *lightBehaviour = new SimplePointLightBehaviour({ 0.8f * ambLightStr, 0.85f * ambLightStr, 1.0f * ambLightStr }, 0.35f, 0.0f);
	if (!lightBehaviour->Initialize(cam))
	{
		ErrMsg("Failed to initialize player darkness light behaviour!");
		return false;
	}


	Entity *flashlight = nullptr;
	if (!CreateEntity(&flashlight, "Flashlight", { {0,0,0},{0.1f,0.1f,0.1f},{0,0,0,1} }, false))
	{
		ErrMsg("Failed to initialize flashlight holder!");
		return false;
	}
	flashlight->SetParent(cam);
	flashlight->GetTransform()->SetPosition({ 0.4f, -0.4f, 0.6f });
	flashlight->GetTransform()->SetEuler({ -0.045f, -0.06f, dx::XM_PI });

	FlashlightBehaviour *flashlightBehaviour = new FlashlightBehaviour();
	if (!flashlightBehaviour->Initialize(flashlight))
	{
		ErrMsg("Failed to initialize flashlight behaviour!");
		return false;
	}

	*out = playerEntity;
	_player = playerEntity;

#ifdef DEBUG_BUILD
	playerHolderEntity->SetSerialization(false);
	playerEntity->SetSerialization(false);
	playerHeadTracker->SetSerialization(false);
	cam->SetSerialization(false);
	flashlight->SetSerialization(false);
#endif
	return true;
}
bool Scene::CreateMonsterEntity(Entity **out)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .1f,.1f,.1f }, { 0,0,0,1 });

	if (!CreateEntity(out, "Monster", bounds, true))
	{
		ErrMsg("Failed to create monster entity!");
		return false;
	}

	_monster = new MonsterBehaviour();
	if (!_monster.Get()->Initialize(*out))
	{
		ErrMsg("Failed to bind monster behaviour to entity!");
		return false;
	}

#ifdef DEBUG_BUILD
	(*out)->SetSerialization(false);
#endif
	return true;
}

bool Scene::CreateLanternEntity(Entity **out)
{
	// Create lantern body
	UINT meshID = _content->GetMeshID("OldLamp_Metal");
	Material mat{};
	mat.textureID = _content->GetTextureID("OldLamp_Metal");
	mat.normalID = _content->GetTextureID("OldLamp_Metal_Normal");
	mat.specularID = _content->GetTextureID("OldLamp_Metal_Specular");
	mat.glossinessID = _content->GetTextureID("OldLamp_Metal_Glossiness");
	mat.ambientID = _content->GetTextureID("AmbientBright");
	mat.occlusionID = _content->GetTextureID("OldLamp_Metal_Occlusion");

	if (!CreateMeshEntity(out, "Lantern", meshID, mat))
	{
		ErrMsg("Failed to create lantern mesh entity!");
		return false;
	}
	(*out)->GetTransform()->SetScale({ 0.2f, 0.2f, 0.2f });

	// Create lantern glass
	meshID = _content->GetMeshID("OldLamp_Glass");
	mat.textureID = _content->GetTextureID("OldLamp_Glass");
	mat.normalID = _content->GetTextureID("OldLamp_Glass_Normal");
	mat.specularID = _content->GetTextureID("OldLamp_Glass_Specular");
	mat.glossinessID = _content->GetTextureID("OldLamp_Glass_Glossiness");
	mat.ambientID = _content->GetTextureID("AmbientBright");
	mat.occlusionID = _content->GetTextureID("OldLamp_Glass_Occlusion");

	Entity *glass = nullptr;
	if (!CreateMeshEntity(&glass, "Lantern Glass", meshID, mat, true, false))
	{
		ErrMsg("Failed to create lantern mesh entity!");
		return false;
	}
	glass->SetParent(*out);

	// Create lantern light
	Entity *light = nullptr;
	if (!CreatePointLightEntity(&light, "Lantern Light", { 8.0f, 6.5f, 4.0f }, 1.65f, 0.1f, 8))
	{
		ErrMsg("Failed to create lantern light entity!");
		return false;
	}
	light->SetParent(*out);
	light->GetTransform()->Move({0.0f, 1.25f, 0.0f});

	return true;
}

bool Scene::CreateSpotLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float angle, bool ortho, float nearZ, UINT updateFrequency, float fogStrength)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .2f,.2f,.2f }, {0,0,0,1});

	nearZ = nearZ < 0.1f ? 0.1f : nearZ;

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	ProjectionInfo projInfo = ProjectionInfo(angle * DEG_TO_RAD, 1.0f, { nearZ, 1.0f });
	SpotLightBehaviour *light = new SpotLightBehaviour(projInfo, color, falloff, fogStrength, ortho, updateFrequency);

	if (!light->Initialize(*out))
	{
		ErrMsg("Failed to bind spotlight to entity '" + name + "'!");
		return false;
	}

	return true;
}
bool Scene::CreatePointLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float nearZ, UINT updateFrequency, float fogStrength)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .2f,.2f,.2f }, {0,0,0,1});

	nearZ = nearZ < 0.1f ? 0.1f : nearZ;

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	PointLightBehaviour *light = new PointLightBehaviour({ nearZ, 1.0f }, color, falloff, updateFrequency);

	if (!light->Initialize(*out))
	{
		ErrMsg("Failed to bind pointlight to entity '" + name + "'!");
		return false;
	}

	return true;
}
bool Scene::CreateSimpleSpotLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float angle, bool ortho, float fogStrength)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .2f,.2f,.2f }, {0,0,0,1});

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	SimpleSpotLightBehaviour *light = new SimpleSpotLightBehaviour(color, angle * DEG_TO_RAD, falloff, ortho, 1.0f);

	if (!light->Initialize(*out))
	{
		ErrMsg("Failed to bind simple spotlight to entity '" + name + "'!");
		return false;
	}

	return true;
}
bool Scene::CreateSimplePointLightEntity(Entity **out, const std::string &name, dx::XMFLOAT3 color, float falloff, float fogStrength)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .2f,.2f,.2f }, {0,0,0,1});

	if (!CreateEntity(out, name, bounds, false))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	SimplePointLightBehaviour *light = new SimplePointLightBehaviour(color, falloff, fogStrength);

	if (!light->Initialize(*out))
	{
		ErrMsg("Failed to bind simple pointlight to entity '" + name + "'!");
		return false;
	}

	return true;
}

bool Scene::CreateGraphNodeEntity(Entity **out, GraphNodeBehaviour **node, dx::XMFLOAT3 pos)
{
	if (!CreateEntity(out, "Node", {{0,0,0}, {1,1,1}, {0,0,0,1}}, true))
	{
		ErrMsg("Failed to create Node entity!");
		return false;
	}

	(*out)->GetTransform()->SetPosition(To3(pos), World);

	*node = new GraphNodeBehaviour();
	if (!(*node)->Initialize(*out))
	{
		ErrMsg("Failed to bind GraphNode to entity!");
		return false;
	}

	return true;
}
bool Scene::CreateSoundEmitterEntity(Entity **out, const std::string &name, const std::string &fileName, bool loop, float volume, float distanceScaler, float reverbScaler, float minimumDelay, float maximumDelay)
{
	const dx::BoundingOrientedBox bounds = dx::BoundingOrientedBox({}, { .1f,.1f,.1f }, { 0,0,0,1 });

	if (!CreateEntity(out, name, bounds, true))
	{
		ErrMsg("Failed to create entity '" + name + "'!");
		return false;
	}

	AmbientSoundBehaviour* emitter = new AmbientSoundBehaviour(fileName, 
		dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters, 
		loop, volume, distanceScaler, reverbScaler, minimumDelay, maximumDelay
	);

	if (!emitter->Initialize(*out))
	{
		ErrMsg("Failed to bind sound behaviour to entity '" + name + "'!");
		return false;
	}
	return true;
}
#pragma endregion
