#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Scenes/Scene.h"
#include "Rendering/Graphics.h"
#include "Content/Content.h"
#include "Timing/TimeUtils.h"
#include "Input/Input.h"
#include "Window/Window.h"
#include "Debug/DebugDrawer.h"

/// Game handles loading content like textures and meshes, as well as managing the update and render steps of the main game loop.
class Game
{
private:
	struct TextureData		{ DXGI_FORMAT type;	std::string name;	std::string file;	bool mipmapped; int downsample;	};
	struct ShaderData		{ ShaderType type;	std::string name;	std::string file;									};
	struct HeightMapData	{					std::string name;	std::string file;									};

	ComPtr<ID3D11Device>		_device;
	ComPtr<ID3D11DeviceContext>	_immediateContext;
#ifdef DEFERRED_CONTEXTS
	// TODO: Implement deferred contexts in culling & rendering stages 
	std::array<ComPtr<ID3D11DeviceContext>, PARALLEL_THREADS> _deferredContexts;
#endif

	Graphics _graphics;
	Content _content;
	std::vector<std::unique_ptr<Scene>> _scenes;
	UINT _activeSceneIndex = -1;
	Window _window;
	std::string _pendingSceneChange = "";
	std::vector<std::string> _pendingSceneRemovals{};

	float _tickTimer = 0.0f;
	float _gameVolume = 10.0f;
	bool _isExiting = false;

	std::thread _workerThread;
	std::binary_semaphore _mainSemaphore{0};
	std::binary_semaphore _workerSemaphore{0};
	std::atomic<bool> _workerState{true};

#ifdef USE_IMGUI
	std::string _pendingLayoutChange = "";
#endif

	[[nodiscard]] bool CompileContent(const std::vector<std::string> &meshNames);
	[[nodiscard]] bool DecompileContent();

	[[nodiscard]] bool LoadContent(
		const std::vector<TextureData> &textureNames,
		const std::vector<TextureData> &cubemapNames,
		const std::vector<ShaderData> &shaderNames,
		const std::vector<HeightMapData> &heightMapNames
	);

	void UpdateWorker();

	[[nodiscard]] bool SetSceneInternal(const std::string &sceneName);

public:
	Game();
	~Game();

	[[nodiscard]] bool Setup(TimeUtils &time, Window window);

	/// Returns true if active scene exists and is initialized, Otherwise false.
	[[nodiscard]] bool ActiveSceneIsValid();

	/// Adds newScene to _scenes, newScene is set as active scene if setActive is true.
	[[nodiscard]] bool AddScene(Scene **newScene, const bool setActive = false);

	/// Sets _activeSceneIndex to sceneIndex and initializes the now active scene if uninitialized.
	[[nodiscard]] bool SetScene(const UINT sceneIndex);
	[[nodiscard]] bool SetScene(const std::string &sceneName);

	[[nodiscard]] Scene *GetScene(const UINT sceneIndex);
	[[nodiscard]] Scene *GetScene(const std::string &sceneName);
	[[nodiscard]] Scene *GetSceneByUID(const size_t uid);

	[[nodiscard]] UINT GetSceneIndex(const std::string &sceneName) noexcept;

	[[nodiscard]] UINT GetActiveSceneIndex() const noexcept;
	[[nodiscard]] std::string_view GetActiveSceneName() const noexcept;

	[[nodiscard]] const std::vector<std::unique_ptr<Scene>> *GetScenes() const noexcept;

	[[nodiscard]] float GetGameVolume() const noexcept;
	void SetGameVolume(float volume);

	[[nodiscard]] Graphics *GetGraphics() noexcept;

	[[nodiscard]] Window &GetWindow() noexcept;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input);
	[[nodiscard]] bool Render(TimeUtils &time, const Input &input);

#ifdef USE_IMGUI
	/// For an in-depth manual of ImGui features and their usages see:
	/// https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
	[[nodiscard]] bool RenderUI(TimeUtils &time);
#endif

	void Exit();
	[[nodiscard]] bool IsExiting() const noexcept;

	TESTABLE()
};
