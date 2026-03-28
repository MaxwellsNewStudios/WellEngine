#pragma once
#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_internal.h"

#ifndef IMGUI_DISABLE

typedef int ImGuiCurveEditFlags;

enum ImGuiCurveEditFlags_
{
	ImGuiCurveEditFlags_None		= 0,
    ImGuiCurveEditFlags_TODO        = 1 << 0,   // Description
};


void ImDrawCallback_ImplDX11_SetSampler(const ImDrawList *parent_list, const ImDrawCmd *cmd);

namespace ImGui
{
	struct BezierPoint
	{
		ImVec2 position;
		ImVec2 controlPoint1;
		ImVec2 controlPoint2;
	};

	bool CurveEdit(const char *label, std::vector<BezierPoint> *points, const ImVec2 &size, const ImRect &pointBounds, float thickness = 2.0f, ImGuiCurveEditFlags flags = ImGuiCurveEditFlags_None);
};

#endif // !IMGUI_DISABLE

