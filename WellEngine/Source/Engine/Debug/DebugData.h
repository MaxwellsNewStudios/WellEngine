#pragma once

class DebugData
{
#ifdef DEBUG_BUILD
private:
	static constexpr const char *dataFileName = ASSETS_EDITOR_PATH "\\DebugData.json";
	float _saveInterval = 1.0f; // Continuously save with this interval
	float _timeUntilNextSave = 0.0f;
	bool _isDirty = false;

public:
	// Debug settings
	float		transformSnap					= 1.0f;
	float		transformScale					= 0.1f;
	int			transformType					= 1; // TransformationType enum
	int			transformSpace					= 1; // ReferenceSpace enum
	int			transformOriginMode				= 1; // TransformOriginMode enum
	bool		transformRelative				= false;
	bool		showViewManipGizmo				= false;
	bool		stretchToFitView				= true;
	float		imGuiFontScale					= 1.0f;
	bool		windowFullscreen				= false;
	bool		windowMaximized					= true;
	int			windowSizeX						= WINDOW_WIDTH;
	int			windowSizeY						= WINDOW_HEIGHT;
	int			sceneViewSizeX					= WINDOW_WIDTH;
	int			sceneViewSizeY					= WINDOW_HEIGHT;
	bool		hierarchyShowHidden				= false;
	std::string layoutName						= "Default";
	std::string activeScene						= "Dev";
	bool		billboardGizmosDraw				= false;
	bool		billboardGizmosOverlay			= true;
	float		billboardGizmosSize				= 0.5f;
	float		movementSpeed					= 1.0f;
	float		mouseSensitivity				= 1.0f;
	int			mouseMovementMode				= 1; // MouseMovementMode enum
	float		debugCamNearDist				= 0.1f;
	float		debugCamFarDist					= 200.0f;
	bool		reportComObjectsOnShutdown		= false;
	bool		graphicsFogEnabled				= true;
	bool		graphicsEmissionEnabled			= true;
	bool		graphicsDofEnabled				= false;
	bool		graphicsOutlineEnabled			= true;
	bool		graphicsScenePointFiltering		= false;
	float		graphicsEmissionScale			= 0.25f;
	float		graphicsFogScale				= 0.25f;
	float		graphicsDofScale				= 0.5f;
	float		graphicsOutlineScale			= 0.5f;


	[[nodiscard]] static inline DebugData &Get()
	{
		static DebugData instance; 
		return instance;
	}

	static void SetDirty() { Get()._isDirty = true; }
	static void Update(float deltaTime);

	// Save all variables in this class to a file
	static void SaveState();
	// Load all variables in this class from a file
	static void LoadState();

#else
public:
	[[nodiscard]] static inline DebugData &Get()
	{
		static DebugData instance; 
		return instance;
	}

	static void SetDirty() {}
	static void Update(float deltaTime) {}
	static void SaveState() {}
	static void LoadState() {}
#endif

	TESTABLE()
};
