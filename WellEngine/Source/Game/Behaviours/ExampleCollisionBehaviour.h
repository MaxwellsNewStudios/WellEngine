#pragma once

#include "Behaviour.h"

class [[register_behaviour]] ExampleCollisionBehaviour : public Behaviour
{
protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

public:
	ExampleCollisionBehaviour() = default;
	~ExampleCollisionBehaviour() = default;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};
