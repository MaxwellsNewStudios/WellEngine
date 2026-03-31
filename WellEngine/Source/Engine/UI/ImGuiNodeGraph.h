#pragma once
#include "Dependencies/ImGui/imgui.h"

#ifdef USE_IMGUI

typedef int ImGuiNodeGraphFlags;

enum ImGuiNodeGraphFlags_
{
	ImGuiNodeGraphFlags_None = 0,
	ImGuiNodeGraphFlags_ = 1 << 0,
};

namespace ImGui::NodeGraph
{
	// forward decls
	struct Pin;
	struct Node;


	struct PinPreset
	{
		// I/O type
		// Data type (by name)
	};

	struct NodePreset
	{
		// Name
		// Size
		// I/O pin presets
		// Callbacks (e.g. rendering body, execution)
	};


	struct Pin
	{
		// Preset ref
		// Owner node ref
		// Linked pin ref
		// I/O data location ptr
		// Signal state (waiting / has signal)
		// Callbacks (e.g. signal transfer)
	};

	struct Node
	{
		// Preset ref
		// Pos
		// I/O pins
		// I/O data locations
	};

	
	struct GraphContext
	{
		// Node presets
	};

	class GraphInstance
	{
		// Context ref
		// Nodes
	};


	bool NodeGraph(const char* label, GraphInstance &instance, ImVec2 size = { 0, 0 }, ImGuiNodeGraphFlags flags = 0);
};

#endif // USE_IMGUI
