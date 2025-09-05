#pragma once

#include "Behaviour.h"
#include "Rendering/RenderQueuer.h"

class [[register_behaviour]] ExampleBehaviour final : public Behaviour
{
private:
	bool _hasCreatedChild = false;
	bool _debugDraw = false;
	bool _overlay = false;
	bool _firstFrame = true;
	float _spinSpeed = 1.0f;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

public:
	ExampleBehaviour() = default;
	~ExampleBehaviour() = default;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};
