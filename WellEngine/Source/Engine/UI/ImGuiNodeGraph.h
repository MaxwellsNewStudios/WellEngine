#pragma once
#include "Dependencies/ImGui/imgui.h"
#include <vector>
#include <string>
#include <functional>

#ifdef USE_IMGUI

typedef int ImGuiNodeGraphFlags;

enum ImGuiNodeGraphFlags_
{
	ImGuiNodeGraphFlags_None          = 0,
	ImGuiNodeGraphFlags_EnableGrid    = 1 << 0,
};


namespace ImGui
{
	namespace NodeGraph
	{
		namespace Internal
		{
			constexpr float		NodeRounding = 3.0f;
			constexpr ImVec2	NodeHeaderPadding = ImVec2(8, 4);
			constexpr float		NodeHeaderTextSize = 15.0f;
			constexpr bool		NodeHeaderTextShadow = true;
			constexpr ImColor	NodeHeaderDefaultColor = ImColor(128, 128, 128, 230);
			constexpr ImVec2	NodeBodyPadding = ImVec2(4, 4);
			constexpr ImColor	NodeBgColor = ImColor(38, 38, 38, 204);

			constexpr float		PinSize = 12.0f;
			constexpr float		PinOutlineThickness = 3.0f;
			constexpr ImVec2	PinPadding = ImVec2(8, 6);
			constexpr float		PinTextSize = 13.0f;
			constexpr float		PinDragThreshold = 1.0f;

			constexpr float		LinkThickness = 3.0f;
			constexpr float		MinLinkCPDist = 80.0f;
		}

		// Forward decls
		struct Pin;
		struct Node;
		struct Link;
		class GraphInstance;

		using NodeId = int32_t;
		using PinId = int32_t;
		using LinkId = int32_t;

		enum class PinGender : uint8_t
		{
			Input,
			Output
		};

		enum class PinType : uint8_t
		{
			Flow,      // execution flow
			Bool,
			Int,
			Float,
			Vec2,
			Vec3,
			Vec4,
			String,
			Custom
		};


		struct PinPreset
		{
			std::string name;
			std::string customTypeName; // if type is Custom, connecting pins must have the same customTypeName
			ImColor color;
			PinType type;
		};

		struct NodePreset
		{
			friend struct Node;
			friend class GraphInstance;

		private:
			ImVec2 headerSize; // Internal
			ImVec4 inPinsRect; // Internal
			ImVec4 outPinsRect; // Internal
			ImVec4 bodyRect; // Internal

		public:
			std::string name;
			std::string category; // Separated by '/' for subcategories, optional
			ImColor headerColor = Internal::NodeHeaderDefaultColor; // Should ideally be dark, keep channels below 164 for good contrast with white text

			ImVec2 size;
			ImVec2 bodySize;

			std::vector<PinPreset> inputs;
			std::vector<PinPreset> outputs;

			std::function<void(Node&)> execFunc; // Function that gets called when the node is executed

			// Optional: custom draw callback for node body (inside the header)
			// Parameters: node reference, body size
			std::function<void(GraphInstance&, Node&, ImVec2)> drawBodyFunc;


			void CalcSize();
		};


		struct Pin
		{
			const PinId id;
			const NodeId nodeId;
			const PinPreset &preset;

			const ImVec2 pos; // Node-space
			const PinGender gender;

			std::vector<LinkId> linkIds; // May only have multiple if it's an output pin

			Pin(PinId id, NodeId nodeId, const PinPreset &preset, const ImVec2 &pos, PinGender gender) : id(id), nodeId(nodeId), preset(preset), pos(pos), gender(gender) {}

			void DrawUI(GraphInstance &instance);
		};

		struct Node
		{
			const NodeId id;
			const NodePreset &preset;

			std::vector<PinId> inputPinIds;
			std::vector<PinId> outputPinIds;

			ImVec2 pos = ImVec2(0, 0); // Cached

			Node(NodeId id, const NodePreset &preset, ImVec2 pos = ImVec2(0, 0)) : id(id), preset(preset), pos(pos) { }

			void DrawUI(GraphInstance &instance);
		};

		struct Link
		{
			const LinkId id;
			const PinId inPinId;
			const PinId outPinId;

			Link(LinkId id, const PinId &inPinId, const PinId &outPinId) : id(id), inPinId(inPinId), outPinId(outPinId) { }

			void DrawUI(GraphInstance &instance);
		};


		struct GraphContext
		{
		private:
			std::vector<NodePreset> nodePresets;
			std::map<std::string, size_t> sortedPresets;

		public:
			const std::vector<NodePreset> &GetNodePresets() const { return nodePresets; }
			const std::map<std::string, size_t> &GetSortedPresets() const { return sortedPresets; }

			size_t GetNodePresetCount() const { return nodePresets.size(); }

			const NodePreset *GetNodePreset(size_t index) const;
			const NodePreset *GetNodePreset(const std::string &category, const std::string &name) const;

			void AddNodePreset(const NodePreset &preset);
			void RemoveNodePreset(size_t index);
		};

		class GraphInstance
		{
			friend struct PinPreset;
			friend struct NodePreset;
			friend struct Pin;
			friend struct Node;
			friend struct Link;

		private:
			const GraphContext &context;

			std::unordered_map<NodeId, Node> nodes;
			std::unordered_map<PinId, Pin> pins;
			std::unordered_map<LinkId, Link> links;

			// ID generators
			NodeId  nextNodeId = 1;
			PinId  nextPinId = 1;
			LinkId  nextLinkId = 1;

			// Interaction state
			ImVec2 windowPos = ImVec2(0, 0);
			ImVec2 viewPos = ImVec2(0, 0);
			ImVec2 viewSize = ImVec2(0, 0);

			bool firstFrame = true;
			bool isPanning = false;
			bool isDraggingNode = false;
			NodeId selectedNode = -1;
			PinId linkingPin = -1;


			void DrawMenuUI();
			void DrawNodeListContextMenu();

		public:
			GraphInstance(const GraphContext &ctx) : context(ctx) {}

			// Helpers

			PinId GetNodePin(NodeId nodeId, int pinIndex, PinGender gender);

			int GetLinkOutIndex(PinId outPinId, PinId inPinId);

			// Editor API

			NodeId AddNode(int presetIndex, const ImVec2 &pos);

			LinkId AddLink(PinId outPinId, PinId inPinId);

			void RemoveLink(LinkId linkId);

			// UI
			bool Open(ImVec2 size = ImVec2(0, 0), ImGuiNodeGraphFlags flags = 0);
		};
	}

	bool OpenNodeGraph(const char* label, NodeGraph::GraphInstance &instance, ImVec2 size = ImVec2(0, 0), ImGuiNodeGraphFlags flags = 0);
};

#endif // USE_IMGUI
