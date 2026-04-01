#include "stdafx.h"
#include "ImGuiNodeGraph.h"
#include "Dependencies/ImGui/imgui_internal.h"

#ifdef LEAK_DETECTION
#define new         DEBUG_NEW
#endif

#ifdef USE_IMGUI

using namespace ImGui;
using namespace ImGui::NodeGraph;


void ImGui::NodeGraph::Link::UpdateControlPoints(GraphInstance &instance)
{
	Pin &outPin = instance.pins.at(outPinId);
	Pin &inPin = instance.pins.at(inPinId);

	Node &outNode = instance.nodes.at(outPin.nodeId);
	Node &inNode = instance.nodes.at(inPin.nodeId);

	ImVec2 outPos = outPin.pos + outNode.pos;
	ImVec2 inPos = inPin.pos + inNode.pos;

	float xMid = (outPos.x + inPos.x) * 0.5f;
	cp1 = ImVec2(xMid, outPos.y);
	cp2 = ImVec2(xMid, inPos.y);
}

void ImGui::NodeGraph::Link::DrawUI(GraphInstance &instance)
{
	// Draw bezier curve from outPin to inPin using cp1 and cp2 as control points
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	UpdateControlPoints(instance);

	Pin &outPin = instance.pins.at(outPinId);
	Pin &inPin = instance.pins.at(inPinId);

	Node &outNode = instance.nodes.at(outPin.nodeId);
	Node &inNode = instance.nodes.at(inPin.nodeId);

	ImVec2 p1 = outPin.pos + outNode.pos + instance.viewPos;
	ImVec2 p2 = cp1 + instance.viewPos;
	ImVec2 p3 = cp2 + instance.viewPos;
	ImVec2 p4 = inPin.pos + inNode.pos + instance.viewPos;

	drawList->AddBezierCubic(p1, p2, p3, p4, outPin.preset.color, Internal::LinkThickness);
}


void ImGui::NodeGraph::Pin::DrawUI(GraphInstance &instance)
{
	// Draw pin as a circle with text label, side depending on whether it's input or output
	// Circle is filled if connected, hollow if not
	// Also draw a small circle in the center if it's currently being linked from/to
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	Node &node = instance.nodes.at(nodeId);
	ImVec2 nodePos = instance.viewPos + node.pos;

	// Circle
	ImVec2 circleCenter = nodePos + pos;
	ImColor pinColor = preset.color;

	if (linkId > 0)
	{
		drawList->AddCircleFilled(circleCenter, Internal::PinSize * 0.5f, pinColor);
		drawList->AddCircle(circleCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, ImGui::GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(circleCenter, Internal::PinSize * 0.5f, ImGui::GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);
	}
	else
	{
		ImColor fadedPinColor(pinColor.Value * ImVec4(0.6f, 0.6f, 0.6f, 0.8f));

		drawList->AddCircleFilled(circleCenter, Internal::PinSize * 0.5f, fadedPinColor);
		drawList->AddCircle(circleCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, ImGui::GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(circleCenter, Internal::PinSize * 0.5f, ImGui::GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);

		if (instance.linkingPin == this)
		{
			drawList->AddCircleFilled(circleCenter, Internal::PinSize * 0.25f, pinColor);
		}
	}

	// Text
	ImFont *font = ImGui::GetFont();
	ImVec2 textSize = font->CalcTextSizeA(Internal::PinTextSize, FLT_MAX, 0.0f, preset.name.c_str());

	ImVec2 textPos = circleCenter;
	textPos.y -= textSize.y / 2.0f;

	if (gender == PinGender::Input)
	{
		textPos.x += Internal::PinSize * 0.5f + Internal::PinPadding.x;
	}
	else
	{
		textPos.x -= Internal::PinSize * 0.5f + Internal::PinPadding.x + textSize.x;
	}

	drawList->AddText(font, Internal::PinTextSize, textPos, ImGui::GetColorU32(ImGuiCol_Text), preset.name.c_str());
}


void ImGui::NodeGraph::Node::DrawUI(GraphInstance &instance)
{
	// Draw body, header, then call draw for all pins
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	ImVec2 nodePos = instance.viewPos + pos;
	ImVec4 nodePosVec4 = ImVec4(nodePos.x, nodePos.y, nodePos.x, nodePos.y);

	ImRect nodeRect(nodePos, nodePos + preset.size);
	ImRect headerRect(nodePos, nodePos + preset.headerSize);
	ImRect inPinsRect(preset.inPinsRect + nodePosVec4);
	ImRect outPinsRect(preset.outPinsRect + nodePosVec4);
	ImRect bodyRect(preset.bodyRect + nodePosVec4);

	drawList->AddRectFilled(nodeRect.Min, nodeRect.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), Internal::NodeRounding);
	drawList->AddRectFilled(headerRect.Min, headerRect.Max, ImGui::GetColorU32(ImGuiCol_Header), Internal::NodeRounding);

	if (preset.drawBodyFunc)
	{
		SetCursorPos(bodyRect.Min);
		BeginChild(("NodeBody" + std::to_string(id)).c_str(), bodyRect.GetSize(), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		preset.drawBodyFunc(*this, bodyRect.GetSize());
		EndChild();
	}

	ImFont *font = ImGui::GetFont();
	drawList->AddText(font, Internal::NodeHeaderTextSize, headerRect.Min + Internal::NodeHeaderPadding, ImGui::GetColorU32(ImGuiCol_Text), preset.name.c_str());

	for (PinId pinId : inputPinIds)
	{
		instance.pins.at(pinId).DrawUI(instance);
	}

	for (PinId pinId : outputPinIds)
	{
		instance.pins.at(pinId).DrawUI(instance);
	}
}


bool ImGui::NodeGraph::GraphInstance::Open(ImVec2 size, ImGuiNodeGraphFlags flags)
{
	// Draw order:
	// - Grid
	// - Links
	// - Nodes -> Pins
	// - Graph UI

	ImGuiWindow *window = GetCurrentWindow();
	ImGuiStorage *storage = GetStateStorage();
	ImDrawList *drawList = GetWindowDrawList();
	ImGuiIO &io = GetIO();
	
	ImVec2 windowPos = GetWindowPos();
	viewPos = windowPos;
	viewSize = size;

	ImRect drawArea = ImRect(windowPos, windowPos + size);

	drawList->PushClipRect(drawArea.Min, drawArea.Max, true);

	// Grid

	// Links
	for (auto &linkIt : links)
	{
		linkIt.second.DrawUI((*this));
	}

	// Nodes
	for (auto &nodeIt : nodes)
	{
		nodeIt.second.DrawUI((*this));
	}

	// Graph UI

	drawList->PopClipRect();

	return false;
}

bool ImGui::OpenNodeGraph(const char* label, GraphInstance& instance, ImVec2 size, ImGuiNodeGraphFlags flags)
{
	if (size.x <= 0) size.x = GetContentRegionAvail().x;
	if (size.y <= 0) size.y = GetContentRegionAvail().y;

	BeginChild(label, size, true);
	ImGuiWindow *window = GetCurrentWindow();

	if (window->SkipItems)
	{
		EndChild();
		return false;
	}

	bool result = instance.Open(size, flags);
	
	EndChild();

	return result;
}

#endif // USE_IMGUI

