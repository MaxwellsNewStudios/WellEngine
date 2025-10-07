#pragma region Includes & Usings
#include "stdafx.h"
#include "ImGuiUtils.h"
#include "Source/Engine/Debug/DebugData.h"
#include <stack>

using namespace ImGuiUtils;

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion


#ifdef USE_IMGUI
void ImGuiUtils::WrapMousePosEx(int axises_mask, const ImRect &wrap_rect)
{
	ImGuiContext &g = *GImGui;
	IM_ASSERT(axises_mask == 1 || axises_mask == 2 || axises_mask == (1 | 2));
	ImVec2 p_mouse = g.IO.MousePos;
	for (int axis = 0; axis < 2; axis++)
	{
		if ((axises_mask & (1 << axis)) == 0)
			continue;
		if (p_mouse[axis] >= wrap_rect.Max[axis])
			p_mouse[axis] = wrap_rect.Min[axis] + 1.0f;
		else if (p_mouse[axis] <= wrap_rect.Min[axis])
			p_mouse[axis] = wrap_rect.Max[axis] - 1.0f;
	}
	if (p_mouse.x != g.IO.MousePos.x || p_mouse.y != g.IO.MousePos.y)
		ImGui::TeleportMousePos(p_mouse);
}
void ImGuiUtils::WrapMousePos(int axises_mask)
{
	ImGuiContext &g = *GImGui;
#ifdef IMGUI_HAS_DOCK
	if (g.IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		const ImGuiPlatformMonitor *monitor = ImGui::GetViewportPlatformMonitor(g.MouseViewport);
		WrapMousePosEx(axises_mask, ImRect(monitor->MainPos, monitor->MainPos + monitor->MainSize - ImVec2(1.0f, 1.0f)));
	}
	else
#endif
	{
		ImGuiViewport *viewport = ImGui::GetMainViewport();
		WrapMousePosEx(axises_mask, ImRect(viewport->Pos, viewport->Pos + viewport->Size - ImVec2(1.0f, 1.0f)));
	}
}
void ImGuiUtils::LockMouseOnActive()
{
	static ImGuiID target_item = 0;
	static ImVec2  prev_mous_pos;

	ImGuiContext &g = *GImGui;
	ImGuiID id = ImGui::GetItemID();

	if (ImGui::IsItemActive() && (!ImGui::GetInputTextState(id) || g.InputTextDeactivatedState.ID == id))
	{
		if (target_item == 0)
		{
			target_item = id;
			prev_mous_pos = ImGui::GetMousePos();
		}
		WrapMousePos(1 << ImGuiAxis_X);
		ImGui::SetMouseCursor(ImGuiMouseCursor_None);
	}
	else if (target_item > 0 && target_item == id && (ImGui::IsItemDeactivated() || g.InputTextDeactivatedState.ID != id))
	{
		ImGui::TeleportMousePos(prev_mous_pos);
		target_item = 0;
	}
}

static std::stack<StyleType> &GetStyleStack()
{
	static std::stack<StyleType> styleStack;
	return styleStack;
}
void ImGuiUtils::BeginButtonStyle(StyleType style)
{
	std::stack<StyleType> &styleStack = GetStyleStack();
	styleStack.push(style);

	switch (style)
	{
	case ImGuiUtils::StyleType::Red:
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.55f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.65f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.75f, 0.7f));
		break;

	case ImGuiUtils::StyleType::Green:
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.33f, 0.55f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.33f, 0.65f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.33f, 0.75f, 0.7f));
		break;

	case ImGuiUtils::StyleType::Yellow:
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.16f, 0.7f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.16f, 0.8f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.16f, 0.9f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor::HSV(0.0f, 0.0f, 1.0f));
		break;
		
	case ImGuiUtils::StyleType::Cornflower:
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.60694f, 0.578f, 0.929f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.60694f, 0.678f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.60694f, 0.778f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
		break;
		
	default:
		Warn("Style not implemented!");
		break;
	}
}
void ImGuiUtils::EndButtonStyle()
{
	std::stack<StyleType> &styleStack = GetStyleStack();
	if (styleStack.empty())
	{
		ErrMsg("Style stack underflow!");
		return;
	}

	switch (styleStack.top())
	{
	case ImGuiUtils::StyleType::Red:
	case ImGuiUtils::StyleType::Green:
		ImGui::PopStyleColor(3);
		break;

	case ImGuiUtils::StyleType::Yellow:
	case ImGuiUtils::StyleType::Cornflower:
		ImGui::PopStyleColor(4);
		break;

	default:
		Warn("Style not implemented!");
		break;
	}
	styleStack.pop();
}

bool ImGuiUtils::BeginFont(const std::string &name, float scale)
{
	ImFont *font = nullptr;
	if (!Utils::GetFont(name, font))
	{
		DbgMsgF("Font '{}' not found!", name);
		return false;
	}

	ImGui::PushFont(font, scale);
	return true;
}
void ImGuiUtils::EndFont()
{
	ImGui::PopFont();
}
bool ImGuiUtils::SetDefaultFont(const std::string &name)
{
	ImFont *font = nullptr;
	if (!Utils::GetFont(name, font))
	{
		DbgMsgF("Font '{}' not found!", name);
		return false;
	}

	ImGui::GetIO().FontDefault = font;
	return true;
}

#pragma region ImGuiAutoWindow
const std::string &ImGuiAutoWindow::GetID() const
{
	return _id;
}
bool ImGuiAutoWindow::IsClosed() const
{
	return !_open;
}

bool ImGuiAutoWindow::Render()
{
	if (_func == nullptr)
		return false;

	ImGui::PushID(_id.c_str());
	ImGui::Begin(_name.c_str(), &_open);
	ImGui::SetWindowFontScale(DebugData::Get().imGuiFontScale);
	bool result = _func();
	ImGui::End();
	ImGui::PopID();

	return result;
}
#pragma endregion


#pragma region Utils
Utils *Utils::GetInstance()
{
	static Utils instance;
	return &instance;
}

UINT Utils::GetWindowCount()
{
	Utils *instance = GetInstance();
	return instance->_windows.size();
}

bool Utils::GetWindow(const std::string &id, ImGuiAutoWindow **window)
{
	Utils *instance = GetInstance();
	for (ImGuiAutoWindow &wind : instance->_windows)
	{
		if (wind.GetID() == id)
		{
			if (window)
				(*window) = &wind;

			return true;
		}
	}
	return false;
}
const std::string *Utils::GetWindowID(UINT index)
{
	Utils *instance = GetInstance();
	if (index >= instance->_windows.size())
		return nullptr;

	return &instance->_windows[index].GetID();
}

bool Utils::OpenWindow(const ImGuiAutoWindow &window)
{
	Utils *instance = GetInstance();

	if (GetWindow(window.GetID(), nullptr))
		return false;

	instance->_windows.push_back(window);
	return true;
}
bool Utils::OpenWindow(const std::string &name, const std::string &id, std::function<bool(void)> func)
{
	ImGuiAutoWindow window(name, id, func);
	return OpenWindow(window);
}
bool Utils::CloseWindow(const std::string &id)
{
	Utils *instance = GetInstance();
	for (int i = 0; i < instance->_windows.size(); i++)
	{
		if (instance->_windows[i].GetID() == id)
		{
			instance->_windows.erase(instance->_windows.begin() + i);
			return true;
		}
	}
	return false;
}

bool Utils::Render()
{
	Utils *instance = GetInstance();

	for (int i = 0; i < instance->_windows.size(); i++)
	{
		ImGuiAutoWindow &wind = instance->_windows[i];

		if (!wind.Render())
		{
			ErrMsgF("Failed to render window (ID:'{}')!", wind.GetID());
			return false;
		}

		if (wind.IsClosed())
		{
			// Close window
			if (!instance->CloseWindow(wind.GetID()))
			{
				ErrMsgF("Failed to close window (ID:'{}')!", wind.GetID());
				return false;
			}

			i--;
		}
	}

	return true;
}
#pragma endregion
#endif