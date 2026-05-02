#pragma once

#include "Tests/TestUtils.h"

namespace WellEngine
{
	class DebugData
	{
	#ifdef DEBUG_BUILD
	private:
		static constexpr const char *dataFileName = WE_D_EDITOR "\\DebugData.json";
		std::map<std::string, std::string> _customSettings;
		float _timeUntilNextSave = 0.0f;
		bool _isDirty = false;

	public:
		// Debug settings
		float		saveInterval					= 1.0f; // Continuously save DebugData with this interval
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
		int			contentBrowserDisplayMode		= 0;
		float		contentBrowserIconScale			= 1.0f;


		bool HasSetting(const std::string &name);

		void SetSetting(const std::string &name, bool value);
		void SetSetting(const std::string &name, int value);
		void SetSetting(const std::string &name, size_t value);
		void SetSetting(const std::string &name, float value);
		void SetSetting(const std::string &name, double value);
		void SetSetting(const std::string &name, const std::string &value);

		template<typename T>
		void SetSetting(const std::string &name, const T &value) { SetSetting(name, value); }

		bool				GetSettingBool(const std::string &name,		bool defaultValue = false) const;
		int					GetSettingInt(const std::string &name,		int defaultValue = 0) const;
		size_t				GetSettingULong(const std::string &name,	size_t defaultValue = 0) const;
		float				GetSettingFloat(const std::string &name,	float defaultValue = 0.0f) const;
		double				GetSettingDouble(const std::string &name,	double defaultValue = 0.0) const;
		std::string_view	GetSettingString(const std::string &name,	std::string_view defaultValue = "") const;
		const void *		GetSettingPtr(const std::string &name,		const void *defaultValue = nullptr) const;

		void RemoveSetting(const std::string &name);


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

		TESTABLE
	};
}

