#pragma once
#include <vector>

#include "Behaviour.h"

class [[register_behaviour]] GraphNodeBehaviour final : public Behaviour
{
private:
	std::vector<GraphNodeBehaviour*> _connections;
	std::vector<UINT> _deserializedConnections;
	float _cost = 0.0f;

protected:
	[[nodiscard]] bool Start() override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

	void OnEnable() override;
	void OnDisable() override;

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
	void PostDeserialize() override;

public:
	GraphNodeBehaviour() = default;
	~GraphNodeBehaviour();

	void SetCost(float cost);
	[[nodiscard]] float GetCost() const;
	[[nodiscard]] const std::vector<GraphNodeBehaviour *> &GetConnections() const;
	void AddConnection(GraphNodeBehaviour *connection, bool secondIteration = false);
	void RemoveConnection(GraphNodeBehaviour *connection, bool secondIteration = false);

	[[nodiscard]] bool DrawConnections();

#ifdef DEBUG_BUILD
	void SetShowNode(bool show);
#endif
};