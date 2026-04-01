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
	float xMid = (p1.x + p2.x) * 0.5f;
	ImVec2 cp1 = ImVec2(max(xMid, p1.x + Internal::MinLinkCPDist), p1.y);
	ImVec2 cp2 = ImVec2(min(xMid, p2.x - Internal::MinLinkCPDist), p2.y);
	ImGui::GetWindowDrawList()->AddBezierCubic(p1, cp1, cp2, p2, color, Internal::LinkThickness);
}


void ImGui::NodeGraph::Link::DrawUI(GraphInstance &instance)
{
	// Draw bezier curve from outPin to inPin
	Pin &outPin = instance.pins.at(outPinId);
	Pin &inPin = instance.pins.at(inPinId);

	Node &outNode = instance.nodes.at(outPin.nodeId);
	Node &inNode = instance.nodes.at(inPin.nodeId);

	ImVec2 p1 = outPin.pos + outNode.pos + instance.viewPos;
	ImVec2 p2 = inPin.pos + inNode.pos + instance.viewPos;

	DrawCurve(p1, p2, outPin.preset.color);
}


void ImGui::NodeGraph::Pin::DrawUI(GraphInstance &instance)
{
	// Draw pin as a circle with text label, side depending on whether it's input or output
	// Circle is filled if connected, hollow if not
	// Also draw a small circle in the center if it's currently being linked from/to
	ImDrawList *drawList = GetWindowDrawList();
	ImGuiWindow *window = GetCurrentWindow();

	Node &node = instance.nodes.at(nodeId);
	ImVec2 nodePos = instance.viewPos + node.pos;
	ImVec2 pinCenter = nodePos + pos;

	// Circle
	ImColor pinColor = preset.color;

	if (linkId > 0)
	{
		drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.5f, pinColor);
		drawList->AddCircle(pinCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(pinCenter, Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);
	}
	else
	{
		ImColor fadedPinColor(pinColor.Value * ImVec4(0.6f, 0.6f, 0.6f, 0.8f));

		drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.5f, fadedPinColor);
		drawList->AddCircle(pinCenter + ImVec2(1, 1), Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_BorderShadow), 0, Internal::PinOutlineThickness);
		drawList->AddCircle(pinCenter, Internal::PinSize * 0.5f, GetColorU32(ImGuiCol_Border), 0, Internal::PinOutlineThickness);

		if (instance.linkingPin == id)
		{
			drawList->AddCircleFilled(pinCenter, Internal::PinSize * 0.25f, pinColor);
		}
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
	// Drag-Drop source + target

	std::string payloadType = "Pin";
	if (preset.type == PinType::Custom)
		payloadType += preset.customTypeName;
	else
		payloadType += std::to_string((int)preset.type);

	PushID(std::format("Pin{}", id).c_str());
	SetCursorPos(pinCenter - ImVec2(Internal::PinSize, Internal::PinSize) * 0.5f - instance.viewPos);
	InvisibleButton("DragDropField", ImVec2(Internal::PinSize, Internal::PinSize));

	// Source
	if (instance.linkingPin <= 0 && BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip | ImGuiDragDropFlags_PayloadNoCrossContext))
	{
		// If already linked, we want to undo it and begin a linking with the other pin instead of this one
		if (linkId > 0)
		{
			Link &link = instance.links.at(linkId);
			PinId otherPinId = (link.inPinId == id) ? link.outPinId : link.inPinId;

			instance.RemoveLink(linkId);

			instance.linkingPin = otherPinId;
			SetDragDropPayload(payloadType.c_str(), &otherPinId, sizeof(PinId));
		}
		else
		{
			instance.linkingPin = id;
			SetDragDropPayload(payloadType.c_str(), &id, sizeof(PinId));
		}

		EndDragDropSource();
	}

	// Target
	if (instance.linkingPin > 0 && BeginDragDropTarget())
	{
		const ImGuiPayload *payload = GetDragDropPayload();

		if (payload && payload->IsDataType(payloadType.c_str()))
		{
			PinId payloadPinId = *(PinId*)payload->Data;
			Pin &payloadPin = instance.pins.at(payloadPinId);

			bool allowDrop = (payloadPin.nodeId != nodeId) && (payloadPin.gender != gender);

			if (allowDrop)
			{
				if (AcceptDragDropPayload(payloadType.c_str(), ImGuiDragDropFlags_PayloadNoCrossContext))
				{
					PinId outPinId, inPinId;

					if (gender == PinGender::Output)
					{
						outPinId = id;
						inPinId = payloadPinId;
					}
					else
					{
						outPinId = payloadPinId;
						inPinId = id;
					}

					// if the payload pin is already linked, remove that link
					if (linkId > 0)
						instance.RemoveLink(linkId);

					instance.AddLink(outPinId, inPinId);
					instance.linkingPin = -1;
				}
			}
		}

		EndDragDropTarget();
	}

	PopID();
}


void ImGui::NodeGraph::Node::DrawUI(GraphInstance &instance)
{
	// Draw body, header, then call draw for all pins
	ImDrawList *drawList = GetWindowDrawList();

	ImVec2 nodePos = instance.viewPos + pos;
	ImVec4 nodePosVec4 = ImVec4(nodePos.x, nodePos.y, nodePos.x, nodePos.y);

	ImRect nodeRect(nodePos, nodePos + preset.size);
	ImRect headerRect(nodePos, nodePos + preset.headerSize);
	ImRect inPinsRect(preset.inPinsRect + nodePosVec4);
	ImRect outPinsRect(preset.outPinsRect + nodePosVec4);
	ImRect bodyRect(preset.bodyRect + nodePosVec4);

	drawList->AddRectFilled(nodeRect.Min, nodeRect.Max, GetColorU32(ImGuiCol_FrameBg), Internal::NodeRounding);
	drawList->AddRectFilled(headerRect.Min, headerRect.Max, GetColorU32(ImGuiCol_Header), Internal::NodeRounding);

	if (preset.drawBodyFunc)
	{
		SetCursorPos(bodyRect.Min);
		BeginChild(("NodeBody" + std::to_string(id)).c_str(), bodyRect.GetSize(), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		preset.drawBodyFunc(*this, bodyRect.GetSize());
		EndChild();
	}

	ImFont *font = GetFont();
	drawList->AddText(font, Internal::NodeHeaderTextSize, headerRect.Min + Internal::NodeHeaderPadding, GetColorU32(ImGuiCol_Text), preset.name.c_str());

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
	// - Linking Pin (if any)
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

	// Linking Pin
	if (linkingPin > 0)
	{
		// Ensure that pin is still being linked (if no drag-drop payload, reset linkingPin)
		if (GetDragDropPayload() == nullptr)
		{
			linkingPin = -1;
		}
		else
		{
			Pin &pin = pins.at(linkingPin);
			ImVec2 pinPos = viewPos + pin.pos + nodes.at(pin.nodeId).pos;
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

