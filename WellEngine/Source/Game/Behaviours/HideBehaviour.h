#pragma once

#include "Behaviours/InteractableBehaviour.h"
#include "Behaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Behaviours/SoundBehaviour.h"

class [[register_behaviour]] HideBehaviour : public Behaviour
{
private:

	Transform *_objectTransform = nullptr;
	Entity *_playerEntity = nullptr;

	SoundBehaviour *_sfx = nullptr;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

public:
	void Hide();

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};

