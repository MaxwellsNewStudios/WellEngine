#include "stdafx.h"
#include "UILayout.h"
#include "Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

#ifdef USE_IMGUI
void UILayout::GetLayoutNames(std::vector<std::string> &layouts)
{
	layouts.clear();

	// Search for all .json files in IMGUI_LAYOUTS_PATH
	for (const auto &entry : std::filesystem::directory_iterator(ASSETS_EDITOR_PATH_LAYOUTS))
	{
		const auto &path = entry.path();
		std::string filename = path.filename().string();
		std::string ext = filename.c_str() + filename.find_last_of('.') + 1;

		if (ext != "json")
			continue; // Skip non-layout files

		filename = filename.substr(0, filename.find_last_of('.'));
		layouts.emplace_back(filename);
	}
}

void UILayout::SaveLayout(const std::string &name)
{
	// Create JSON document
	json::Document doc;
	json::Document::AllocatorType &docAlloc = doc.GetAllocator();

	json::Value layoutObj(json::kObjectType);
	{
		ImGuiStyle &layoutStyle = ImGui::GetStyle();
		layoutObj.AddMember("Alpha", layoutStyle.Alpha, docAlloc);
		layoutObj.AddMember("DisabledAlpha", layoutStyle.DisabledAlpha, docAlloc);
		layoutObj.AddMember("WindowPadding", SerializerUtils::SerializeVec(layoutStyle.WindowPadding, docAlloc), docAlloc);
		layoutObj.AddMember("WindowRounding", layoutStyle.WindowRounding, docAlloc);
		layoutObj.AddMember("WindowBorderSize", layoutStyle.WindowBorderSize, docAlloc);
		layoutObj.AddMember("WindowMinSize", SerializerUtils::SerializeVec(layoutStyle.WindowMinSize, docAlloc), docAlloc);
		layoutObj.AddMember("WindowTitleAlign", SerializerUtils::SerializeVec(layoutStyle.WindowTitleAlign, docAlloc), docAlloc);
		layoutObj.AddMember("WindowMenuButtonPosition", layoutStyle.WindowMenuButtonPosition, docAlloc);
		layoutObj.AddMember("ChildRounding", layoutStyle.ChildRounding, docAlloc);
		layoutObj.AddMember("ChildBorderSize", layoutStyle.ChildBorderSize, docAlloc);
		layoutObj.AddMember("PopupRounding", layoutStyle.PopupRounding, docAlloc);
		layoutObj.AddMember("PopupBorderSize", layoutStyle.PopupBorderSize, docAlloc);
		layoutObj.AddMember("FramePadding", SerializerUtils::SerializeVec(layoutStyle.FramePadding, docAlloc), docAlloc);
		layoutObj.AddMember("FrameRounding", layoutStyle.FrameRounding, docAlloc);
		layoutObj.AddMember("FrameBorderSize", layoutStyle.FrameBorderSize, docAlloc);
		layoutObj.AddMember("ItemSpacing", SerializerUtils::SerializeVec(layoutStyle.ItemSpacing, docAlloc), docAlloc);
		layoutObj.AddMember("ItemInnerSpacing", SerializerUtils::SerializeVec(layoutStyle.ItemInnerSpacing, docAlloc), docAlloc);
		layoutObj.AddMember("CellPadding", SerializerUtils::SerializeVec(layoutStyle.CellPadding, docAlloc), docAlloc);
		layoutObj.AddMember("TouchExtraPadding", SerializerUtils::SerializeVec(layoutStyle.TouchExtraPadding, docAlloc), docAlloc);
		layoutObj.AddMember("IndentSpacing", layoutStyle.IndentSpacing, docAlloc);
		layoutObj.AddMember("ColumnsMinSpacing", layoutStyle.ColumnsMinSpacing, docAlloc);
		layoutObj.AddMember("ScrollbarSize", layoutStyle.ScrollbarSize, docAlloc);
		layoutObj.AddMember("ScrollbarRounding", layoutStyle.ScrollbarRounding, docAlloc);
		layoutObj.AddMember("GrabMinSize", layoutStyle.GrabMinSize, docAlloc);
		layoutObj.AddMember("GrabRounding", layoutStyle.GrabRounding, docAlloc);
		layoutObj.AddMember("LogSliderDeadzone", layoutStyle.LogSliderDeadzone, docAlloc);
		layoutObj.AddMember("TabRounding", layoutStyle.TabRounding, docAlloc);
		layoutObj.AddMember("TabBorderSize", layoutStyle.TabBorderSize, docAlloc);
		layoutObj.AddMember("TabMinWidthForCloseButton", layoutStyle.TabMinWidthForCloseButton, docAlloc);
		layoutObj.AddMember("TabBarBorderSize", layoutStyle.TabBarBorderSize, docAlloc);
		layoutObj.AddMember("TabBarOverlineSize", layoutStyle.TabBarOverlineSize, docAlloc);
		layoutObj.AddMember("TableAngledHeadersAngle", layoutStyle.TableAngledHeadersAngle, docAlloc);
		layoutObj.AddMember("TableAngledHeadersTextAlign", SerializerUtils::SerializeVec(layoutStyle.TableAngledHeadersTextAlign, docAlloc), docAlloc);
		layoutObj.AddMember("ColorButtonPosition", layoutStyle.ColorButtonPosition, docAlloc);
		layoutObj.AddMember("ButtonTextAlign", SerializerUtils::SerializeVec(layoutStyle.ButtonTextAlign, docAlloc), docAlloc);
		layoutObj.AddMember("SelectableTextAlign", SerializerUtils::SerializeVec(layoutStyle.SelectableTextAlign, docAlloc), docAlloc);
		layoutObj.AddMember("SeparatorTextBorderSize", layoutStyle.SeparatorTextBorderSize, docAlloc);
		layoutObj.AddMember("SeparatorTextAlign", SerializerUtils::SerializeVec(layoutStyle.SeparatorTextAlign, docAlloc), docAlloc);
		layoutObj.AddMember("SeparatorTextPadding", SerializerUtils::SerializeVec(layoutStyle.SeparatorTextPadding, docAlloc), docAlloc);
		layoutObj.AddMember("DisplayWindowPadding", SerializerUtils::SerializeVec(layoutStyle.DisplayWindowPadding, docAlloc), docAlloc);
		layoutObj.AddMember("DisplaySafeAreaPadding", SerializerUtils::SerializeVec(layoutStyle.DisplaySafeAreaPadding, docAlloc), docAlloc);
		layoutObj.AddMember("DockingSeparatorSize", layoutStyle.DockingSeparatorSize, docAlloc);
		layoutObj.AddMember("MouseCursorScale", layoutStyle.MouseCursorScale, docAlloc);
		layoutObj.AddMember("AntiAliasedLines", layoutStyle.AntiAliasedLines, docAlloc);
		layoutObj.AddMember("AntiAliasedLinesUseTex", layoutStyle.AntiAliasedLinesUseTex, docAlloc);
		layoutObj.AddMember("AntiAliasedFill", layoutStyle.AntiAliasedFill, docAlloc);
		layoutObj.AddMember("CurveTessellationTol", layoutStyle.CurveTessellationTol, docAlloc);
		layoutObj.AddMember("CircleTessellationMaxError", layoutStyle.CircleTessellationMaxError, docAlloc);

		json::Value colorsArray(json::kArrayType);
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
			colorsArray.PushBack(SerializerUtils::SerializeVec(layoutStyle.Colors[i], docAlloc), docAlloc);
		layoutObj.AddMember("Colors", colorsArray, docAlloc);

		layoutObj.AddMember("HoverStationaryDelay", layoutStyle.HoverStationaryDelay, docAlloc);
		layoutObj.AddMember("HoverDelayShort", layoutStyle.HoverDelayShort, docAlloc);
		layoutObj.AddMember("HoverDelayNormal", layoutStyle.HoverDelayNormal, docAlloc);
		layoutObj.AddMember("HoverFlagsForTooltipMouse", layoutStyle.HoverFlagsForTooltipMouse, docAlloc);
		layoutObj.AddMember("HoverFlagsForTooltipNav", layoutStyle.HoverFlagsForTooltipNav, docAlloc);

	}
	doc.SetObject().AddMember("Layout", layoutObj, docAlloc);

	// Write doc to file
	std::ofstream file(PATH_FILE_EXT(ASSETS_EDITOR_PATH_LAYOUTS, name, "json"), std::ios::out);
	if (!file)
		ErrMsg("Could not save layout!");

	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file << buffer.GetString();
	file.close();

	DebugData::Get().layoutName = name;

	// Save current .ini settings
	{
		ImGui::SaveIniSettingsToDisk(PATH_FILE_EXT(ASSETS_EDITOR_PATH_LAYOUTS, name, "ini").c_str());
	}
}
void UILayout::LoadLayout(const std::string &name)
{
	json::Document doc;
	{
		// Check if the layout file exists. If so, use it.
		std::ifstream layoutFile(PATH_FILE_EXT(ASSETS_EDITOR_PATH_LAYOUTS, name, "json"));

		if (!layoutFile.is_open())
			return; // No layout file exists, nothing to load.

		std::string fileContents;
		layoutFile.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(layoutFile)), std::istreambuf_iterator<char>());
		layoutFile.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
		{
			ErrMsgF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
			return;
		}
	}

	json::Value &settings = doc["Layout"];
	{
		ImGuiStyle &layoutStyle = ImGui::GetStyle();
		std::string memberName;

		memberName = "Alpha";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.Alpha = settings[memberName.c_str()].GetFloat();

		memberName = "DisabledAlpha";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.DisabledAlpha = settings[memberName.c_str()].GetFloat();

		memberName = "WindowPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.WindowPadding, settings[memberName.c_str()].GetArray());

		memberName = "WindowRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.WindowRounding = settings[memberName.c_str()].GetFloat();

		memberName = "WindowBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.WindowBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "WindowMinSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.WindowMinSize, settings[memberName.c_str()].GetArray());

		memberName = "WindowTitleAlign";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.WindowTitleAlign, settings[memberName.c_str()].GetArray());

		memberName = "WindowMenuButtonPosition";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsInt())
			layoutStyle.WindowMenuButtonPosition = (ImGuiDir)settings[memberName.c_str()].GetInt();

		memberName = "ChildRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ChildRounding = settings[memberName.c_str()].GetFloat();

		memberName = "ChildBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ChildBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "PopupRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.PopupRounding = settings[memberName.c_str()].GetFloat();

		memberName = "PopupBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.PopupBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "FramePadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.FramePadding, settings[memberName.c_str()].GetArray());

		memberName = "FrameRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.FrameRounding = settings[memberName.c_str()].GetFloat();

		memberName = "FrameBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.FrameBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "ItemSpacing";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.ItemSpacing, settings[memberName.c_str()].GetArray());

		memberName = "ItemInnerSpacing";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.ItemInnerSpacing, settings[memberName.c_str()].GetArray());

		memberName = "CellPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.CellPadding, settings[memberName.c_str()].GetArray());

		memberName = "TouchExtraPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.TouchExtraPadding, settings[memberName.c_str()].GetArray());

		memberName = "IndentSpacing";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.IndentSpacing = settings[memberName.c_str()].GetFloat();

		memberName = "ColumnsMinSpacing";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ColumnsMinSpacing = settings[memberName.c_str()].GetFloat();

		memberName = "ScrollbarSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ScrollbarSize = settings[memberName.c_str()].GetFloat();

		memberName = "ScrollbarRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ScrollbarRounding = settings[memberName.c_str()].GetFloat();

		memberName = "GrabMinSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.GrabMinSize = settings[memberName.c_str()].GetFloat();

		memberName = "GrabRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.GrabRounding = settings[memberName.c_str()].GetFloat();

		memberName = "LogSliderDeadzone";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.LogSliderDeadzone = settings[memberName.c_str()].GetFloat();

		memberName = "TabRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabRounding = settings[memberName.c_str()].GetFloat();

		memberName = "ScrollbarRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.ScrollbarRounding = settings[memberName.c_str()].GetFloat();

		memberName = "GrabMinSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.GrabMinSize = settings[memberName.c_str()].GetFloat();

		memberName = "GrabRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.GrabRounding = settings[memberName.c_str()].GetFloat();

		memberName = "LogSliderDeadzone";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.LogSliderDeadzone = settings[memberName.c_str()].GetFloat();

		memberName = "TabRounding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabRounding = settings[memberName.c_str()].GetFloat();

		memberName = "TabBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "TabMinWidthForCloseButton";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabMinWidthForCloseButton = settings[memberName.c_str()].GetFloat();

		memberName = "TabBarBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabBarBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "TabBarOverlineSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TabBarOverlineSize = settings[memberName.c_str()].GetFloat();

		memberName = "TableAngledHeadersAngle";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.TableAngledHeadersAngle = settings[memberName.c_str()].GetFloat();

		memberName = "TableAngledHeadersTextAlign";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.TableAngledHeadersTextAlign, settings[memberName.c_str()].GetArray());

		memberName = "ColorButtonPosition";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsInt())
			layoutStyle.ColorButtonPosition = (ImGuiDir)settings[memberName.c_str()].GetInt();

		memberName = "ButtonTextAlign";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.ButtonTextAlign, settings[memberName.c_str()].GetArray());

		memberName = "SelectableTextAlign";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.SelectableTextAlign, settings[memberName.c_str()].GetArray());

		memberName = "SeparatorTextBorderSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.SeparatorTextBorderSize = settings[memberName.c_str()].GetFloat();

		memberName = "SeparatorTextAlign";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.SeparatorTextAlign, settings[memberName.c_str()].GetArray());

		memberName = "SeparatorTextPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.SeparatorTextPadding, settings[memberName.c_str()].GetArray());

		memberName = "DisplayWindowPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.DisplayWindowPadding, settings[memberName.c_str()].GetArray());

		memberName = "DisplaySafeAreaPadding";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
			SerializerUtils::DeserializeVec(layoutStyle.DisplaySafeAreaPadding, settings[memberName.c_str()].GetArray());

		memberName = "DockingSeparatorSize";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.DockingSeparatorSize = settings[memberName.c_str()].GetFloat();

		memberName = "MouseCursorScale";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.MouseCursorScale = settings[memberName.c_str()].GetFloat();

		memberName = "AntiAliasedLines";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsBool())
			layoutStyle.AntiAliasedLines = settings[memberName.c_str()].GetBool();

		memberName = "AntiAliasedLinesUseTex";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsBool())
			layoutStyle.AntiAliasedLinesUseTex = settings[memberName.c_str()].GetBool();

		memberName = "AntiAliasedFill";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsBool())
			layoutStyle.AntiAliasedFill = settings[memberName.c_str()].GetBool();

		memberName = "CurveTessellationTol";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.CurveTessellationTol = settings[memberName.c_str()].GetFloat();

		memberName = "CircleTessellationMaxError";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.CircleTessellationMaxError = settings[memberName.c_str()].GetFloat();

		memberName = "Colors";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsArray())
		{
			const json::Value &colorsArray = settings[memberName.c_str()];
			for (int i = 0; i < ImGuiCol_COUNT && i < colorsArray.Size(); ++i)
			{
				if (colorsArray[i].IsArray())
					SerializerUtils::DeserializeVec(layoutStyle.Colors[i], colorsArray[i].GetArray());
			}
		}

		memberName = "HoverStationaryDelay";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.HoverStationaryDelay = settings[memberName.c_str()].GetFloat();

		memberName = "HoverDelayShort";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.HoverDelayShort = settings[memberName.c_str()].GetFloat();

		memberName = "HoverDelayNormal";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.HoverDelayNormal = settings[memberName.c_str()].GetFloat();

		memberName = "HoverFlagsForTooltipMouse";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.HoverFlagsForTooltipMouse = settings[memberName.c_str()].GetFloat();

		memberName = "HoverFlagsForTooltipNav";
		if (settings.HasMember(memberName.c_str()) && settings[memberName.c_str()].IsFloat())
			layoutStyle.HoverFlagsForTooltipNav = settings[memberName.c_str()].GetFloat();
	}

	DebugData::Get().layoutName = name;

	// Copy over .ini file (for docking, window placement, etc.)
	{
		std::string iniFilePath = PATH_FILE_EXT(ASSETS_EDITOR_PATH_LAYOUTS, name, "ini");
		// Check if the ini file exists.
		std::ifstream iniFile(iniFilePath);
		if (!iniFile.is_open())
			return; // No ini file to copy over
		iniFile.close();

		ImGui::LoadIniSettingsFromDisk(iniFilePath.c_str());
	}
}
#endif // USE_IMGUI