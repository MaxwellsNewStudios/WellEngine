#include "stdafx.h"
#include "ImGuiNodeGraph.h"
#include "Dependencies/ImGui/imgui_internal.h"

#ifdef LEAK_DETECTION
#define new         DEBUG_NEW
#endif

#ifdef USE_IMGUI

using namespace ImGui;
using namespace ImGui::NodeGraph;


static void DrawCurve(const ImVec2 &p1, const ImVec2 &p2, ImColor color)
{
	float xDiff = (p2.x - p1.x);
	float yDiff = (p2.y - p1.y);
	float dist = std::sqrtf(xDiff * xDiff + yDiff * yDiff);
	float t = min(dist / (Internal::MinLinkCPDist), 1.0f);

	float xMid = (p1.x + p2.x) * 0.5f;
	ImVec2 cp1 = ImVec2(max(xMid, p1.x + Internal::MinLinkCPDist * t), p1.y);
	ImVec2 cp2 = ImVec2(min(xMid, p2.x - Internal::MinLinkCPDist * t), p2.y);
	ImGui::GetWindowDrawList()->AddBezierCubic(p1, cp1, cp2, p2, color, Internal::LinkThickness);
}


void ImGui::NodeGraph::Link::DrawUI(GraphInstance &instance)
{
	// Draw bezier curve from outPin to inPin
	Pin &outPin = instance.pins.at(outPinId);
	Pin &inPin = instance.pins.at(inPinId);

	Node &outNode = instance.nodes.at(outPin.nodeId);
	Node &inNode = instance.nodes.at(inPin.nodeId);

	ImVec2 p1 = outPin.pos + outNode.pos + instance.windowPos + instance.viewPos;
	ImVec2 p2 = inPin.pos + inNode.pos + instance.windowPos + instance.viewPos;

	ImColor middleColor = ImColor((outPin.preset.color.Value + inPin.preset.color.Value) * 0.5f);

	DrawCurve(p1, p2, middleColor);
}


void ImGui::NodeGraph::Pin::DrawUI(GraphInstance &instance)
{
	// Draw pin as a circle with text label, side depending on whether it's input or output
	// Circle is filled if connected, hollow if not
	// Also draw a small circle in the center if it's currently being linked from/to
	PushID(std::format("Pin{}", id).c_str());

	ImDrawList *drawList = GetWindowDrawList();
	ImGuiWindow *window = GetCurrentWindow();

	Node &node = instance.nodes.at(nodeId);
	ImVec2 nodePos = instance.windowPos + instance.viewPos + node.pos;
	ImVec2 pinCenter = nodePos + pos;

	// Circle
	ImColor pinColor = preset.color;

	if (linkIds.empty())
	{
		ImColor fadedPinColor(pinColor.Value * ImVec4(0.66f, 0.66f, 0.66f, 0.9f));

		drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.5f, fadedPinColor);
		drawList->AddCircle(pinCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(pinCenter, Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);

		if (instance.linkingPin == id)
			drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.3f, pinColor);
	}
	else
	{
		drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.5f, pinColor);
		drawList->AddCircle(pinCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(pinCenter, Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);
	}


	// Text
	ImFont *font = GetFont();
	ImVec2 textSize = font->CalcTextSizeA(Internal::PinTextSize, FLT_MAX, 0.0f, preset.name.c_str());

	ImVec2 textPos = pinCenter;
	textPos.y -= textSize.y / 2.0f;

	if (gender == PinGender::Input)
	{
		textPos.x += Internal::PinSize * 0.5f + Internal::PinPadding.x;
	}
	else
	{
		textPos.x -= Internal::PinSize * 0.5f + Internal::PinPadding.x + textSize.x;
	}

	drawList->AddText(font, Internal::PinTextSize, textPos, GetColorU32(ImGuiCol_Text), preset.name.c_str());

	// Interaction
	float pinInteractSize = Internal::PinSize + Internal::PinPadding.y;
	SetCursorPos(pinCenter - ImVec2(pinInteractSize, pinInteractSize) * 0.5f - instance.windowPos);
	SetNextItemAllowOverlap();
	InvisibleButton("##PinInvisButton", ImVec2(pinInteractSize, pinInteractSize));

	if (instance.linkingPin > 0) // Target
	{
		Pin &linkPin = instance.pins.at(instance.linkingPin);

		bool compatible = true;
		compatible &= linkPin.nodeId != nodeId;
		compatible &= linkPin.gender != gender;
		compatible &= linkPin.preset.type == preset.type;

		if (compatible && preset.type == PinType::Custom)
			compatible &= linkPin.preset.customTypeName == preset.customTypeName;

		if (compatible)
		{
			if (IsItemHovered(ImGuiHoveredFlags_RectOnly))
			{
				// Draw highlight
				drawList->AddCircle(pinCenter, (Internal::PinSize * 0.5f) + Internal::PinOutlineThickness + 1.0f, GetColorU32(ImGuiCol_DragDropTarget), 0, 3.0f);

				if (IsMouseReleased(ImGuiMouseButton_Left))
				{
					PinId outPinId, inPinId;

					if (gender == PinGender::Output)
					{
						outPinId = id;
						inPinId = instance.linkingPin;
					}
					else
					{
						outPinId = instance.linkingPin;
						inPinId = id;

						// If this pin is an input and already linked, remove that link first
						if (!linkIds.empty())
							instance.RemoveLink(linkIds[0]);
					}

					instance.AddLink(outPinId, inPinId);
					instance.linkingPin = -1;
				}
			}
		}
	}
	else // Source
	{
		if (IsItemHovered())
		{
			// Draw highlight
			ImColor highlightColor = pinColor;
			highlightColor.Value.w *= 0.5f;
			drawList->AddCircle(pinCenter, (Internal::PinSize * 0.5f) + 1.0f, highlightColor, 0, 3.0f);

			if (IsMouseDragging(ImGuiMouseButton_Left, Internal::PinDragThreshold))
			{
				// If this is an already linked input, we want to undo it and begin a linking with the other pin instead of this one
				if (gender == PinGender::Input && !linkIds.empty())
				{
					Link &link = instance.links.at(linkIds[0]);
					PinId otherPinId = (link.inPinId == id) ? link.outPinId : link.inPinId;

					instance.RemoveLink(linkIds[0]);
					instance.linkingPin = otherPinId;
				}
				else
				{
					instance.linkingPin = id;
				}
			}
		}
	}

	PopID();
}


void ImGui::NodeGraph::Node::DrawUI(GraphInstance &instance)
{
	// Draw body, header, then call draw for all pins
	PushID(std::format("Node{}", id).c_str());

	ImDrawList *drawList = GetWindowDrawList();

	ImVec2 nodePos = instance.windowPos + instance.viewPos + pos;
	ImVec4 nodePosVec4 = ImVec4(nodePos.x, nodePos.y, nodePos.x, nodePos.y);

	ImRect nodeRect(nodePos, nodePos + preset.size);
	ImRect headerRect(nodePos, nodePos + preset.headerSize);
	ImRect inPinsRect(preset.inPinsRect + nodePosVec4);
	ImRect outPinsRect(preset.outPinsRect + nodePosVec4);
	ImRect bodyRect(preset.bodyRect + nodePosVec4);

	// Interaction
	SetCursorPos(nodeRect.Min - instance.windowPos);
	SetNextItemAllowOverlap();
	InvisibleButton("##NodeInvisButton", nodeRect.GetSize());

	bool isHovered = IsItemHovered();
	bool isDragged = false;

	if (isHovered)
	{
		if (IsMouseClicked(ImGuiMouseButton_Left))
			instance.selectedNode = id;

		if (instance.selectedNode == id && instance.linkingPin < 0 && !instance.isDraggingNode)
			instance.isDraggingNode = IsMouseDragging(ImGuiMouseButton_Left, 2.0f);
	}

	if (instance.selectedNode == id && instance.isDraggingNode)
	{
		if (IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
		{
			isDragged = true;
			ImVec2 delta = GetIO().MouseDelta;
			pos += delta;

			// Recalculate pin positions
			nodePos = instance.windowPos + instance.viewPos + pos;
			nodePosVec4 = ImVec4(nodePos.x, nodePos.y, nodePos.x, nodePos.y);
			nodeRect = ImRect(nodePos, nodePos + preset.size);
			headerRect = ImRect(nodePos, nodePos + preset.headerSize);
			inPinsRect = ImRect(preset.inPinsRect + nodePosVec4);
			outPinsRect = ImRect(preset.outPinsRect + nodePosVec4);
			bodyRect = ImRect(preset.bodyRect + nodePosVec4);
		}
	}

	// Draw
	drawList->AddRectFilled(nodeRect.Min, nodeRect.Max, Internal::NodeBgColor, Internal::NodeRounding);
	drawList->AddRectFilled(headerRect.Min, headerRect.Max, preset.headerColor, Internal::NodeRounding);
	
	// Draw signifiers for hovered, selected and dragging states
	if (isDragged)
		drawList->AddRect(nodeRect.Min, nodeRect.Max, GetColorU32(ImGuiCol_DragDropTarget, 0.6f), Internal::NodeRounding, 0, 4.0f);
	else if (instance.selectedNode == id)
		drawList->AddRect(nodeRect.Min, nodeRect.Max, GetColorU32(ImGuiCol_HeaderActive, 0.5f), Internal::NodeRounding, 0, 3.0f);
	else if (isHovered)
		drawList->AddRect(nodeRect.Min, nodeRect.Max, GetColorU32(ImGuiCol_HeaderHovered, 0.4f), Internal::NodeRounding, 0, 2.0f);

	if (preset.drawBodyFunc)
	{
		SetCursorPos(bodyRect.Min - instance.windowPos);
		BeginChild(("NodeBody" + std::to_string(id)).c_str(), bodyRect.GetSize(), true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		preset.drawBodyFunc(*this, bodyRect.GetSize());
		EndChild();
	}

	ImFont *font = GetFont();
	ImVec2 headerTextSize = font->CalcTextSizeA(Internal::NodeHeaderTextSize, FLT_MAX, 0.0f, preset.name.c_str());
	if (Internal::NodeHeaderTextShadow)
		drawList->AddText(font, Internal::NodeHeaderTextSize, headerRect.GetCenter() - headerTextSize * 0.5f + ImVec2(1,1), IM_COL32(12, 13, 16, 192), preset.name.c_str());
	drawList->AddText(font, Internal::NodeHeaderTextSize, headerRect.GetCenter() - headerTextSize * 0.5f, GetColorU32(ImGuiCol_Text), preset.name.c_str());

	for (PinId pinId : inputPinIds)
	{
		instance.pins.at(pinId).DrawUI(instance);
	}

	for (PinId pinId : outputPinIds)
	{
		instance.pins.at(pinId).DrawUI(instance);
	}

	PopID();
}


static void DrawMenuUI(GraphInstance &instance)
{
	// eg. Node creation, global i/o, settings, etc.

	// Menu bar

	// Right-click node list
}


bool ImGui::NodeGraph::GraphInstance::Open(ImVec2 size, ImGuiNodeGraphFlags flags)
{
	ImGuiWindow *window = GetCurrentWindow();
	ImGuiStorage *storage = GetStateStorage();
	ImDrawList *drawList = GetWindowDrawList();
	ImGuiIO &io = GetIO();
	
	windowPos = GetWindowPos();
	viewSize = size;

	ImRect drawArea = ImRect(windowPos, windowPos + size);
	drawList->PushClipRect(drawArea.Min, drawArea.Max, true);

	SetCursorPos({0,0});
	SetNextItemAllowOverlap();
	if (InvisibleButton("##BackgroundInvisButton", viewSize) && !isPanning)
		selectedNode = -1; // If pressed, deselect nodes

	if (IsItemHovered())
	{
		if (IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
		{
			isPanning = true;
			ImVec2 delta = io.MouseDelta;
			viewPos += delta;
		}
	}

	// Grid


	// Links
	for (auto &linkIt : links)
		linkIt.second.DrawUI((*this));


	// Linking Pin
	if (linkingPin > 0)
	{
		Pin &pin = pins.at(linkingPin);
		ImVec2 pinPos = windowPos + viewPos + pin.pos + nodes.at(pin.nodeId).pos;
		ImVec2 mousePos = io.MousePos;

		ImVec2 outPos, inPos;
		if (pin.gender == PinGender::Output)
		{
			outPos = pinPos;
			inPos = mousePos;
		}
		else
		{
			outPos = mousePos;
			inPos = pinPos;
		}

		DrawCurve(outPos, inPos, pin.preset.color);
	}


	// Nodes
	for (auto &nodeIt : nodes)
		nodeIt.second.DrawUI((*this));


	// Graph UI
	DrawMenuUI(*this);


	if (IsMouseReleased(ImGuiMouseButton_Left))
	{
		linkingPin = -1; // Cancel linking if released mouse button and no pin caught it
		isDraggingNode = false;
		isPanning = false;
	}

	drawList->PopClipRect();
	firstFrame = false;
	return false;
}

bool ImGui::OpenNodeGraph(const char* label, GraphInstance& instance, ImVec2 size, ImGuiNodeGraphFlags flags)
{
	if (size.x <= 0) size.x = GetContentRegionAvail().x;
	if (size.y <= 0) size.y = GetContentRegionAvail().y;

	BeginChild(label, size, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse);
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

