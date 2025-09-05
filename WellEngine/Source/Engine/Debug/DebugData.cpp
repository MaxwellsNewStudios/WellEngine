#include "stdafx.h"
#include "DebugData.h"


void DebugData::Update(float deltaTime)
{
	DebugData &data = Get();
	data._timeUntilNextSave -= deltaTime;

	if (data._isDirty || data._timeUntilNextSave <= 0.0f)
	{
		data._isDirty = false;

		Input &input = Input::Instance();
		data.windowSizeX = input.GetWindowSize().x;
		data.windowSizeY = input.GetWindowSize().y;

		if (Window *wnd = input.GetWindow())
		{
			SDL_Window *sdlWnd = wnd->GetWindow();
			SDL_WindowFlags wFlags = SDL_GetWindowFlags(sdlWnd);
			data.windowMaximized = (wFlags & SDL_WINDOW_MAXIMIZED) != 0;
		}

		data.SaveState();
		data._timeUntilNextSave = data._saveInterval;
	}
}

void DebugData::SaveState()
{
	// Create JSON document
	json::Document doc;
	json::Document::AllocatorType &docAlloc = doc.GetAllocator();

	json::Value settingsObj(json::kObjectType);
	{
		DebugData &data = Get();
		settingsObj.AddMember("Transform Snap", data.transformSnap, docAlloc);
		settingsObj.AddMember("Transform Scale", data.transformScale, docAlloc);
		settingsObj.AddMember("Transform Type", data.transformType, docAlloc);
		settingsObj.AddMember("Transform Space", data.transformSpace, docAlloc);
		settingsObj.AddMember("Transform Origin", data.transformOriginMode, docAlloc);
		settingsObj.AddMember("Transform Relative", data.transformRelative, docAlloc);
		settingsObj.AddMember("Show View Manipulator", data.showViewManipGizmo, docAlloc);
		settingsObj.AddMember("Stretch To Fit View", data.stretchToFitView, docAlloc);
		settingsObj.AddMember("ImGui Font Scale", data.imGuiFontScale, docAlloc);
		settingsObj.AddMember("Window Fullscreen", data.windowFullscreen, docAlloc);
		settingsObj.AddMember("Window Maximized", data.windowMaximized, docAlloc);
		settingsObj.AddMember("Window Size X", data.windowSizeX, docAlloc);
		settingsObj.AddMember("Window Size Y", data.windowSizeY, docAlloc);
		settingsObj.AddMember("Scene View Size X", data.sceneViewSizeX, docAlloc);
		settingsObj.AddMember("Scene View Size Y", data.sceneViewSizeY, docAlloc);
		settingsObj.AddMember("UI Layout", SerializerUtils::SerializeString(data.layoutName, docAlloc), docAlloc);
		settingsObj.AddMember("Active Scene", SerializerUtils::SerializeString(data.activeScene, docAlloc), docAlloc);
		settingsObj.AddMember("Billboard Gizmos Draw", data.billboardGizmosDraw, docAlloc);
		settingsObj.AddMember("Billboard Gizmos Overlay", data.billboardGizmosOverlay, docAlloc);
		settingsObj.AddMember("Billboard Gizmos Size", data.billboardGizmosSize, docAlloc);
		settingsObj.AddMember("Movement Speed", data.movementSpeed, docAlloc);
		settingsObj.AddMember("Mouse Sensitivity", data.mouseSensitivity, docAlloc);
		settingsObj.AddMember("Debug Camera Near Plane", data.debugCamNearDist, docAlloc);
		settingsObj.AddMember("Debug Camera Far Plane", data.debugCamFarDist, docAlloc);
	}
	doc.SetObject().AddMember("Settings", settingsObj, docAlloc);

	// Write doc to file
	std::ofstream file(dataFileName, std::ios::out);
	if (!file)
		ErrMsg("Could not save debug data!");

	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file << buffer.GetString();
	file.close();
}

void DebugData::LoadState()
{
	json::Document doc;
	{
		// Check if a data file exists. If so, use it.
		std::ifstream dataFile(dataFileName);

		if (!dataFile.is_open())
			return; // No data file exists, nothing to load.

		std::string fileContents;
		dataFile.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(dataFile)), std::istreambuf_iterator<char>());
		dataFile.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
			ErrMsgF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
	}

	json::Value &settings = doc["Settings"];
	{
		DebugData &data = Get();
		std::string memberName;

		memberName = "Transform Snap";
		if (settings.HasMember(memberName.c_str()))
			data.transformSnap = settings[memberName.c_str()].GetFloat();

		memberName = "Transform Scale";
		if (settings.HasMember(memberName.c_str()))
			data.transformScale = settings[memberName.c_str()].GetFloat();

		memberName = "Transform Type";
		if (settings.HasMember(memberName.c_str()))
			data.transformType = settings[memberName.c_str()].GetInt();

		memberName = "Transform Space";
		if (settings.HasMember(memberName.c_str()))
			data.transformSpace = settings[memberName.c_str()].GetInt();

		memberName = "Transform Origin";
		if (settings.HasMember(memberName.c_str()))
			data.transformOriginMode = settings[memberName.c_str()].GetInt();

		memberName = "Transform Relative";
		if (settings.HasMember(memberName.c_str()))
			data.transformRelative = settings[memberName.c_str()].GetBool();

		memberName = "Show View Manipulator";
		if (settings.HasMember(memberName.c_str()))
			data.showViewManipGizmo = settings[memberName.c_str()].GetBool();

		memberName = "Stretch To Fit View";
		if (settings.HasMember(memberName.c_str()))
			data.stretchToFitView = settings[memberName.c_str()].GetBool();

		memberName = "ImGui Font Scale";
		if (settings.HasMember(memberName.c_str()))
			data.imGuiFontScale = settings[memberName.c_str()].GetFloat();

		memberName = "Window Fullscreen";
		if (settings.HasMember(memberName.c_str()))
			data.windowFullscreen = settings[memberName.c_str()].GetBool();

		memberName = "Window Maximized";
		if (settings.HasMember(memberName.c_str()))
			data.windowMaximized = settings[memberName.c_str()].GetBool();

		memberName = "Window Size X";
		if (settings.HasMember(memberName.c_str()))
			data.windowSizeX = settings[memberName.c_str()].GetInt();

		memberName = "Window Size Y";
		if (settings.HasMember(memberName.c_str()))
			data.windowSizeY = settings[memberName.c_str()].GetInt();

		memberName = "Scene View Size X";
		if (settings.HasMember(memberName.c_str()))
			data.sceneViewSizeX = settings[memberName.c_str()].GetInt();

		memberName = "Scene View Size Y";
		if (settings.HasMember(memberName.c_str()))
			data.sceneViewSizeY = settings[memberName.c_str()].GetInt();

		memberName = "UI Layout";
		if (settings.HasMember(memberName.c_str()))
			data.layoutName = settings[memberName.c_str()].GetString();

		memberName = "Active Scene";
		if (settings.HasMember(memberName.c_str()))
			data.activeScene = settings[memberName.c_str()].GetString();

		memberName = "Billboard Gizmos Draw";
		if (settings.HasMember(memberName.c_str()))
			data.billboardGizmosDraw = settings[memberName.c_str()].GetBool();

		memberName = "Billboard Gizmos Overlay";
		if (settings.HasMember(memberName.c_str()))
			data.billboardGizmosOverlay = settings[memberName.c_str()].GetBool();

		memberName = "Billboard Gizmos Size";
		if (settings.HasMember(memberName.c_str()))
			data.billboardGizmosSize = settings[memberName.c_str()].GetFloat();

		memberName = "Movement Speed";
		if (settings.HasMember(memberName.c_str()))
			data.movementSpeed = settings[memberName.c_str()].GetFloat();

		memberName = "Mouse Sensitivity";
		if (settings.HasMember(memberName.c_str()))
			data.mouseSensitivity = settings[memberName.c_str()].GetFloat();

		memberName = "Debug Camera Near Plane";
		if (settings.HasMember(memberName.c_str()))
			data.debugCamNearDist = settings[memberName.c_str()].GetFloat();

		memberName = "Debug Camera Far Plane";
		if (settings.HasMember(memberName.c_str()))
			data.debugCamFarDist = settings[memberName.c_str()].GetFloat();
	}
}
