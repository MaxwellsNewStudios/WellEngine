#pragma once

#include "Behaviour.h"
#include "Behaviours/InteractableBehaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"

class [[register_behaviour]] BreadcrumbPileBehaviour : public Behaviour
{
private:
	BillboardMeshBehaviour *_flare = nullptr;
	Entity *_flashlightEntity = nullptr;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

public:
	BreadcrumbPileBehaviour() = default;
	~BreadcrumbPileBehaviour() = default;

	void Pickup();
};

