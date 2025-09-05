#pragma once

#include "Behaviours/InteractableBehaviour.h"
#include "Behaviour.h"

class [[register_behaviour]] PickupBehaviour : public Behaviour
{
private:
	bool _isHolding = false;

	Transform *_objectTransform = nullptr;
	Transform *_playerTransform = nullptr;

	dx::XMFLOAT3A _offset = { -0.9f, -0.6f, 0.8f }; // Local offset from player camera

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

public:
	PickupBehaviour() = default;
	~PickupBehaviour() = default;

	void Pickup();

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};

