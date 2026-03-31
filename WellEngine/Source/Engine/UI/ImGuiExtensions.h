#pragma once
#include "Dependencies/ImGui/imgui.h"
#include "ImGuiUtils.h"
#include "ImGuiCurve.h"
#include "ImGuiNodeGraph.h"

#ifndef IMGUI_DISABLE
void ImDrawCallback_ImplDX11_SetSampler(const ImDrawList *parent_list, const ImDrawCmd *cmd);
#endif // !IMGUI_DISABLE
