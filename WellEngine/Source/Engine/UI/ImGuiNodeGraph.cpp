#include "stdafx.h"
#include "ImGuiNodeGraph.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

#ifdef USE_IMGUI

using namespace ImGui;


bool NodeGraph::NodeGraph(const char* label, GraphInstance& instance, ImVec2 size, ImGuiNodeGraphFlags flags)
{
    return false;
}

#endif // USE_IMGUI
