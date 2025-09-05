#pragma once

class DebugData
{
private:
	static constexpr const char *dataFileName = "Assets\\Editor\\DebugData.json";
	float _saveInterval = 3.0f; // Continuously save with this interval
	float _timeUntilNextSave = 0.0f;
	bool _isDirty = false;

public:
	// Debug settings
	float		transformSnap				= 1.0f;
	float		transformScale				= 0.1f;
	int			transformType				= 1; // TransformationType enum
	int			transformSpace				= 1; // ReferenceSpace enum
	int			transformOriginMode			= 1; // TransformOriginMode enum
	bool		transformRelative			= false;
	bool		showViewManipGizmo			= false;
	bool		stretchToFitView			= false;
	float		imGuiFontScale				= 1.0f;
	bool		windowFullscreen			= false;
	bool		windowMaximized				= false;
	int			windowSizeX					= WINDOW_WIDTH;
	int			windowSizeY					= WINDOW_HEIGHT;
	int			sceneViewSizeX				= WINDOW_WIDTH;
	int			sceneViewSizeY				= WINDOW_HEIGHT;
	std::string layoutName					= "Default";
	std::string activeScene					= "Cave";
	bool		billboardGizmosDraw			= false;
	bool		billboardGizmosOverlay		= true;
	float		billboardGizmosSize			= 0.5f;
	float		movementSpeed				= 1.0f;
	float		mouseSensitivity			= 1.0f;
	float		debugCamNearDist			= 0.1f;
	float		debugCamFarDist				= 1000.0f;
	bool		reportComObjectsOnShutdown	= false;


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

	TESTABLE()
};
