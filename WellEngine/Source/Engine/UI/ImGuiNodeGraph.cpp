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
	float t = MIN(dist / (Internal::MinLinkCPDist), 1.0f);

	float xMid = (p1.x + p2.x) * 0.5f;
	ImVec2 cp1 = ImVec2(MAX(xMid, p1.x + Internal::MinLinkCPDist * t), p1.y);
	ImVec2 cp2 = ImVec2(MIN(xMid, p2.x - Internal::MinLinkCPDist * t), p2.y);
	ImGui::GetWindowDrawList()->AddBezierCubic(p1, cp1, cp2, p2, color, Internal::LinkThickness);
}


void ImGui::NodeGraph::NodePreset::CalcSize()
{
	/*
	____________________________
	|        HeaderText        |
	|--------------------------|
	o InPin1Text   OutPin1Text o
	o InPin2Text               |
	| ________________________ |
	| |         Body         | |
	| ------------------------ |
	----------------------------
	*/

	ImVec2 headerArea = ImVec2(0, 0);		// Header text size + header padding * 2
	ImVec2 inputPinsArea = ImVec2(0, 0);	// In Pins * (pin size + pin padding * 2 + pin text size.x)
	ImVec2 outputPinsArea = ImVec2(0, 0);	// Out Pins * (pin size + pin padding * 2 + pin text size.x)
	ImVec2 bodyArea = ImVec2(0, 0);			// hasBody * (Body padding * 2)

	ImVec2 totalArea = ImVec2(0, 0);
	// Width: MAX(header, input pins + output pins, body)
	// Height: header + MAX(input pins, output pins) + body

	ImFont *font = ImGui::GetFont();
	bool hasBody = drawBodyFunc != nullptr;

	// Header area
	ImVec2 headerTextSize = font->CalcTextSizeA(Internal::NodeHeaderTextSize, FLT_MAX, 0.0f, name.c_str());
	headerArea = headerTextSize;
	headerArea += Internal::NodeHeaderPadding * 2;

	// Input pins area
	if (!inputs.empty())
	{
		inputPinsArea.y += Internal::PinPadding.y;

		for (const PinPreset &pin : inputs)
		{
			ImVec2 pinTextSize = font->CalcTextSizeA(Internal::PinTextSize, FLT_MAX, 0.0f, pin.name.c_str());
			inputPinsArea.x = MAX(inputPinsArea.x, Internal::PinSize * 0.5f + Internal::PinPadding.x * 2 + pinTextSize.x);
			inputPinsArea.y += MAX(Internal::PinSize, pinTextSize.y) + Internal::PinPadding.y;
		}
	}

	// Output pins area
	if (!outputs.empty())
	{
		outputPinsArea.y += Internal::PinPadding.y;

		for (const PinPreset &pin : outputs)
		{
			ImVec2 pinTextSize = font->CalcTextSizeA(Internal::PinTextSize, FLT_MAX, 0.0f, pin.name.c_str());
			outputPinsArea.x = MAX(outputPinsArea.x, Internal::PinSize * 0.5f + Internal::PinPadding.x * 2 + pinTextSize.x);
			outputPinsArea.y += MAX(Internal::PinSize, pinTextSize.y) + Internal::PinPadding.y;
		}
	}

	// Body area
	if (hasBody)
		bodyArea += bodySize + Internal::NodeBodyPadding * 2;

	float extraPinPadding = (inputs.empty() || outputs.empty()) ? 0 : Internal::PinPadding.x;

	totalArea.x = MAX(headerArea.x, MAX(inputPinsArea.x + outputPinsArea.x + extraPinPadding, bodyArea.x));
	totalArea.y = headerArea.y + MAX(inputPinsArea.y, outputPinsArea.y) + bodyArea.y;

	if (size.x < totalArea.x)
		size.x = totalArea.x;
	if (size.y < totalArea.y)
		size.y = totalArea.y;

	headerSize = ImVec2(MAX(headerArea.x, size.x), headerArea.y);
	inPinsRect = ImVec4(0, headerArea.y, inputPinsArea.x, headerArea.y + inputPinsArea.y);
	outPinsRect = ImVec4(size.x - outputPinsArea.x, headerArea.y, size.x, headerArea.y + outputPinsArea.y);
	bodyRect = ImVec4(
		Internal::NodeBodyPadding.x,
		MAX(inPinsRect.w, outPinsRect.w) + Internal::NodeBodyPadding.y,
		size.x - Internal::NodeBodyPadding.x,
		size.y - Internal::NodeBodyPadding.y
	);
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
		preset.drawBodyFunc(instance, *this, bodyRect.GetSize());
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


const NodePreset *ImGui::NodeGraph::GraphContext::GetNodePreset(size_t index) const
{
	if (index < nodePresets.size())
		return &nodePresets[index];
	return nullptr;
}

const NodePreset *ImGui::NodeGraph::GraphContext::GetNodePreset(const std::string &category, const std::string &name) const
{
	std::string key = category + "/" + name;
	auto it = sortedPresets.find(key);
	if (it != sortedPresets.end())
		return &nodePresets[it->second];
	return nullptr;
}

void ImGui::NodeGraph::GraphContext::AddNodePreset(const NodePreset &preset)
{
	nodePresets.push_back(preset);
	sortedPresets.emplace(preset.category + "/" + preset.name, nodePresets.size() - 1);
}

void ImGui::NodeGraph::GraphContext::RemoveNodePreset(size_t index)
{
	if (index >= nodePresets.size())
		return;
	sortedPresets.erase(nodePresets[index].category + "/" + nodePresets[index].name);
	nodePresets.erase(nodePresets.begin() + index);

	// Update sortedPresets indices by decrementing those greater than the removed index
	for (auto it = sortedPresets.begin(); it != sortedPresets.end();)
	{
		if (it->second > index)
			it->second--;
		it++;
	}
}


PinId ImGui::NodeGraph::GraphInstance::GetNodePin(NodeId nodeId, int pinIndex, PinGender gender)
{
	if (gender == PinGender::Input)
	{
		const Node &node = nodes.at(nodeId);
		if (pinIndex < 0 || pinIndex >= node.inputPinIds.size())
			return 0;

		return node.inputPinIds[pinIndex];
	}
	else
	{
		const Node &node = nodes.at(nodeId);
		if (pinIndex < 0 || pinIndex >= node.outputPinIds.size())
			return 0;

		return node.outputPinIds[pinIndex];
	}
}

int ImGui::NodeGraph::GraphInstance::GetLinkOutIndex(PinId outPinId, PinId inPinId)
{
	if (pins.find(outPinId) == pins.end() || pins.find(inPinId) == pins.end())
		return false;

	const Pin &outPin = pins.at(outPinId);
	const Pin &inPin = pins.at(inPinId);

	if (inPin.linkIds.empty() || outPin.linkIds.empty())
		return false;

	for (int i = 0; i < outPin.linkIds.size(); i++)
	{
		if (outPin.linkIds[i] == inPin.linkIds[0])
			return i;
	}

	return -1;
}

NodeId ImGui::NodeGraph::GraphInstance::AddNode(int presetIndex, const ImVec2 &pos)
{
	if (presetIndex < 0 || presetIndex >= context.GetNodePresetCount())
		return 0;

	const NodePreset &preset = context.GetNodePresets()[presetIndex];

	NodeId nodeId = nextNodeId++;
	nodes.emplace(nodeId, Node(nodeId, preset, pos));
	Node &node = nodes.at(nodeId);

	// Create pins for node
	{
		float pinPosY = preset.inPinsRect.y + Internal::PinPadding.y + Internal::PinSize * 0.5f;
		float pinStrideY = Internal::PinPadding.y + Internal::PinSize;

		node.inputPinIds.reserve(preset.inputs.size());
		for (const PinPreset &pinPreset : preset.inputs)
		{
			ImVec2 pinPos = ImVec2(preset.inPinsRect.x, pinPosY);
			pinPosY += pinStrideY;

			PinId pinId = nextPinId++;
			pins.emplace(pinId, Pin(pinId, nodeId, pinPreset, pinPos, PinGender::Input));

			node.inputPinIds.push_back(pinId);
		}

		pinPosY = preset.outPinsRect.y + Internal::PinPadding.y + Internal::PinSize * 0.5f;

		node.outputPinIds.reserve(preset.outputs.size());
		for (const PinPreset &pinPreset : preset.outputs)
		{
			ImVec2 pinPos = ImVec2(preset.outPinsRect.z, pinPosY);
			pinPosY += pinStrideY;

			PinId pinId = nextPinId++;
			pins.emplace(pinId, Pin(pinId, nodeId, pinPreset, pinPos, PinGender::Output));

			node.outputPinIds.push_back(pinId);
		}
	}

	return nodeId;
}

LinkId ImGui::NodeGraph::GraphInstance::AddLink(PinId outPinId, PinId inPinId)
{
	if (pins.find(outPinId) == pins.end() || pins.find(inPinId) == pins.end())
		return 0;

	Pin &inPin = pins.at(inPinId);
	Pin &outPin = pins.at(outPinId);

	if (inPin.nodeId == outPin.nodeId)
		return 0;

	if (inPin.gender != PinGender::Input || outPin.gender != PinGender::Output)
		return 0;

	if (!inPin.linkIds.empty())
		return 0;

	if (inPin.preset.type != outPin.preset.type)
		return 0;

	if (inPin.preset.type == PinType::Custom)
		if (inPin.preset.customTypeName != outPin.preset.customTypeName)
			return 0;

	// TODO: also check if link would create a cycle

	LinkId linkId = nextLinkId++;
	links.emplace(linkId, Link(linkId, inPinId, outPinId));

	inPin.linkIds.emplace_back(linkId);
	outPin.linkIds.emplace_back(linkId);

	return linkId;
}

void ImGui::NodeGraph::GraphInstance::RemoveLink(LinkId linkId)
{
	if (links.find(linkId) == links.end())
		return;

	Link &link = links.at(linkId);
	Pin &inPin = pins.at(link.inPinId);
	Pin &outPin = pins.at(link.outPinId);

	int outPinLinkIndex = GetLinkOutIndex(outPin.id, inPin.id);

	inPin.linkIds.clear();
	outPin.linkIds.erase(outPin.linkIds.begin() + outPinLinkIndex);

	links.erase(linkId);
}

void ImGui::NodeGraph::GraphInstance::DrawNodeListContextMenu()
{
	// Set window position to mouse position if first frame of opening
	ImVec2 mousePos = ImGui::GetIO().MousePos;
	ImGui::SetNextWindowPos(mousePos, ImGuiCond_Appearing);

	// Right-click on empty space to open context menu for creating nodes
	if (ImGui::BeginPopup("NodeListContextMenu"))
	{
		// TODO: Add search-bar and list nodes in categories as nested TreeNodes
		// See entity "Add Behaviour" popup for setup

		for (size_t i = 0; i < context.GetNodePresetCount(); i++)
		{
			const NodePreset &preset = context.GetNodePresets()[i];
			if (ImGui::MenuItem(preset.name.c_str()))
			{
				ImVec2 ctxMenuPos = ImGui::GetWindowPos();
				ImVec2 spawnPos = ctxMenuPos - windowPos - viewPos;

				AddNode(i, spawnPos);
			}
		}
		ImGui::EndPopup();
	}
}

void ImGui::NodeGraph::GraphInstance::DrawMenuUI()
{
	DrawNodeListContextMenu();

	// eg. Node creation, global i/o, settings, etc.

	// Menu bar
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

	SetNextItemAllowOverlap();
	if (InvisibleButton("##BackgroundInvisButton", GetContentRegionAvail()) && !isPanning)
		selectedNode = -1; // If pressed, deselect nodes

	if (IsItemHovered())
	{
		if (IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
			isPanning = true;

		if (IsMouseClicked(ImGuiMouseButton_Right))
			ImGui::OpenPopup("NodeListContextMenu");
	}

	if (isPanning)
		viewPos += io.MouseDelta;

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
	DrawMenuUI();


	if (IsMouseReleased(ImGuiMouseButton_Left))
	{
		linkingPin = -1; // Cancel linking if released mouse button and no pin caught it
		isDraggingNode = false;
		isPanning = false;
	}


	// If pressed delete and a node is selected, delete it
	if (IsKeyPressed(ImGuiKey_Delete) && selectedNode > 0)
	{
		Node &node = nodes.at(selectedNode);

		// Remove links
		for (PinId pinId : node.inputPinIds)
		{
			Pin &pin = pins.at(pinId);
			if (!pin.linkIds.empty())
				RemoveLink(pin.linkIds[0]);
		}

		for (PinId pinId : node.outputPinIds)
		{
			Pin &pin = pins.at(pinId);
			while (!pin.linkIds.empty())
				RemoveLink(pin.linkIds[0]);
		}

		nodes.erase(selectedNode);
		selectedNode = -1;
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