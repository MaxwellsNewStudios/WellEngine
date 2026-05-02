#include "stdafx.h"
#include "DebugData.h"

using namespace SerializerUtils;

#ifdef DEBUG_BUILD
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
		data._timeUntilNextSave = data.saveInterval;
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

		json::Value customSettingsObj(json::kObjectType);
		for (const auto &[name, value] : data._customSettings)
		{
			customSettingsObj.AddMember(
				SerializeString(name, docAlloc),
				SerializeString(value, docAlloc), 
				docAlloc
			);
		}
		settingsObj.AddMember("Custom", customSettingsObj, docAlloc);

		settingsObj.AddMember("Save Interval", data.saveInterval, docAlloc);
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
		settingsObj.AddMember("Hierarchy Show Hidden", data.hierarchyShowHidden, docAlloc);
		settingsObj.AddMember("UI Layout", SerializeString(data.layoutName, docAlloc), docAlloc);
		settingsObj.AddMember("Active Scene", SerializeString(data.activeScene, docAlloc), docAlloc);
		settingsObj.AddMember("Billboard Gizmos Draw", data.billboardGizmosDraw, docAlloc);
		settingsObj.AddMember("Billboard Gizmos Overlay", data.billboardGizmosOverlay, docAlloc);
		settingsObj.AddMember("Billboard Gizmos Size", data.billboardGizmosSize, docAlloc);
		settingsObj.AddMember("Movement Speed", data.movementSpeed, docAlloc);
		settingsObj.AddMember("Mouse Sensitivity", data.mouseSensitivity, docAlloc);
		settingsObj.AddMember("Mouse Movement Mode", data.mouseMovementMode, docAlloc);
		settingsObj.AddMember("Debug Camera Near Plane", data.debugCamNearDist, docAlloc);
		settingsObj.AddMember("Debug Camera Far Plane", data.debugCamFarDist, docAlloc);
		settingsObj.AddMember("Enable Fog", data.graphicsFogEnabled, docAlloc);
		settingsObj.AddMember("Enable Emission", data.graphicsEmissionEnabled, docAlloc);
		settingsObj.AddMember("Enable Depth of Field", data.graphicsDofEnabled, docAlloc);
		settingsObj.AddMember("Enable Outline", data.graphicsOutlineEnabled, docAlloc);
		settingsObj.AddMember("Scene Point Filtering", data.graphicsScenePointFiltering, docAlloc);
		settingsObj.AddMember("Emission Resolution Scale", data.graphicsEmissionScale, docAlloc);
		settingsObj.AddMember("Fog Resolution Scale", data.graphicsFogScale, docAlloc);
		settingsObj.AddMember("DoF Resolution Scale", data.graphicsDofScale, docAlloc);
		settingsObj.AddMember("Outline Resolution Scale", data.graphicsOutlineScale, docAlloc);
		settingsObj.AddMember("Content Browser Display Mode", data.contentBrowserDisplayMode, docAlloc);
		settingsObj.AddMember("Content Browser Icon Scale", data.contentBrowserIconScale, docAlloc);
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

		memberName = "Custom";
		if (settings.HasMember(memberName.c_str()))
		{
			const json::Value &customSettings = settings[memberName.c_str()];
			for (json::Value::ConstMemberIterator it = customSettings.MemberBegin(); it != customSettings.MemberEnd(); ++it)
			{
				const std::string &settingName = it->name.GetString();
				const std::string &settingValue = it->value.GetString();
				data._customSettings[settingName] = settingValue;
			}
		}

		memberName = "Save Interval";
		if (settings.HasMember(memberName.c_str()))
			data.saveInterval = settings[memberName.c_str()].GetFloat();

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

		memberName = "Hierarchy Show Hidden";
		if (settings.HasMember(memberName.c_str()))
			data.hierarchyShowHidden = settings[memberName.c_str()].GetBool();

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

		memberName = "Mouse Movement Mode";
		if (settings.HasMember(memberName.c_str()))
			data.mouseMovementMode = settings[memberName.c_str()].GetInt();

		memberName = "Debug Camera Near Plane";
		if (settings.HasMember(memberName.c_str()))
			data.debugCamNearDist = settings[memberName.c_str()].GetFloat();

		memberName = "Debug Camera Far Plane";
		if (settings.HasMember(memberName.c_str()))
			data.debugCamFarDist = settings[memberName.c_str()].GetFloat();

		memberName = "Enable Fog";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsFogEnabled = settings[memberName.c_str()].GetBool();

		memberName = "Enable Emission";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsEmissionEnabled = settings[memberName.c_str()].GetBool();

		memberName = "Enable Depth of Field";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsDofEnabled = settings[memberName.c_str()].GetBool();

		memberName = "Enable Outline";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsOutlineEnabled = settings[memberName.c_str()].GetBool();

		memberName = "Scene Point Filtering";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsScenePointFiltering = settings[memberName.c_str()].GetBool();
		
		memberName = "Emission Resolution Scale";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsEmissionScale = settings[memberName.c_str()].GetFloat();

		memberName = "Fog Resolution Scale";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsFogScale = settings[memberName.c_str()].GetFloat();

		memberName = "DoF Resolution Scale";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsDofScale = settings[memberName.c_str()].GetFloat();

		memberName = "Outline Resolution Scale";
		if (settings.HasMember(memberName.c_str()))
			data.graphicsOutlineScale = settings[memberName.c_str()].GetFloat();

		memberName = "Content Browser Display Mode";
		if (settings.HasMember(memberName.c_str()))
			data.contentBrowserDisplayMode = settings[memberName.c_str()].GetInt();

		memberName = "Content Browser Icon Scale";
		if (settings.HasMember(memberName.c_str()))
			data.contentBrowserIconScale = settings[memberName.c_str()].GetFloat();
	}
}


bool DebugData::HasSetting(const std::string &name)
{
	return _customSettings.find(name) != _customSettings.end();
}

void DebugData::SetSetting(const std::string &name, bool value)
{
	_customSettings[name] = value ? "true" : "false";
	SetDirty();
}
void DebugData::SetSetting(const std::string &name, int value)
{
	_customSettings[name] = std::to_string(value);
	SetDirty();
}
void DebugData::SetSetting(const std::string &name, size_t value)
{
	_customSettings[name] = std::to_string(value);
	SetDirty();
}
void DebugData::SetSetting(const std::string &name, float value)
{
	_customSettings[name] = std::to_string(value);
	SetDirty();
}
void DebugData::SetSetting(const std::string &name, double value)
{
	_customSettings[name] = std::to_string(value);
	SetDirty();
}
void DebugData::SetSetting(const std::string &name, const std::string &value)
{
	_customSettings[name] = value;
	SetDirty();
}

bool DebugData::GetSettingBool(const std::string &name, bool defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;
	
	const std::string &valueStr = it->second;
	if (valueStr == "true")
		return true;
	else if (valueStr == "false")
		return false;
}
int DebugData::GetSettingInt(const std::string &name, int defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return std::stoi(valueStr);
}
size_t DebugData::GetSettingULong(const std::string &name, size_t defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return std::stoul(valueStr);
}
float DebugData::GetSettingFloat(const std::string &name, float defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return std::stof(valueStr);
}
double DebugData::GetSettingDouble(const std::string &name, double defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return std::stod(valueStr);
}
std::string_view DebugData::GetSettingString(const std::string &name, std::string_view defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return valueStr;
}
const void *DebugData::GetSettingPtr(const std::string &name, const void *defaultValue) const
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return defaultValue;

	const std::string &valueStr = it->second;
	return valueStr.data();
}

void DebugData::RemoveSetting(const std::string &name)
{
	auto it = _customSettings.find(name);
	if (it == _customSettings.end())
		return;
	
	_customSettings.erase(it);
	SetDirty();
}
#endif
