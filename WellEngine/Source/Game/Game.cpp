#include "stdafx.h"
#include "Game.h"
#include "Engine/Debug/DebugData.h"
#include "Engine/UI/UILayout.h"
#include "ContentManager/Registry/RegistryAssetFileManager.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;


Game::Game()
{
	_activeSceneIndex = -1;
}
Game::~Game()
{
	_systemManager.Shutdown();
	_graphics.Shutdown();
	_content.Shutdown();
	DebugDrawer::Instance().Shutdowm();
	_scenes.clear();

	if (_workerThread.joinable())
	{
		_workerState = false;
		_mainSemaphore.release();
		_workerSemaphore.release();
		_workerThread.join();
	}

	_immediateContext.Reset();
	_window = {};

#if defined(_DEBUG) && defined(DEBUG_BUILD)
	if (DebugData::Get().reportComObjectsOnShutdown)
	{
		ID3D11Device *devicePtr = _device.Get();
		ReportLiveDeviceObjects(devicePtr);
	}
#endif
}

bool Game::LoadContent()
{
	ZoneScopedC(RandomUniqueColor());

	using namespace ContentManager;
	using namespace ContentManager::Registry;

	std::vector<RegistryData> assetRegistryFiles = GetAssetRegistriesInDirectory(WE_D_REGISTRY, true);
	std::vector<RegistryData*> meshList, tex2dList, texCubeList, shaderList;

	for (RegistryData &entry : assetRegistryFiles)
	{
		switch (entry.header.assetType)
		{
		case AssetType::Mesh:
			meshList.push_back(&entry);
			break;
		case AssetType::Texture:
			tex2dList.push_back(&entry);
			break;
		case AssetType::Cubemap:
			texCubeList.push_back(&entry);
			break;
		case AssetType::Shader:
			shaderList.push_back(&entry);
			break;
		default:
			break;
		}
	}

	DbgMsg("Loading Meshes...");

	for (auto &entry : meshList)
	{
		AssetPropertiesMesh prop = {};
		if (!entry->properties.empty())
			prop = *(AssetPropertiesMesh *)(entry->properties.data());

		std::string name = entry->header.alias;

		if (_content.AddMesh(_device.Get(), name, (TO_SOLUTION_PATH + entry->header.assetPath).c_str(), false) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add mesh {}!", name);
			return false;
		}
	}

	int meshCount = (int)_content.GetMeshCount();

#pragma omp parallel num_threads(4)
	{
		int threadID = omp_get_thread_num();

		if (threadID == 0)
			DbgMsg("Generating Mesh Colliders...");

#pragma omp for schedule(dynamic) nowait
		for (int i = 0; i < meshCount; i++)
		{
			MeshD3D11 *mesh = _content.GetMesh((UINT)i);

			if (!mesh)
				continue;

			if (!mesh->GenerateCollider())
			{
				ErrMsgF("Failed to generate collider for mesh {}!", _content.GetMeshName((UINT)i));
			}
		}

		if (threadID == 0)
			DbgMsg("Loading Cubemaps...");

#pragma omp for schedule(dynamic) nowait
		for (int i = 0; i < texCubeList.size(); i++)
		{
			auto &entry = texCubeList[i];

			AssetPropertiesTexture prop = {};
			if (!entry->properties.empty())
				prop = *(AssetPropertiesTexture *)(entry->properties.data());

			TextureData cubemap = {};
			cubemap.path = TO_SOLUTION_PATH + entry->header.assetPath;
			cubemap.name = entry->header.alias;
			cubemap.type = prop.format;
			cubemap.mipmapped = prop.mipmapped;
			cubemap.downsample = prop.downsample;

			if (_content.AddCubemap(
				_device.Get(), _immediateContext.Get(),
				cubemap.name,
				cubemap.path,
				cubemap.type,
				cubemap.mipmapped,
				cubemap.downsample) == CONTENT_NULL)
			{
				ErrMsgF("Failed to add cubemap {}!", cubemap.name);
			}
		}
	}

	DbgMsg("Loading Textures...");

	for (int i = 0; i < tex2dList.size(); i++)
	{
		auto &entry = tex2dList[i];

		AssetPropertiesTexture prop = {};
		if (!entry->properties.empty())
			prop = *(AssetPropertiesTexture *)(entry->properties.data());

		TextureData texture = {};
		texture.path = TO_SOLUTION_PATH + entry->header.assetPath;
		texture.name = entry->header.alias;
		texture.type = prop.format;
		texture.mipmapped = prop.mipmapped;
		texture.downsample = prop.downsample;

		if (_content.AddTexture(
			_device.Get(), _immediateContext.Get(),
			texture.name,
			texture.path,
			texture.type,
			texture.mipmapped,
			texture.downsample) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add Tex {}!", texture.name);
		}
	}

	DbgMsg("Loading Shaders...");

	for (int i = 0; i < shaderList.size(); i++)
	{
		auto &entry = shaderList[i];

		AssetPropertiesShader prop = {};
		if (!entry->properties.empty())
			prop = *(AssetPropertiesShader *)(entry->properties.data());

		std::string fileName = fs::path(entry->header.assetPath).stem().string();

		ShaderData shader = {};
		shader.path = TO_SOLUTION_PATH + entry->header.assetPath;
		shader.name = entry->header.alias;
		shader.type = prop.type;

		if (_content.AddShader(_device.Get(), shader.name, shader.path, shader.type, WE_DFE(WE_D_COMPILED_CSO, fileName, "cso")) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add shader {}!", shader.name);
			return false;
		}
	}

	DbgMsg("Loading Fonts...");

	// Font Atlases
	for (const auto &entry : fs::directory_iterator(WE_D_DATA_ATLAS))
	{
		const auto &path = entry.path();
		std::string filename = path.filename().string();
		std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

		if (ext != WE_E_DATA_ATLAS)
			continue; // Skip non-font atlas files

		filename = filename.substr(0, filename.find_last_of('.'));

		size_t lastSlash = filename.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			filename = filename.substr(lastSlash + 1);

		if (_content.AddFontAtlas(filename) == CONTENT_NULL)
		{
			ErrMsgF("Failed to add font atlas {}!", filename);
			return false;
		}
	}

	// Samplers
	{
		if (_content.AddSampler(_device.Get(), "Fallback", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Fallback!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Point", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Point!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Clamp", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Clamp!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Wrap", D3D11_TEXTURE_ADDRESS_WRAP, D3D11_FILTER_ANISOTROPIC) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler Wrap!");
			return false;
		}

		D3D11_SAMPLER_DESC samplerDesc = { };
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		if (_content.AddSampler(_device.Get(), "HQ", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add sampler HQ!");
			return false;
		}

		samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;

		if (_content.AddSampler(_device.Get(), "Shadow", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add shadow sampler!");
			return false;
		}

		samplerDesc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;

		if (_content.AddSampler(_device.Get(), "ShadowCube", samplerDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add shadow cube sampler!");
			return false;
		}

		if (_content.AddSampler(_device.Get(), "Test", D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR) == CONTENT_NULL)
		{
			ErrMsg("Failed to add test sampler!");
			return false;
		}
	}
	
	// Blend States
	{
		D3D11_BLEND_DESC blendDesc = { };
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;

		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;

		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		if (_content.AddBlendState(_device.Get(), "Fallback", blendDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add fallback blend state!");
			return false;
		}

#ifdef DEBUG_BUILD
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_BLEND_FACTOR;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_BLEND_FACTOR;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

		if (_content.AddBlendState(_device.Get(), "Overdraw", blendDesc) == CONTENT_NULL)
		{
			ErrMsg("Failed to add overdraw blend state!");
			return false;
		}
#endif
	}

	// Input layouts
	{
		const std::vector<Semantic> fallbackInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32_FLOAT },
			{ "NORMAL",		DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TANGENT",	DXGI_FORMAT_R32G32B32_FLOAT },
			{ "TEXCOORD",	DXGI_FORMAT_R32G32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "Fallback", fallbackInputLayout, _content.GetShaderID("VS_Geometry")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_Fallback!");
			return false;
		}

#ifdef DEBUG_BUILD
		const std::vector<Semantic> debugDrawInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32_FLOAT		},
			{ "SIZE",		DXGI_FORMAT_R32_FLOAT			},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDraw", debugDrawInputLayout, _content.GetShaderID("VS_DebugDraw")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDraw!");
			return false;
		}
		const std::vector<Semantic> debugDrawBezierInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "CONTROL",	DXGI_FORMAT_R32G32B32_FLOAT		},
			{ "TESSFACTOR",	DXGI_FORMAT_R32_FLOAT			},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawBezier", debugDrawBezierInputLayout, _content.GetShaderID("VS_DebugDrawBezier")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawBezier!");
			return false;
		}

		const std::vector<Semantic> debugDrawTriInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	}
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawTri", debugDrawTriInputLayout, _content.GetShaderID("VS_DebugDrawTri")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawTri!");
			return false;
		}

		const std::vector<Semantic> debugDrawSpriteInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "COLOR",		DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "TEXCOORD",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "SIZE",		DXGI_FORMAT_R32G32_FLOAT		},
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawSprite", debugDrawSpriteInputLayout, _content.GetShaderID("VS_DebugDrawSprite")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawSprite!");
			return false;
		}

		const std::vector<Semantic> debugDrawMeshInputLayout{
			{ "POSITION",	DXGI_FORMAT_R32G32B32A32_FLOAT	},
			{ "NORMAL",		DXGI_FORMAT_R32G32B32A32_FLOAT	},
		};

		if (_content.AddInputLayout(_device.Get(), "DebugDrawMesh", debugDrawMeshInputLayout, _content.GetShaderID("VS_DebugDrawMesh")) == CONTENT_NULL)
		{
			ErrMsg("Failed to add IL_DebugDrawMesh!");
			return false;
		}
#endif
	}

	return true;
}

bool Game::Setup(TimeUtils &time, Window window)
{
	_window = std::move(window);
	const bool fullscreen = false;
	const UINT width = _window.GetWidth();
	const UINT height = _window.GetHeight();

	if (!_graphics.Setup(fullscreen, width, height, window,
		*_device.ReleaseAndGetAddressOf(), 
		*_immediateContext.ReleaseAndGetAddressOf(),
		&_content))
	{
		ErrMsg("Failed to setup d3d11!");
		return false;
	}

	ZoneScopedC(RandomUniqueColor());

	time.TakeSnapshot("LoadContent");
	if (!LoadContent())
	{
		ErrMsg("Failed to load game content!");
		return false;
	}
	time.TakeSnapshot("LoadContent");

	// Setup systems
	time.TakeSnapshot("SetupSystems");
	{
		if (!_systemManager.Initialize(this))
		{
			ErrMsg("Failed to setup systems!");
			return false;
		}
	}
	time.TakeSnapshot("SetupSystems");

	// Add all scenes & load the active scene
	time.TakeSnapshot("AddScenes");
	{
		Scene *tempScene = nullptr;

		// Search for all .scene files in ASSET_PATH_SCENES
		for (const auto &entry : std::filesystem::directory_iterator(WE_D_DATA_SCENE))
		{
			const auto &path = entry.path();
			std::string filename = path.filename().string();
			std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

			if (ext != WE_E_DATA_SCENE)
				continue; // Skip non-scene files

			filename = filename.substr(0, filename.find_last_of('.'));

			// Load the scene
			DbgMsgF("Adding Scene '{}'...", filename); LogIndentIncr();
			tempScene = new Scene(filename);
			if (!AddScene(&tempScene))
			{
				ErrMsg("Failed to add scene!");
				return false;
			}
			LogIndentDecr();
		}

#ifdef EDIT_MODE
		const std::string &activeSceneName = DebugData::Get().activeScene;
		DbgMsgF("Loading Active Scene '{}'...", activeSceneName); LogIndentIncr();

		// Set active scene to the game scene
		if (!SetScene(activeSceneName))
		{
			const std::string &firstScene = (*GetScenes())[0]->GetName();

			LogIndentDecr();
			DbgMsgF("Loading Active Scene Failed! \nFallback to '{}'...", firstScene);
			LogIndentIncr();

			if (!SetScene(firstScene))
			{
				ErrMsg("Failed to set scene!");
				return false;
			}
		}
#else
		DbgMsg("Loading Start-up Scene '" STARTUP_SCENE "'..."); LogIndentIncr();
		if (!SetScene(STARTUP_SCENE))
		{
			ErrMsg("Failed to set scene!");
			return false;
		}
#endif
		LogIndentDecr();
	}
	time.TakeSnapshot("AddScenes");

	// Open the pending scene (if there is one)
	if (!_pendingSceneChange.empty())
	{
		if (!SetSceneInternal(_pendingSceneChange))
			DbgMsgF("Failed to change scene to '{}'!", _pendingSceneChange);

		_pendingSceneChange.clear();
	}

#ifdef DEBUG_BUILD
#ifdef _DEBUG
	_graphics.notifications.emplace_back("Debug", 0);
#endif
#ifdef DEBUG_D3D11_DEVICE
	_graphics.notifications.emplace_back("D3D11 Debug Device", 0);
#endif
#if (MESH_COLLISION_DETAIL_REDUCTION == 3)
	_graphics.notifications.emplace_back("Mesh Collision Disabled", 2, 15.0f);
#endif
#endif

	// Create worker thread
	_workerThread = std::thread(&Game::UpdateWorker, this);

	return true;
}

bool Game::AddScene(Scene **newScene, const bool setActive)
{
	if (newScene == nullptr)
		return false;

	if (*newScene == nullptr)
		return false;

	// Take ownership of the scene
	Scene *scene = *newScene;
	(*newScene) = nullptr;

	// Ensure the scene name is unique
	for (const auto &existingScene : _scenes)
	{
		if (existingScene->GetName() == scene->GetName())
		{
			DbgMsgF("A scene with the name '{}' already exists!", scene->GetName());
			delete scene;
			return false;
		}
	}

	const UINT sceneIndex = (UINT)_scenes.size();
	_scenes.emplace_back(scene);

	if (setActive)
		return SetScene(sceneIndex);

	return true;
}
bool Game::ActiveSceneIsValid()
{
	if (_activeSceneIndex < _scenes.size())
	{
		if (_scenes[_activeSceneIndex] == nullptr)
		{
			return false;
		}

		if (!_scenes[_activeSceneIndex]->IsInitialized())
		{
			return false;
		}
	}

	return true;
}

bool Game::SetSceneInternal(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
	{
		DbgMsgF("Scene '{}' not found!", sceneName);
		return false;
	}

	if (sceneIndex == _activeSceneIndex)
		return true;

	Scene *prevScene = _activeSceneIndex == -1 ? nullptr : _scenes[_activeSceneIndex].get();
	Scene *scene = _scenes[sceneIndex].get();

	if (!scene)
		return false;

	_activeSceneIndex = sceneIndex;

	if (prevScene)
		prevScene->ExitScene();

	if (!scene->IsInitialized())
	{
		if (!scene->InitializeBase(sceneName, _device.Get(), _immediateContext.Get(), this, &_content, &_graphics, _gameVolume))
		{
			ErrMsg("Failed to initialize scene!");
			return false;
		}
	}

	if (!_systemManager.OnSceneChange(prevScene, scene))
	{
		ErrMsg("Failed to register scene change in systems!");
		return false;
	}

	scene->EnterScene();
	scene->SetSceneVolume(_gameVolume);

#ifdef DEBUG_BUILD
	if (!scene->IsTransitionScene())
		DebugData::Get().activeScene = sceneName;
#endif

	return true;
}
bool Game::SetScene(const UINT sceneIndex)
{
	if (sceneIndex >= _scenes.size())
	{
		DbgMsgF("Scene index '{}' is out of range!", sceneIndex);
		return false;
	}

	Scene *scene = _scenes[sceneIndex].get();
	if (!scene)
	{
		DbgMsgF("Scene at index '{}' is null!", sceneIndex);
		return false;
	}

	_pendingSceneChange = scene->GetName();
	return true;
}
bool Game::SetScene(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
	{
		DbgMsgF("Scene '{}' not found!", sceneName);
		return false;
	}

	_pendingSceneChange = sceneName;
	return true;
}

Scene *Game::GetScene(const UINT sceneIndex)
{
	return _scenes[sceneIndex].get();
}
Scene *Game::GetScene(const std::string &sceneName)
{
	UINT sceneIndex = GetSceneIndex(sceneName);
	if (sceneIndex == CONTENT_NULL)
		return nullptr;

	return _scenes[sceneIndex].get();
}
Scene *Game::GetSceneByUID(const size_t uid)
{
	for (const auto &scene : _scenes)
	{
		if (scene->GetUID() == uid)
			return scene.get();
	}
	return nullptr;
}

const std::vector<std::unique_ptr<Scene>> *Game::GetScenes() const noexcept
{
	return &_scenes;
}
UINT Game::GetSceneIndex(const std::string &sceneName) noexcept
{
	for (UINT i = 0; i < _scenes.size(); i++)
	{
		if (_scenes[i]->GetName() == sceneName)
		{
			return i;
		}
	}
	return CONTENT_NULL;
}
Scene *Game::GetActiveScene() const noexcept
{
	if (_activeSceneIndex == CONTENT_NULL)
		return nullptr;
	if (_activeSceneIndex >= _scenes.size())
		return nullptr;
	return _scenes[_activeSceneIndex].get();
}
UINT Game::GetActiveSceneIndex() const noexcept
{
	return _activeSceneIndex;
}
std::string_view Game::GetActiveSceneName() const noexcept
{
	Scene *activeScene = GetActiveScene();
	if (!activeScene)
		return "";

	return activeScene->GetName();
}

Graphics *Game::GetGraphics() noexcept
{
	return &_graphics;
}
JoltManager *Game::GetJoltManager() noexcept
{
	return &_joltManager;
}
Window &Game::GetWindow() noexcept
{
	return _window;
}

float Game::GetGameVolume() const noexcept
{
	return _gameVolume;
}
void Game::SetGameVolume(float volume)
{
	_gameVolume = volume;

	if (ActiveSceneIsValid())
		_scenes[_activeSceneIndex]->SetSceneVolume(_gameVolume);
}

bool Game::Update(TimeUtils &time, const Input& input)
{
	ZoneScopedC(RandomUniqueColor());

	for (const std::string &sceneName : _pendingSceneRemovals)
	{
		UINT sceneIndex = GetSceneIndex(sceneName);
		if (sceneIndex == CONTENT_NULL)
			continue;

		_scenes.erase(_scenes.begin() + sceneIndex);

		if (sceneIndex == _activeSceneIndex)
		{
			UINT newSceneIndex = sceneIndex;
			if (newSceneIndex >= _scenes.size())
				newSceneIndex = _scenes.size() - 1;

			const std::string &newSceneName = _scenes[newSceneIndex]->GetName();
			if (!SetSceneInternal(newSceneName))
				return false;
		}
		else if (sceneIndex < _activeSceneIndex)
		{
			_activeSceneIndex--;
		}
	}
	_pendingSceneRemovals.clear();

	if (!_pendingSceneChange.empty())
	{
		if (!SetSceneInternal(_pendingSceneChange))
			DbgMsgF("Failed to change scene to '{}'!", _pendingSceneChange);

		_pendingSceneChange.clear();
	}

	// Update systems
	{
		if (!_systemManager.Update(time, input))
		{
			ErrMsg("Failed to update systems!");
			return false;
		}
	}

	// Update
	time.TakeSnapshot("SceneUpdateTime");
	if (ActiveSceneIsValid())
	{
		if (!_scenes[_activeSceneIndex]->Update(time, input))
		{
			ErrMsg("Failed to update scene!");
			return false;
		}
	}
	time.TakeSnapshot("SceneUpdateTime");

	float dTime = time.GetDeltaTime();
	float absDTime = abs(dTime);

	// Fixed update
	static bool firstFixedUpdate = true;
	float fixedDTime = time.GetFixedDeltaTime();
	_fixedTickTimer += absDTime;

	while (_fixedTickTimer >= fixedDTime)
	{
		time.TakeSnapshot("SceneFixedUpdateTime");

		_fixedTickTimer -= fixedDTime;
		if (firstFixedUpdate)
		{
			firstFixedUpdate = false;
			_fixedTickTimer = 0.0f; // Prevent a large fixed update on the first frame.
		}

		if (ActiveSceneIsValid())
		{
			if (!_scenes[_activeSceneIndex]->FixedUpdate(fixedDTime, input))
			{
				ErrMsg("Failed to update scene at fixed step!");
				return false;
			}
		}

#ifdef _DEBUG
		// Prevent too many simultaneous fixed updates in case of a long hitch.
		// Only for debug builds, as this is obviously a very faulty solution.
		if (_fixedTickTimer >= fixedDTime * 16.0f)
			_fixedTickTimer = fixedDTime * 16.0f;
#endif

		time.TakeSnapshot("SceneFixedUpdateTime");
	}

	// Physics update
	static bool firstPhysUpdate = true;
	float physDTime = time.GetPhysDeltaTime();
	_physTickTimer += absDTime;

	while (_physTickTimer >= physDTime)
	{
		time.TakeSnapshot("ScenePhysUpdateTime");

		_physTickTimer -= physDTime;
		if (firstPhysUpdate)
		{
			firstPhysUpdate = false;
			_physTickTimer = 0.0f; // Prevent a large physics update on the first frame.
		}

		if (ActiveSceneIsValid())
		{
			if (!_scenes[_activeSceneIndex]->PhysUpdate(physDTime))
			{
				ErrMsg("Failed to update scene at physics step!");
				return false;
			}
		}

		time.TakeSnapshot("ScenePhysUpdateTime");
	}

	// Late update
	time.TakeSnapshot("SceneLateUpdateTime");
	if (ActiveSceneIsValid())
	{
		if (!_scenes[_activeSceneIndex]->LateUpdate(time, input))
		{
			ErrMsg("Failed to late update scene!");
			return false;
		}
	}
	time.TakeSnapshot("SceneLateUpdateTime");

	return true;
}
void Game::UpdateWorker()
{
	tracy::SetThreadName("Worker Thread");

	while (true)
	{
		_workerSemaphore.acquire();

		if (!_workerState)
			return;
		
		// Update worker thread
		{
			ZoneScopedC(RandomUniqueColor());

			if (!_graphics.ResetRenderState())
			{
				_workerState = false;
				return;
			}

			if (ActiveSceneIsValid())
			{
				if (!_scenes[_activeSceneIndex]->UpdateCullingTree())
				{
					_workerState = false;
					return;
				}
			}
		}

		_mainSemaphore.release();

		if (!_workerState)
			return;
	}
}

bool Game::Render(TimeUtils &time, const Input& input)
{
	ZoneScopedC(RandomUniqueColor());

	if (!_graphics.BeginSceneRender())
	{
		ErrMsg("Failed to begin rendering!");
		return false;
	}

	time.TakeSnapshot("SceneRenderTime");
	if (ActiveSceneIsValid())
		if (!_scenes[_activeSceneIndex]->Render(time, input))
		{
			ErrMsg("Failed to render scene!");
			return false;
		}
	time.TakeSnapshot("SceneRenderTime");

	if (!_graphics.EndSceneRender(time))
	{
		ErrMsg("Failed to end rendering!");
		return false;
	}
	
#ifdef USE_IMGUI
	if (_pendingLayoutChange != "")
	{
		UILayout::LoadLayout(_pendingLayoutChange);
		_pendingLayoutChange = "";
	}

	if (!_graphics.BeginUIRender())
	{
		ErrMsg("Failed to begin UI rendering!");
		return false;
	}

	if (!RenderUI(time))
	{
		ErrMsg("Failed to render UI!");
		return false;
	}

	if (!_graphics.EndUIRender())
	{
		ErrMsg("Failed to end UI rendering!");
		return false;
	}
#endif

	if (!_graphics.ScreenSpaceRender())
	{
		ErrMsg("Failed to render screen-space!");
		return false;
	}

	// While the main thread is waiting for the GPU to finish, let worker thread work.
	_workerSemaphore.release();

	if (!_graphics.EndFrame())
	{
		ErrMsg("Failed to end frame!");
		return false;
	}

	// Wait for the worker thread to finish.
	_mainSemaphore.acquire();

	// Check result of worker thread.
	if (!_workerState)
	{
		ErrMsg("Worker thread failed!");
		return false;
	}

	return true;
}

#ifdef USE_IMGUI
bool Game::RenderInspectorUI(TimeUtils &time)
{
	// TODO: 
	//   Show selected content details here.
	//   Global selection will need to be handelled in Game class.

	if (ActiveSceneIsValid())
	{
		Scene *scene = _scenes[_activeSceneIndex].get();

		if (!scene->RenderSelectionUI())
		{
			ErrMsg("Failed to render scene UI!");
			return false;
		}
	}

	return true;
}

bool Game::RenderUI(TimeUtils &time)
{
	ZoneScopedC(RandomUniqueColor());

	Input &input = Input::Instance();
	DebugData &debugData = DebugData::Get();

	int mainDockID = ImGui::GetID("Main");
	ImGuiWindowFlags_ defaultWindowFlags = (ImGuiWindowFlags_)((ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs) & ~ImGuiWindowFlags_NoCollapse);
	ImGuiWindowFlags viewWindowFlags = defaultWindowFlags | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar;
	float &imGuiFontScale = debugData.imGuiFontScale;
	int stylesPushed = 0;

	bool drawImGuizmo = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File##MainMenu"))
		{
			ImGui::SeparatorText("Scene");
			{
				constexpr ImGuiWindowFlags popupFlags =
					ImGuiWindowFlags_AlwaysAutoResize |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoDocking;

				if (ImGui::BeginMenu("Scenes##SceneListMenu"))
				{
					UINT newSceneID = _activeSceneIndex;
					bool hasSelected = false;

					float longestNameWidth = 32.0f;
					for (int i = 0; i < _scenes.size(); i++)
					{
						const auto& scene = _scenes[i];

						if (!scene)
							continue;

						float nameWidth = ImGui::CalcTextSize(scene->GetName().c_str()).x;
						if (nameWidth > longestNameWidth)
						{
							longestNameWidth = nameWidth;
						}
					}

					ImGui::PushID("SceneList");
					for (int i = 0; i < _scenes.size(); i++)
					{
						const auto& scene = _scenes[i];
						if (!scene)
							continue;

						const std::string& sceneName = scene->GetName();

						ImGui::PushID(sceneName.c_str());

						const bool isSelected = (_activeSceneIndex == i);
						if (ImGui::Button(sceneName.c_str(), { longestNameWidth + 8.0f, 0.0f }))
						{
							newSceneID = i;
							hasSelected = true;
						}

						if (_scenes.size() > 1)
						{
							ImGui::SameLine();
							ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
							if (ImGui::Button(std::format("X", sceneName, i).c_str()))
							{
								ImGui::OpenPopup("ConfirmDeleteScene");
							}
							ImGuiUtils::EndButtonStyle();

							if (ImGui::BeginPopup("ConfirmDeleteScene", popupFlags))
							{
								ImGui::Text("Are you sure you want to delete scene '%s'?", sceneName.c_str());

								if (ImGui::Button("Delete Scene"))
								{
									std::string sceneFilePath = WE_DFE(WE_D_DATA_SCENE, sceneName, WE_E_DATA_SCENE);
									std::filesystem::remove(sceneFilePath);
									_pendingSceneRemovals.push_back(sceneName);
									ImGui::CloseCurrentPopup();
								}

								ImGui::SameLine();
								if (ImGui::Button("Cancel"))
								{
									ImGui::CloseCurrentPopup();
								}

								ImGui::EndPopup();
							}
						}

						ImGui::PopID();
					}
					ImGui::PopID();

					if (hasSelected)
					{
						if (!SetScene(newSceneID))
						{
							ErrMsg("Failed to set scene!");
							ImGui::EndMenu();
							ImGui::EndMenu();
							ImGui::EndMenuBar();
							return false;
						}
					}

					ImGui::EndMenu();
				}

				if (ImGui::Button("New Scene"))
				{
					ImGui::OpenPopup("NewScenePopup");
				}

				if (ImGui::BeginPopup("NewScenePopup", popupFlags))
				{
					static std::string newSceneName = "New Scene";
					static dx::BoundingBox newSceneBounds = { {0, 0, 0}, {500, 250, 500} };
					static bool transitional = false;

					ImGui::Text("Create New Scene");

					ImGui::Text("Name:"); ImGui::SameLine();
					ImGui::InputText("##NewSceneName", &newSceneName);

					ImGui::Text("Center:"); ImGui::SameLine();
					ImGui::DragFloat3("##NewSceneCenter", &newSceneBounds.Center.x);

					ImGui::Text("Extents:"); ImGui::SameLine();
					if (ImGui::DragFloat3("##NewSceneExtents", &newSceneBounds.Extents.x))
					{
						newSceneBounds.Extents.x = MAX(0.1f, newSceneBounds.Extents.x);
						newSceneBounds.Extents.y = MAX(0.1f, newSceneBounds.Extents.y);
						newSceneBounds.Extents.z = MAX(0.1f, newSceneBounds.Extents.z);
					}

					ImGui::Text("Transitional:"); ImGui::SameLine();
					ImGui::Checkbox("##NewSceneTransitional", &transitional);

					bool isValid = true;
					if (newSceneName.empty())
					{
						isValid = false;
						ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Input a name!");
					}
					else
					{
						for (const auto& scene : _scenes)
						{
							if (scene && scene->GetName() == newSceneName)
							{
								isValid = false;
								ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.0f, 1.0f), "Name taken!");
								break;
							}
						}
					}

					ImGui::BeginDisabled(!isValid);
					if (ImGui::Button("Create"))
					{
						Scene* newScene = new Scene(newSceneName, transitional);
						if (!AddScene(&newScene, true))
						{
							delete newScene;
							ErrMsg("Failed to create new scene!");
						}

						newSceneName = "New Scene";
						newSceneBounds = { {0, 0, 0}, {500, 250, 500} };
						transitional = false;

						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::EndPopup();
				}
			}


			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Edit##MainMenu"))
		{
			ImGui::SeparatorText("Transform");
			{
				static const std::string transformTypes[6] = { "None", "Translate", "Rotate", "Scale", "Universal", "Bounds" };
				int& transformType = debugData.transformType;

				if (ImGui::BeginMenu(std::format("Tool: {}", transformTypes[transformType]).c_str()))
				{
					for (int i = 0; i < 6; i++)
					{
						if (ImGui::MenuItem(std::format("{}##SelectTransformType{}", transformTypes[i], i).c_str(), NULL, transformType == i))
							transformType = i;
					}

					ImGui::EndMenu();
				}

				static const std::string transformOrigins[5] = { "None", "Primary", "Center", "Average", "Separate " };
				int& transformOrigin = debugData.transformOriginMode;

				if (ImGui::BeginMenu(std::format("Origin: {}", transformOrigins[transformOrigin]).c_str()))
				{
					for (int i = 0; i < 4; i++)
					{
						if (ImGui::MenuItem(std::format("{}##SelectTransformOrigin{}", transformOrigins[i], i).c_str(), NULL, transformOrigin == i))
							transformOrigin = i;
					}

					ImGui::BeginDisabled(true);
					if (ImGui::MenuItem("Separate##SelectTransformOrigin4", NULL, transformOrigin == 4))
						transformOrigin = 4;
					ImGui::EndDisabled();

					ImGui::EndMenu();
				}

				int& transformSpace = debugData.transformSpace;
				if (ImGui::Button(transformSpace == (int)Local ? "Space: Local" : "Space: World"))
					transformSpace = transformSpace == (int)Local ? (int)World : (int)Local;

				bool& transformRelative = debugData.transformRelative;
				if (ImGui::Button(transformRelative ? "Relative" : "Absolute"))
					transformRelative = !transformRelative;

				ImGui::Separator();

				ImGui::SliderFloat("Gizmo Scale", &debugData.transformScale, 0.0f, 1.0f);

				ImGui::DragFloat("Snap Size", &debugData.transformSnap, 0.01f, FLT_MIN);
				ImGuiUtils::LockMouseOnActive();
			}

			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Window##MainMenu"))
		{
			ImGui::SeparatorText("Layout");
			{
				static std::string selectedLoadout = "";
				static std::string styleName = "";

				if (ImGui::Button("Load Layout"))
				{
					ImGui::OpenPopup("Load Layout");
					selectedLoadout = DebugData::Get().layoutName;
				}
				
				if (ImGui::Button("Save Layout"))
				{
					ImGui::OpenPopup("Save Layout");
					styleName = DebugData::Get().layoutName;
				}

				if (ImGui::BeginPopup("Load Layout"))
				{
					std::vector<std::string> layouts;
					UILayout::GetLayoutNames(layouts);

					if (ImGui::BeginCombo("Layouts", selectedLoadout.c_str()))
					{
						for (int i = 0; i < layouts.size(); i++)
						{
							const bool isSelected = layouts[i] == selectedLoadout;
							if (ImGui::Selectable(layouts[i].c_str(), isSelected))
								selectedLoadout = layouts[i];

							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (ImGui::Button("Confirm"))
					{
						_pendingLayoutChange = selectedLoadout;
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				if (ImGui::BeginPopup("Save Layout"))
				{
					ImGui::InputText("Layout Name", &styleName);

					if (ImGui::Button("Save"))
					{
						UILayout::SaveLayout(styleName);
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	ImGui::GetStyle().FontScaleMain = imGuiFontScale;
	float defWindowPadding = ImGui::GetStyle().WindowPadding.y;

	// Set input to be absorbed by default, it may be set back to false from the scene view window.
	input.SetKeyboardAbsorbed(true);
	input.SetMouseAbsorbed(true);

	stylesPushed = 0;
	stylesPushed++; ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	stylesPushed++; ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

	if (ImGui::Begin("View", nullptr, viewWindowFlags))
	{
		ImGui::PopStyleVar(stylesPushed);

		if (ImGui::BeginMenuBar())
		{
			ImVec2 menuBarScreenPos = ImGui::GetCursorScreenPos();
			ImVec2 menuBarRegionMin = ImGui::GetCursorPos();
			float menuBarRegionAvailX = ImGui::GetContentRegionAvail().x;

			if (ImGui::BeginMenu("Settings##ViewViewMenu"))
			{
				bool sceneValid = ActiveSceneIsValid();

				ImGui::SeparatorText("Control");

				if (sceneValid)
				{
					Scene *scene = _scenes[_activeSceneIndex].get();
					SceneHolder *sceneHolder = scene->GetSceneHolder();
					B_Camera *viewCam = scene->GetMainCamera();

					if (ImGui::Button("Select View Camera"))
						scene->SetSelection(viewCam->GetEntity());

					if (ImGui::BeginMenu("Camera View"))
					{
						SceneContents::SceneIterator entIter = sceneHolder->GetEntities();

						while (Entity *ent = entIter.Step())
						{
							B_Camera *cam = nullptr;
							if (!ent->GetBehaviourByType<B_Camera>(cam))
								continue;

							if (ImGui::MenuItem(std::format("{}##SelectViewCamID{}", ent->GetName(), ent->GetID()).c_str(), NULL, cam == viewCam))
								scene->SetMainCamera(cam);
						}

						ImGui::EndMenu();
					}
				}

				if (ImGui::BeginMenu("Mouse Movement Mode"))
				{
					int &mode = debugData.mouseMovementMode;

					if (ImGui::MenuItem("None", NULL, mode == 0))
						mode = 0;

					if (ImGui::MenuItem("Orbit / Pan", NULL, mode == 1))
						mode = 1;

					if (ImGui::MenuItem("Fly Camera", NULL, mode == 2))
						mode = 2;

					ImGui::EndMenu();
				}
				
				float sensitivity = input.GetMouseSensitivity();
				if (ImGui::SliderFloat("Sensitivity", &sensitivity, 0.0f, 2.0f))
					input.SetMouseSensitivity(sensitivity);
				
				ImGui::DragFloat("Movement Speed", &debugData.movementSpeed, 0.01f, 0.0001f);
				ImGuiUtils::LockMouseOnActive();

				if (Window *wnd = input.GetWindow())
				{
					ImGui::SeparatorText("Window");
					static dx::XMINT2 windowResolution = {
						debugData.windowSizeX,
						debugData.windowSizeY
					};

					if (ImGui::DragInt2("##WindowResolutionInput", &windowResolution.x, 0.33f))
					{
						windowResolution.x = MAX(1, windowResolution.x);
						windowResolution.y = MAX(1, windowResolution.y);
					}
					ImGuiUtils::LockMouseOnActive();

					if (ImGui::Button("Apply##WindowResolutionApply"))
					{
						debugData.windowSizeX = windowResolution.x;
						debugData.windowSizeY = windowResolution.y;
						wnd->SetWindowSize(windowResolution);
					}

					ImGui::SameLine();
					if (ImGui::Button("Copy Display##WindowResolutionCopyDisplay"))
					{
						auto screenSize = input.GetScreenSize();
						windowResolution.x = (int)screenSize.x;
						windowResolution.y = (int)screenSize.y;
					}

					bool isFullscreen = wnd->IsFullscreen();
					if (ImGui::MenuItem("Fullscreen", NULL, &isFullscreen))
						wnd->SetFullscreen(isFullscreen);
				}

				ImGui::SeparatorText("Scene");
				static dx::XMINT2 sceneResolution = { 
					debugData.sceneViewSizeX, 
					debugData.sceneViewSizeY 
				};

				if (ImGui::DragInt2("##SceneResolutionInput", &sceneResolution.x, 0.33f))
				{
					sceneResolution.x = MAX(1, sceneResolution.x);
					sceneResolution.y = MAX(1, sceneResolution.y);
				}
				ImGuiUtils::LockMouseOnActive();

				if (ImGui::Button("Apply##SceneResolutionApply"))
				{
					debugData.sceneViewSizeX = sceneResolution.x;
					debugData.sceneViewSizeY = sceneResolution.y;
				}

				ImGui::SameLine();
				if (ImGui::Button("Copy Display##SceneResolutionCopyDisplay"))
				{
					auto screenSize = input.GetScreenSize();
					sceneResolution.x = (int)screenSize.x;
					sceneResolution.y = (int)screenSize.y;
				}

				ImGui::MenuItem("Stretch to Fit", NULL, &debugData.stretchToFitView);

				if (ImGui::MenuItem("Point Filtering", NULL, &debugData.graphicsScenePointFiltering))
					_graphics.SetScenePointFiltering(debugData.graphicsScenePointFiltering);

				ImGui::SeparatorText("Gizmos");

				ImGui::MenuItem("View Manipulator", NULL, &debugData.showViewManipGizmo);

				if (sceneValid)
				{
					Scene *scene = _scenes[_activeSceneIndex].get();
					bool doUpdate = false;
					
					if (ImGui::MenuItem("Draw Icons", NULL, &debugData.billboardGizmosDraw))
						doUpdate = true;

					if (debugData.billboardGizmosDraw)
					{
						if (ImGui::MenuItem("Overlay Icons", NULL, &debugData.billboardGizmosOverlay))
							doUpdate = true;

						if (ImGui::DragFloat("Icon Size", &debugData.billboardGizmosSize, 0.001f, 0.05f))
							doUpdate = true;
						ImGuiUtils::LockMouseOnActive();
					}

					if (doUpdate)
						scene->UpdateBillboardGizmos();
				}

				ImGui::EndMenu();
			}
			
			// Scene name
			if (ActiveSceneIsValid())
			{
				Scene *scene = _scenes[_activeSceneIndex].get();
				std::string_view title = scene->GetName();

				ImVec2 textSize = ImGui::CalcTextSize(title.data());
				ImVec2 namePos = menuBarRegionMin + ImVec2((menuBarRegionAvailX - textSize.x) * 0.5f, -1.0f);

				float frameHeight = ImGui::GetFrameHeight();

				ImVec2 borderSize = textSize + ImVec2(10, 0);
				ImVec2 borderPos = menuBarScreenPos + (ImVec2(menuBarRegionAvailX, frameHeight) - borderSize) * 0.5f;

				bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
				ImColor borderRimColor = ImGui::GetStyleColorVec4(isFocused ? ImGuiCol_TabSelected : ImGuiCol_TabDimmedSelected);
				ImColor borderColor = ImGui::GetStyleColorVec4(isFocused ? ImGuiCol_TitleBgActive : ImGuiCol_TitleBg);

				ImGui::GetWindowDrawList()->AddRectFilled(borderPos - ImVec2(1, frameHeight), borderPos + borderSize + ImVec2(1, 1.5f), borderRimColor, 6.0f);
				ImGui::GetWindowDrawList()->AddRectFilled(borderPos - ImVec2(0, frameHeight), borderPos + borderSize, borderColor, 5.0f);

				ImGui::SetCursorPos(namePos);
				ImGui::Text(title.data());
			}

			ImGui::EndMenuBar();
		}

		if (!_graphics.RenderSceneView())
		{
			ErrMsg("Failed to render scene view!");
			return false;
		}

		drawImGuizmo = true;
	}
	else
		ImGui::PopStyleVar(stylesPushed);
	ImGui::End();

	if (ImGui::Begin("General", nullptr, defaultWindowFlags))
	{
#if defined(_DEBUG) && defined(DEBUG_D3D11_DEVICE)
		ImGui::Checkbox("[D3D11] Report Live Device Objects on Shutdown", &DebugData::Get().reportComObjectsOnShutdown);
#endif

		static bool showStyleEditor = false;
		if (ImGui::Button(showStyleEditor ? "Hide Style Editor" : "Show Style Editor"))
			showStyleEditor = !showStyleEditor;

		if (showStyleEditor)
		{
			ImGui::BeginChild("StyleEditorChild", ImGui::GetContentRegionAvail(), true | ImGuiChildFlags_ResizeY);

			// Layout load/save
			{
				static std::string selectedLoadout = "";
				static std::string styleName = "";

				if (ImGui::Button("Load Window Layout"))
				{
					ImGui::OpenPopup("Load Layout");
					selectedLoadout = DebugData::Get().layoutName;
				}
				ImGui::SameLine();
				if (ImGui::Button("Save Window Layout"))
				{
					ImGui::OpenPopup("Save Layout");
					styleName = DebugData::Get().layoutName;
				}

				if (ImGui::BeginPopup("Load Layout"))
				{
					std::vector<std::string> layouts;
					UILayout::GetLayoutNames(layouts);

					if (ImGui::BeginCombo("Layouts", selectedLoadout.c_str()))
					{
						for (int i = 0; i < layouts.size(); i++)
						{
							const bool isSelected = layouts[i] == selectedLoadout;
							if (ImGui::Selectable(layouts[i].c_str(), isSelected))
								selectedLoadout = layouts[i];

							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					if (ImGui::Button("Confirm"))
					{
						_pendingLayoutChange = selectedLoadout;
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				if (ImGui::BeginPopup("Save Layout"))
				{
					ImGui::InputText("Layout Name", &styleName);

					if (ImGui::Button("Save"))
					{
						UILayout::SaveLayout(styleName);
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
			}
			ImGui::Separator();

			ImGui::ShowStyleEditor();
			ImGui::EndChild();
		}
		ImGui::Dummy({ 0, 2 });

		if (ImGui::TreeNode("Fonts"))
		{
			auto fonts = ImGuiUtils::Utils::GetFonts();
			static std::string inputStr = "";

			for (const auto &[name, font] : fonts)
			{
				ImGui::PushFont(font, 0.0f);

				ImGui::Text("[%s]", name.c_str());
				ImGui::TextWrapped("The quick brown fox jumps over the lazy dog .,!? +-/* 123 456 789");
				ImGui::InputText(std::format("##FontTestInput {}", name).c_str(), &inputStr);

				ImGui::PopFont();
				ImGui::Dummy({ 0, 4 });
			}
			ImGui::TreePop();
		}
		ImGui::Dummy({ 0, 2 });

		if (ImGui::DragFloat("ImGui Font Scale", &imGuiFontScale, 0.01f))
			imGuiFontScale = MAX(0.25f, imGuiFontScale);
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::Button("Reset Font Scale"))
			imGuiFontScale = 1.0f;
		ImGui::Dummy({ 0, 4 });


		if (ImGui::DragFloat("Volume", &_gameVolume, 0.01f))
		{
			_gameVolume = MAX(0, _gameVolume);
			_scenes[_activeSceneIndex]->SetSceneVolume(_gameVolume);
		}
		ImGuiUtils::LockMouseOnActive(); 
		ImGui::Dummy({ 0, 4 });


		float timeScale = time.GetTimeScale();
		if (ImGui::DragFloat("Time Scale", &timeScale, 0.005f, 0, 0, "%.3f"))
			time.SetTimeScale(MAX(timeScale, 0.0f));
		ImGuiUtils::LockMouseOnActive();

		float fixedDeltaTime = time.GetFixedDeltaTime();
		if (ImGui::DragFloat("Fixed Time Step", &fixedDeltaTime, 0.001f, 0, 0, "%.3f"))
			time.SetFixedDeltaTime(fixedDeltaTime);
		ImGuiUtils::LockMouseOnActive();

		float physDeltaTime = time.GetPhysDeltaTime();
		if (ImGui::DragFloat("Phys Time Step", &physDeltaTime, 0.001f, 0, 0, "%.3f"))
		{
			physDeltaTime = MAX(0.001f, physDeltaTime);
			time.SetPhysDeltaTime(physDeltaTime);
		}
		ImGuiUtils::LockMouseOnActive();
		ImGui::Dummy({ 0, 4 });


		if (ImGui::TreeNode("Systems"))
		{
			if (!_systemManager.RenderUI())
			{
				ErrMsg("Failed to render system manager UI!");
				ImGui::TreePop();
				return false;
			}

			ImGui::Separator();
			ImGui::TreePop();
		}
		ImGui::Dummy({ 0, 4 });

		if (ImGui::TreeNode("Utility"))
		{
#ifdef _WIN32
#ifdef TRACY_ENABLE
			if (ImGui::Button("Launch Tracy Profiler"))
			{
				::ShellExecuteA(NULL, "open",
					TO_SOLUTION_PATH "WellEngine\\Dependencies\\tracy-0.11.1\\Tracy\\tracy-profiler.exe",
					NULL, NULL, SW_SHOWDEFAULT
				);
			}
			ImGui::Dummy({ 0, 4 });
#endif

			if (ImGui::Button("Open ImGui Manual"))
			{
				::ShellExecuteA(NULL, "open",
					"https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html",
					NULL, NULL, SW_SHOWDEFAULT
				);
			}
			ImGui::Dummy({ 0, 4 });
#endif
			ImGui::TreePop();
		}
		ImGui::Dummy({ 0, 4 });

		if (ImGui::TreeNode("Version Info"))
		{
			ImGui::Text("Engine Version: %s", ENGINE_VERSION);
			ImGui::Text("Build Date: %s", __DATE__);
			ImGui::Dummy({ 0, 2 });

			ImGui::Text("SDL Version: %d", SDL_GetVersion());
			ImGui::Text("ImGui Version: %s, %d", IMGUI_VERSION, IMGUI_VERSION_NUM);
			ImGui::Dummy({ 0, 4 });

			if (ImGui::TreeNode("Compilation Flags"))
			{
#ifdef _DEBUG
				ImGui::Text("Debug Mode");
#else
				ImGui::Text("Release Mode");
#endif
				ImGui::Dummy({ 0, 3 });

#ifdef PARALLEL_UPDATE
				ImGui::Text("Parallel Update");
				ImGui::Text(std::format("Thread Count: {}", PARALLEL_THREADS).c_str());
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef FORCE_COMPILE_CONTENT
				ImGui::Text("Compile Content");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef TRACY_ENABLE
				ImGui::Text("Tracy Profiler");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef TRACY_MEMORY
				ImGui::Text("Tracy Memory Tracking");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_BUILD
				ImGui::Text("Debug Build");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_MESSAGES
				ImGui::Text("Debug Messages");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef USE_IMGUIZMO
				ImGui::Text("ImGuizmo");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef EDIT_MODE
				ImGui::Text("Edit Mode");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DEBUG_DRAW
				ImGui::Text("Debug Draw");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef DISABLE_MONSTER
				ImGui::Text("Disable Monster");
				ImGui::Dummy({ 0, 3 });
#endif

#ifdef LEAK_DETECTION
				ImGui::Text("Leak Detection");
				ImGui::Dummy({ 0, 3 });
#endif
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (ImGui::Begin("Performance", nullptr, defaultWindowFlags))
	{
		if (ImGui::TreeNode("FPS"))
		{
			constexpr size_t FPS_BUF_SIZE = 256;
			static size_t usedBufSize = FPS_BUF_SIZE;

			static float fpsBuf[FPS_BUF_SIZE]{};
			static size_t fpsBufIndex = 0;

			float currFps = time.GetTimeScale() / time.GetDeltaTime();
			float dTime = 1.0f / currFps;
			fpsBuf[fpsBufIndex] = currFps;

			float avgFps = 0.0f;
			float dropFps = FLT_MAX;
			for (size_t i = 0; i < usedBufSize; i++)
			{
				avgFps += fpsBuf[i];

				if (dropFps > fpsBuf[i])
					dropFps = fpsBuf[i];
			}
			avgFps /= usedBufSize;

			static float minFPS = FLT_MAX;
			if (minFPS > currFps)
				minFPS = currFps;

			static UINT rebaseBufferSizeTimer = 0;
			if (++rebaseBufferSizeTimer >= 30)
			{
				size_t prevSize = usedBufSize;

				rebaseBufferSizeTimer = 0;
				if (avgFps > 240.f && usedBufSize != FPS_BUF_SIZE)
					usedBufSize = FPS_BUF_SIZE;
				else if (avgFps > 90.f && usedBufSize != (FPS_BUF_SIZE / 2))
					usedBufSize = FPS_BUF_SIZE / 2;
				else if (avgFps > 30.f && usedBufSize != (FPS_BUF_SIZE / 4))
					usedBufSize = FPS_BUF_SIZE / 4;
				else if (avgFps > 10.f && usedBufSize != (FPS_BUF_SIZE / 8))
					usedBufSize = FPS_BUF_SIZE / 8;

				if (prevSize != usedBufSize)
				{
					for (size_t i = 0; i < FPS_BUF_SIZE; i++)
						fpsBuf[i] = avgFps;
				}
			}

			// plot
			{
				ImGui::BeginChild("FPS Plot", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);

				static float history = 1.0f;
				ImGui::SliderFloat("History", &history, 0.1f, 15.0f, "%.1f s");

				static float currTimeCoverage = 0.0f;
				static float t = 0.0f;

				static std::vector<ImVec2> plotFpsData;
				if (plotFpsData.empty())
					plotFpsData.reserve(FPS_BUF_SIZE);

				float yData = currFps;

				while (currTimeCoverage > history + 0.1f)
				{
					currTimeCoverage -= 1.0f / plotFpsData[0].y;
					plotFpsData.erase(plotFpsData.begin());
				}
				plotFpsData.emplace_back(t, yData);
				currTimeCoverage += dTime;
				t += dTime;

				float minValue = currFps;
				float maxValue = currFps;
				float avgValue = 0.0f;
				for (int i = 0; i < plotFpsData.size(); i++)
				{
					minValue = MIN(minValue, plotFpsData[i].y);
					maxValue = MAX(maxValue, plotFpsData[i].y);
					avgValue += plotFpsData[i].y;
				}
				avgValue /= plotFpsData.size();

				ImVec4 minVals = { t - history, minValue, t, minValue };
				ImVec4 maxVals = { t - history, maxValue, t, maxValue };
				ImVec4 avgVals = { t - history, avgValue, t, avgValue };

				ImPlotFlags plotFlags = ImPlotFlags_None;
				plotFlags |= ImPlotFlags_NoTitle;
				//plotFlags |= ImPlotFlags_NoLegend;
				plotFlags |= ImPlotFlags_NoMouseText;
				plotFlags |= ImPlotFlags_NoBoxSelect;

				ImPlotAxisFlags xFlags = ImPlotAxisFlags_None;
				xFlags |= ImPlotAxisFlags_NoDecorations;

				ImPlotAxisFlags yFlags = ImPlotAxisFlags_None;
				yFlags |= ImPlotAxisFlags_AutoFit;

				ImVec2 availRegion = ImGui::GetContentRegionAvail();

				if (ImPlot::BeginPlot("##Scrolling", availRegion, plotFlags))
				{
					const ImVec2 minMaxFPS = { 0.0f, 165.0f };

					ImPlot::SetupLegend(ImPlotLocation_West);
					ImPlot::SetupAxes(nullptr, nullptr, xFlags, yFlags);
					ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
					ImPlot::SetupAxisLimits(ImAxis_Y1, minMaxFPS.x, minMaxFPS.y);

					ImPlot::PlotLine("Min", &minVals.x, &minVals.y, 2, 0, 0, 2 * sizeof(float));
					ImPlot::PlotLine("Max", &maxVals.x, &maxVals.y, 2, 0, 0, 2 * sizeof(float));
					ImPlot::PlotLine("Avg", &avgVals.x, &avgVals.y, 2, 0, 0, 2 * sizeof(float));

					ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);
					ImPlot::PlotShaded("FPS", &plotFpsData[0].x, &plotFpsData[0].y, plotFpsData.size(), -INFINITY, 0, 0, 2 * sizeof(float));

					ImPlot::EndPlot();
				}
				ImGui::EndChild();
			}

			(++fpsBufIndex) %= usedBufSize;

			char fps[8]{};
			snprintf(fps, sizeof(fps), "%.2f", avgFps);
			ImGui::Text(std::format("Avg: {}", fps).c_str());

			snprintf(fps, sizeof(fps), "%.2f", dropFps);
			ImGui::Text(std::format("Drop: {}", fps).c_str());

			snprintf(fps, sizeof(fps), "%.2f", minFPS);
			ImGui::Text(std::format("Min: {}", fps).c_str());

			static bool countLongAvg = false;
			static bool hasLongAvg = false;
			static float longAvgAccumulation = 0.0f;
			static int longAvgCount = 0;

			ImGui::Text("Long Exposure Avg: ");
			ImGui::SameLine();
			if (!countLongAvg)
			{
				if (ImGui::SmallButton("Start"))
				{
					countLongAvg = true;

					hasLongAvg = true;
					longAvgAccumulation = currFps;
					longAvgCount = 1;
				}
			}
			else
			{
				if (ImGui::SmallButton("Stop"))
					countLongAvg = false;

				longAvgAccumulation += currFps;
				longAvgCount++;
			}

			if (hasLongAvg)
			{
				ImGui::SameLine();
				ImGui::Text(std::format("Iter: {}", longAvgCount).c_str());

				if (longAvgCount > 0)
					ImGui::Text(std::format("Result: {}", (longAvgAccumulation / (float)longAvgCount)).c_str());
				else
					ImGui::Text("Result: NaN");
			}

			if (ImGui::Button("Reset"))
			{
				minFPS = currFps;

				for (size_t i = 0; i < FPS_BUF_SIZE; i++)
					fpsBuf[i] = 0.0f;

				countLongAvg = false;
				hasLongAvg = false;
				longAvgAccumulation = 0.0f;
				longAvgCount = 0;
			}
			ImGui::TreePop();
		}

		ImGui::PushID("Frame Time");
		if (ImGui::TreeNode("Frame Time"))
		{
			char timeStr[32]{};

			snprintf(timeStr, sizeof(timeStr), "%.6f", time.GetDeltaTime());
			ImGui::Text(std::format("{} Frame", timeStr).c_str());

			ImGui::Spacing();

			if (ImGui::TreeNode("Scene"))
			{
				if (ImGui::TreeNode("Update"))
				{
					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneUpdateTime"));
					ImGui::Text(std::format("{} Scene Update", timeStr).c_str());

					static float fixedUpdateTime = -1.0f;
					time.TryCompareSnapshots("SceneFixedUpdateTime", &fixedUpdateTime);
					snprintf(timeStr, sizeof(timeStr), "%.6f", fixedUpdateTime);
					ImGui::Text(std::format("{} Scene Fixed Update", timeStr).c_str());

					static float physUpdateTime = -1.0f;
					time.TryCompareSnapshots("ScenePhysUpdateTime", &physUpdateTime);
					snprintf(timeStr, sizeof(timeStr), "%.6f", physUpdateTime);
					ImGui::Text(std::format("{} Scene Phys Update", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneLateUpdateTime"));
					ImGui::Text(std::format("{} Scene Late Update", timeStr).c_str());

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Render"))
				{
					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("SceneRenderTime"));
					ImGui::Text(std::format("{} Scene Render", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingTotal"));
					ImGui::Text(std::format("{} Culling Total", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingSetup"));
					ImGui::Text(std::format("{} Culling Setup", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingCameras"));
					ImGui::Text(std::format("{} Culling Cameras", timeStr).c_str());

					snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CullingCameraCubes"));
					ImGui::Text(std::format("{} Culling Camera Cubes", timeStr).c_str());

					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			ImGui::Spacing();

			if (ImGui::TreeNode("Graphics"))
			{
				ImGui::TreePop();
			}

			ImGui::Spacing();
			ImGui::TreePop();
		}
		ImGui::PopID();

		if (ImGui::TreeNode("Draws"))
		{
			if (ImGui::BeginTable("DrawDataTable", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame))
			{
				size_t mainDrawCalls = _graphics.GetMainDrawCallCount();
				size_t mainTriDraws = _graphics.GetMainTriDrawCount();
				size_t overlayDrawCalls = _graphics.GetOverlayDrawCallCount();
				size_t overlayTriDraws = _graphics.GetOverlayTriDrawCount();
				size_t transparentDrawCalls = _graphics.GetTransparentDrawCallCount();
				size_t transparentTriDraws = _graphics.GetTransparentTriDrawCount();
				size_t lightDrawCalls = _graphics.GetLightDrawCallCount();
				size_t lightTriDraws = _graphics.GetLightTriDrawCount();

				size_t viewDrawCalls = mainDrawCalls + overlayDrawCalls + transparentDrawCalls;
				size_t viewTriDraws = mainTriDraws + overlayTriDraws + transparentTriDraws;

				size_t totalDrawCalls = viewDrawCalls + lightDrawCalls;
				size_t totalTriDraws = viewTriDraws + lightTriDraws;

				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Draw Calls", ImGuiTableColumnFlags_WidthFixed, 150);
				ImGui::TableSetupColumn("Tri Draws", ImGuiTableColumnFlags_WidthFixed, 150);
				ImGui::TableHeadersRow();

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Main");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", mainDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", mainTriDraws);
				}

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Overlay");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", overlayDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", overlayTriDraws);
				}

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Transparent");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", transparentDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", transparentTriDraws);
				}

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Combined View");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", viewDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", viewTriDraws);
				}

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Lights");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", lightDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", lightTriDraws);
				}

				ImGui::TableNextRow();
				{
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("Total");

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%ld", totalDrawCalls);

					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%ld", totalTriDraws);
				}

				ImGui::EndTable();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Collisions"))
		{
			char timeStr[32]{};

			snprintf(timeStr, sizeof(timeStr), "%.6f", time.CompareSnapshots("CollisionChecks"));
			ImGui::Text(std::format("{} Collision Checks Total", timeStr).c_str());

			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (ImGui::Begin("Content", nullptr, defaultWindowFlags))
	{
		if (!_content.RenderUI(_device.Get()))
		{
			ErrMsg("Failed to render content UI!");
			return false;
		}
	}
	ImGui::End();

	if (ImGui::Begin("Graphics", nullptr, defaultWindowFlags))
	{
		if (!_graphics.RenderUI(time))
		{
			ErrMsg("Failed to render graphics UI!");
			return false;
		}
	}
	ImGui::End();

	if (ImGui::Begin("Physics", nullptr, defaultWindowFlags))
	{
		if (ActiveSceneIsValid())
		{
			Scene *scene = _scenes[_activeSceneIndex].get();

			if (!scene->GetPhysicsInstance()->RenderUI())
			{
				ErrMsg("Failed to render physics UI!");
				return false;
			}
		}
	}
	ImGui::End();

	stylesPushed = 0;
	stylesPushed++; ImGui::PushStyleVarY(ImGuiStyleVar_WindowPadding, 0);
	if (ImGui::Begin("Hierarchy", nullptr, defaultWindowFlags | ImGuiWindowFlags_MenuBar))
	{
		ImGui::PopStyleVar(stylesPushed);

		if (ActiveSceneIsValid())
		{
			Scene *scene = _scenes[_activeSceneIndex].get();

			if (ImGui::BeginMenuBar())
			{
				if (!scene->RenderHierarchyMenuBarUI())
				{
					ErrMsg("Failed to render scene context menu UI!");
					return false;
				}

				ImGui::EndMenuBar();
			}

			if (!scene->RenderHierarchyUI())
			{
				ErrMsg("Failed to render scene hierarchy UI!");
				return false;
			}
		}
	}
	else
		ImGui::PopStyleVar(stylesPushed);
	ImGui::End();

	if (static char focusedOnStartup = 0; focusedOnStartup <= 1)
	{
		if (focusedOnStartup == 1)
			ImGui::SetNextWindowFocus();
		focusedOnStartup++;
	}

	if (ImGui::Begin("Inspector", nullptr, defaultWindowFlags))
	{
		if (!RenderInspectorUI(time))
		{
			ErrMsg("Failed to render inspector UI!");
			return false;
		}
	}
	ImGui::End();

	if (ImGui::Begin("Scene", nullptr, defaultWindowFlags))
	{
		if (ActiveSceneIsValid())
		{
			Scene *scene = _scenes[_activeSceneIndex].get();

			if (!scene->RenderSceneUI())
			{
				ErrMsg("Failed to render scene UI!");
				return false;
			}
		}
	}
	ImGui::End();

	if (!ImGuiUtils::Utils::Render())
	{
		ErrMsg("Failed to render ImGuiUtils!");
		return false;
	}

	if (ImGui::Begin("Input", nullptr, defaultWindowFlags))
	{
		if (!input.RenderUI())
		{
			ErrMsg("Failed to render input UI!");
			return false;
		}
	}
	ImGui::End();

#ifdef USE_IMGUIZMO
	if (drawImGuizmo && ActiveSceneIsValid())
	{
		Scene *scene = _scenes[_activeSceneIndex].get();
		if (!scene->RenderGizmoUI())
		{
			ErrMsg("Failed to render scene gizmo UI!");
			return false;
		}
	}
#endif

	return true;
}
#endif

void Game::Exit()
{
	DbgMsg("Exiting game...");
	_isExiting = true;
}
bool Game::IsExiting() const noexcept
{
	return _isExiting;
}
