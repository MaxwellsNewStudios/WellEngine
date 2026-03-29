#pragma once
#include "Dependencies/ImGui/imgui.h"
#include "Dependencies/ImGui/imgui_internal.h"

#ifndef IMGUI_DISABLE

typedef int ImGuiCurveEditFlags;

enum ImGuiCurveEditFlags_
{
	ImGuiCurveEditFlags_None			= 0,
	ImGuiCurveEditFlags_Quadratic		= 1 << 0,   // Use quadratic Bezier curves instead of cubic (only one control point per segment, controlPoint1 is used and controlPoint2 is ignored)
	ImGuiCurveEditFlags_Jointed			= 1 << 1,   // Control points are automatically mirrored around the main point when editing
	ImGuiCurveEditFlags_ReadOnly		= 1 << 2,   // Disable editing of points and control points, but still display them
	ImGuiCurveEditFlags_NoLabels		= 1 << 3,   // Disable point number labels (drawn near each point) for better visibility when there are many points
	ImGuiCurveEditFlags_NoPoints		= 1 << 4,   // Don't draw the main points
	ImGuiCurveEditFlags_ForceSpanWidth	= 1 << 5,   // Ensure line spans the entire bound width, by setting x-value of first and last points to bounds min and max
	ImGuiCurveEditFlags_ClampX			= 1 << 6,   // Clamp points to bounds on x-axis while editing
	ImGuiCurveEditFlags_ClampY			= 1 << 7,   // Clamp points to bounds on y-axis while editing
	ImGuiCurveEditFlags_ClampPoints		= ImGuiCurveEditFlags_ClampX | ImGuiCurveEditFlags_ClampY, // Clamp points to bounds on both axes while editing
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

	bool CurveEdit(const char* label, std::vector<BezierPoint>* points, const ImVec2& size, const ImRect& pointBounds, float thickness = 2, ImRect padding = {48, 24, 20, 24}, ImVec2i gridLines = {4, 4}, ImGuiCurveEditFlags flags = ImGuiCurveEditFlags_None);
};

#endif // !IMGUI_DISABLE

