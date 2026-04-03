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
			std::string name;
			std::string category; // Separated by '/' for subcategories, optional
			ImColor headerColor = Internal::NodeHeaderDefaultColor; // Should ideally be dark, keep channels below 164 for good contrast with white text

			ImVec2 size;
			ImVec2 bodySize;

			ImVec2 headerSize; // Internal
			ImVec4 inPinsRect; // Internal
			ImVec4 outPinsRect; // Internal
			ImVec4 bodyRect; // Internal

			std::vector<PinPreset> inputs;
			std::vector<PinPreset> outputs;

			std::function<void(Node&)> execFunc; // Function that gets called when the node is executed

			// Optional: custom draw callback for node body (inside the header)
			// Parameters: node reference, body size
			std::function<void(Node&, ImVec2)> drawBodyFunc;


			void CalcSize()
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
				// Width: max(header, input pins + output pins, body)
				// Height: header + max(input pins, output pins) + body
			
				ImFont* font = ImGui::GetFont();
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
						inputPinsArea.x = max(inputPinsArea.x, Internal::PinSize * 0.5f + Internal::PinPadding.x * 2 + pinTextSize.x);
						inputPinsArea.y += max(Internal::PinSize, pinTextSize.y) + Internal::PinPadding.y;
					}
				}

				// Output pins area
				if (!outputs.empty())
				{
					outputPinsArea.y += Internal::PinPadding.y;

					for (const PinPreset &pin : outputs)
					{
						ImVec2 pinTextSize = font->CalcTextSizeA(Internal::PinTextSize, FLT_MAX, 0.0f, pin.name.c_str());
						outputPinsArea.x = max(outputPinsArea.x, Internal::PinSize * 0.5f + Internal::PinPadding.x * 2 + pinTextSize.x);
						outputPinsArea.y += max(Internal::PinSize, pinTextSize.y) + Internal::PinPadding.y;
					}
				}

				// Body area
				if (hasBody)
					bodyArea += bodySize + Internal::NodeBodyPadding * 2;

				float extraPinPadding = (inputs.empty() || outputs.empty()) ? 0 : Internal::PinPadding.x;

				totalArea.x = max(headerArea.x, max(inputPinsArea.x + outputPinsArea.x + extraPinPadding, bodyArea.x));
				totalArea.y = headerArea.y + max(inputPinsArea.y, outputPinsArea.y) + bodyArea.y;

				if (size.x < totalArea.x)
					size.x = totalArea.x;
				if (size.y < totalArea.y)
					size.y = totalArea.y;

				headerSize = ImVec2(max(headerArea.x, size.x), headerArea.y);
				inPinsRect = ImVec4(0, headerArea.y, inputPinsArea.x, headerArea.y + inputPinsArea.y);
				outPinsRect = ImVec4(size.x - outputPinsArea.x, headerArea.y, size.x, headerArea.y + outputPinsArea.y);
				bodyRect = ImVec4(
					Internal::NodeBodyPadding.x, 
					max(inPinsRect.w, outPinsRect.w) + Internal::NodeBodyPadding.y,
					size.x - Internal::NodeBodyPadding.x,
					size.y - Internal::NodeBodyPadding.y
				);
			}
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

			const NodePreset *GetNodePreset(size_t index) const
			{
				if (index < nodePresets.size())
					return &nodePresets[index];
				return nullptr;
			}
			const NodePreset *GetNodePreset(const std::string &category, const std::string &name) const
			{
				std::string key = category + "/" + name;
				auto it = sortedPresets.find(key);
				if (it != sortedPresets.end())
					return &nodePresets[it->second];
				return nullptr;
			}

			void AddNodePreset(const NodePreset &preset)
			{
				nodePresets.push_back(preset);
				sortedPresets.emplace(preset.category + "/" + preset.name, nodePresets.size() - 1);
			}
			void RemoveNodePreset(size_t index)
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
		};

		class GraphInstance
		{
			friend struct Node;
			friend struct Pin;
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

		public:
			GraphInstance(const GraphContext &ctx) : context(ctx) {}

			// Helpers

			PinId GetNodePin(NodeId nodeId, int pinIndex, PinGender gender)
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

			int GetLinkOutIndex(PinId outPinId, PinId inPinId)
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

			// Editor API

			NodeId AddNode(int presetIndex, const ImVec2 &pos)
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

			LinkId AddLink(PinId outPinId, PinId inPinId)
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

			void RemoveLink(LinkId linkId)
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

			// UI
			bool Open(ImVec2 size = ImVec2(0, 0), ImGuiNodeGraphFlags flags = 0);
		};
	}

	bool OpenNodeGraph(const char* label, NodeGraph::GraphInstance &instance, ImVec2 size = ImVec2(0, 0), ImGuiNodeGraphFlags flags = 0);
};

#endif // USE_IMGUI
