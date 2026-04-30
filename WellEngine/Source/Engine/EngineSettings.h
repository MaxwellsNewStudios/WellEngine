#pragma once
/// NOTE: In this file I use '///' (doc comments) for comments and '//' for exclusion.
/// This is because they are generally displayed with slightly different colors, making it easier to parse visually (doc comments are slightly muted in color).
/// If they are both the same color for you, go to Tools -> Options -> Environment -> Fonts and Colors, find "XML Doc Comment" and change it's color.


#pragma region General
	#define GAME_TITLE "Game"
	#define ENGINE_VERSION "0.1.1.7"

	/// STARTUP_SCENE defines the starting scene outside of edit mode.
	#define STARTUP_SCENE "Main Menu"
#pragma endregion


#pragma region Content
	/// FORCE_COMPILE_CONTENT enables recompilation of the content file upon startup. Required after modifying or adding new meshes.
	/// Disabled by default as it slightly increases load time.
	//#define FORCE_COMPILE_CONTENT
	
	/// AUTO_RECOMPILE_CONTENT_ON_CHANGE enables automatic detection of changes in source content files and recompilation of the content file if any changes are detected.
	#define AUTO_RECOMPILE_CONTENT_ON_CHANGE

	/// FORCE_BAKE_TEXTURES causes textures to be baked into their post-processed form after loading.
	//#define FORCE_BAKE_TEXTURES

	/// USE_OWN_RESIZE_ALGORITHM enables the use of a custom resize algorithm for downsampling textures.
	/// Otherwise, use stb_image_resize2, which produces blurrier results.
	/// TODO: Remove references
	#define USE_OWN_RESIZE_ALGORITHM

	/// MIPS_DISCARDED discards a specified amount of higher resultion mipmaps for all textures.
	/// This is used to reduce memory usage and bandwidth.
	constexpr auto MIPS_DISCARDED = 0;
#pragma endregion


#pragma region Performance
	constexpr float FIXED_DELTA_TIME = 1.0f / 20.0f;
	constexpr float PHYS_DELTA_TIME = 1.0f / 60.0f; // Default only

	constexpr auto LOD_DIST_MIN_MULT = 10.0f; // The depth to start considering lower LODs, as multiple of near-plane.
	constexpr auto LOD_DIST_MAX_MULT = 0.9f; // The depth that the lowest LOD is picked, as multiple of far-plane.
	constexpr auto LOD_DIST_DIM_SCALE_FACTOR = 0.6f; // Curve LOD falloff based on mesh size. Lower value means quicker LOD falloff.

	/// PARALLEL_UPDATE enables the use of the ParallelUpdate method in entities.
	#define PARALLEL_UPDATE

	/// PARALLEL_THREADS sets the number of threads to use for parallel updates. This is only used if PARALLEL_UPDATE is enabled.
	constexpr auto PARALLEL_THREADS = 3;

	/// Makes meshes generate colliders using lower LODs when available, or skip it entirely.
	/// This option exists as it drastically increases load times with compiler optimizations turned off.
	/// 0: Use highest LOD.  1: Use middle LOD (default).  2: Use lowest LOD.  3: Raycast with bounding boxes only.
	#define MESH_COLLISION_DETAIL_REDUCTION 1

	/// MESH_COLLISION_MAX_TRIS sets the maximum amount of triangles a node can contain before it is split.
	#define MESH_COLLISION_MAX_TRIS 8

	/// MESH_COLLISION_MAX_DEPTH sets the maximum depth of the collision tree.
	#define MESH_COLLISION_MAX_DEPTH 6
#pragma endregion


#pragma region Profiling
	/// TRACY_ENABLE enables Tracy for profiling. A very powerful and fast tool for profiling CPU and GPU performance.
	/// NOTE: This definition is done project-wide. Undefine here, or toggle by going to Properties -> C/C++ -> Preprocessor -> Preprocessor Definitions.
	//#undef TRACY_ENABLE
	#ifdef TRACY_ENABLE
		/// TRACY_GPU activates gpu zones.
		#define TRACY_GPU

		/// TRACY_DETAILED activates more, less important zones.
		//#define TRACY_DETAILED

		/// TRACY_MEMORY enables tracking memeory allocation with Tracy.
		//#define TRACY_MEMORY

		/// TRACY_REFS Enables tracking of the Ref<T> interface.
		//#define TRACY_REFS

		/// TRACY_SCREEN_CAPTURE enables capturing previews of each frame. 
		//#define TRACY_SCREEN_CAPTURE
		constexpr auto TRACY_CAPTURE_WIDTH = 320;
	#endif

	/// LEAK_DETECTION enables memory leak detection through the use of the _CrtSetDbgFlag function. 
	/// This is only available in debug mode. All .cpp files using the new operator must redefine new to DEBUG_NEW 
	/// for leak reporting to work properly.
	//#define LEAK_DETECTION
#pragma endregion


#pragma region Debug
	/// BREAK_ON_WARN enables breaking into the debugger when a warning is issued through Warn().
	//#define BREAK_ON_WARN

	/// DEBUG_D3D11_DEVICE enables the use of the D3D11 debug device. This is only available in debug mode.
	//#define DEBUG_D3D11_DEVICE

	/// DEBUG_BUILD enables debug features like ImGui, gizmos, debug drawing and debug messages.
	/// NOTE: Entirely separate from VS build contiguration (Debug/Release).
	#define DEBUG_BUILD

	#ifdef DEBUG_BUILD
		/// EDIT_MODE works like a preset for map editing. For example, it sets the active scene to the game scene, 
		/// increases the ambient light level, disables fog and skips spawning the player & monster.
		#define EDIT_MODE

		/// DEBUG_MESSAGES enables writing debug messages to the console using DbgMsg() and opening inline messages using ErrMsg() and Warn().
		#define DEBUG_MESSAGES

		/// DEBUG_DRAW enables drawing lines in the scene at any point of the frame loop.
		#define DEBUG_DRAW
		#ifdef DEBUG_DRAW
			/// DEBUG_DRAW_SORT enables sorting of debug draws by distance to the camera.
			#define DEBUG_DRAW_SORT
		#endif

		/// USE_IMGUI enables the use of ImGui for development purposes. Most runtime development tools are used through ImGui.
		#define USE_IMGUI

		#ifdef USE_IMGUI
			/// USE_IMGUI_VIEWPORTS enables the use of separate ImGui viewports.
			#define USE_IMGUI_VIEWPORTS

			/// USE_IMGUI enables the use of ImGuizmo.
			/// Scaling is currently broken, avoid using anything other than bounds scaling.
			#define USE_IMGUIZMO
		#endif
	#endif
#pragma endregion


#pragma region Path Configuration Defines
	/// WE: Well Engine
	/// D: Directory
	/// F: File
	/// E: Extension

	#define WE_D_ENGINE						TO_SOLUTION_PATH "WellEngine\\Source"
	#define WE_D_ENGINE_SHADER				WE_D_ENGINE "\\Shaders"
	#define WE_D_ENGINE_BEHAVIOUR			WE_D_ENGINE "\\Game\\Behaviours"

	#define WE_D_INTERNAL					TO_SOLUTION_PATH "Internal"
	#define WE_F_INTERNAL_BINDINGS			WE_D_INTERNAL "\\Bindings.json"

	#define WE_D_REGISTRY					WE_D_INTERNAL "\\Registry"
	#define WE_E_REGISTRY					"wer"

	#define WE_D_COMPILED					WE_D_INTERNAL "\\Compiled"
	#define WE_D_COMPILED_CSO				WE_D_COMPILED "\\Shaders"
	#define WE_D_COMPILED_TEXTURE			WE_D_COMPILED "\\Textures"

	#define WE_D_ASSET						TO_SOLUTION_PATH "Content"
	#define WE_D_ASSET_FONT					WE_D_ASSET "\\Fonts"
	#define WE_D_ASSET_MESH					WE_D_ASSET "\\Meshes"
	#define WE_D_ASSET_SOUND				WE_D_ASSET "\\Sounds"
	#define WE_D_ASSET_TEXTURE				WE_D_ASSET "\\Textures"
	#define WE_E_ASSET_FONT					"ttf"

	#define WE_D_DATA						WE_D_ASSET "\\Data"
	#define WE_D_DATA_SAVE					WE_D_DATA "\\Saves"
	#define WE_D_DATA_ATLAS					WE_D_DATA "\\Atlas"
	#define WE_D_DATA_PREFAB				WE_D_DATA "\\Prefabs"
	#define WE_D_DATA_SCENE					WE_D_DATA "\\Scenes"
	#define WE_E_DATA_SAVE					"save"
	#define WE_E_DATA_SCENE					"scene"
	#define WE_E_DATA_PREFAB				"prefab"
	#define WE_E_DATA_ATLAS					"atlas"

	#ifdef USE_IMGUI
	#define WE_D_EDITOR						WE_D_INTERNAL "\\Editor"
	#define WE_D_EDITOR_LAYOUT				WE_D_EDITOR "\\Layouts"
	#endif

	#define WE_DF(path, file)				std::format("{}\\{}", path, file)
	#define WE_DFE(path, file, ext)			std::format("{}\\{}.{}", path, file, ext)
#pragma endregion


#pragma region Graphics Settings
	constexpr auto WINDOW_WIDTH = 1600;
	constexpr auto WINDOW_HEIGHT = 900;

	/// DIM_FORCED_MULTIPLE forces the scene render target dimensions to be a multiple of this value.
	/// This is to ensure that post-processing effects that rely on downsampling work correctly.
	constexpr unsigned int DIM_FORCED_MULTIPLE = 4u;

	/// 3 increases FPS, 2 reduces latency but increases stuttering
	constexpr auto SWAPCHAIN_BUFFER_COUNT = 2; 

	#define SWAPCHAIN_BUFFER_FORMAT			DXGI_FORMAT_R8G8B8A8_UNORM // DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT (HDR)
	#define VIEW_BUFFER_FORMAT				DXGI_FORMAT_R8G8B8A8_UNORM // DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT
	#define VIEW_DEPTH_BUFFER_FORMAT		DXGI_FORMAT_D32_FLOAT // DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT
	#define SHADOW_DEPTH_BUFFER_FORMAT		DXGI_FORMAT_D16_UNORM // DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT
	#define SHADOWCUBE_DEPTH_BUFFER_FORMAT	DXGI_FORMAT_D16_UNORM // DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT

	/// Ideally this is set as low as posible. But the lower it is, the more limited your camera's FOV gets,
	/// as DirectX throws a hissy fit over frustum colliders with too small values and intentionally crashes.
	/// 0.0005f is a reasonable middle-ground for deployment, perhaps until a better solution is found.
	#define LIGHT_CULLING_NEAR_PLANE 0.01f

	/// Maximum amount of lights for each type in each light tile.
	/// Value should follow (n^2 - 1), where n is any non-zero natural number. 
	/// If changing, remember to update MAX_LIGHTS in Shaders/Headers/LightData.hlsli too.
	constexpr unsigned int MAX_LIGHTS = 7u;

	/// Resolution of the light tile grid. Total amount of light tiles is LIGHT_GRID_RES^2.
	constexpr unsigned int LIGHT_GRID_RES = 8u;

	constexpr float LIGHT_MIN_INTENSITY = 0.05f;
#pragma endregion


/// Override settings if in deployment build
#ifdef _DEPLOY
	#undef TRACY_ENABLE
	#undef TRACY_GPU
	#undef TRACY_DETAILED
	#undef TRACY_MEMORY
	#undef TRACY_SCREEN_CAPTURE
	#undef LEAK_DETECTION
	#undef DEBUG_D3D11_DEVICE
	#undef DEBUG_BUILD
	#undef EDIT_MODE
	#undef DEBUG_MESSAGES
	#undef DEBUG_DRAW
	#undef USE_IMGUI
	#undef USE_IMGUI_VIEWPORTS
	#undef USE_IMGUIZMO
	#undef DISABLE_MONSTER
	#undef LIGHT_CULLING_NEAR_PLANE
	#define LIGHT_CULLING_NEAR_PLANE 0.00025f
#endif
