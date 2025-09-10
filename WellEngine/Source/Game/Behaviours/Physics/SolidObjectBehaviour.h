#pragma once
#include <DirectXMath.h>
#include "Source/Game/Behaviour.h"
#include "Source/Engine/Collision/Intersections.h"

class [[register_behaviour]] SolidObjectBehaviour : public Behaviour
{
private:

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start();

public:
	SolidObjectBehaviour() = default;
	~SolidObjectBehaviour() = default;

	// Adjust position of entity based on collision properties
	void AdjustForCollision(const Collisions::CollisionData &data);

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene);
};