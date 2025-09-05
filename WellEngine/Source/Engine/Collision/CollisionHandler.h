#pragma once

#include "Collision/Colliders.h"
#include "Behaviours/ColliderBehaviour.h"

class Scene;

class CollisionHandler
{
public:
	CollisionHandler() = default;
	~CollisionHandler() = default;

	bool Initialize(Scene *scene);
	bool CheckCollisions(TimeUtils &time, Scene *scene, ID3D11DeviceContext *context);

	bool CheckCollision(const Collisions::Collider *col, Scene *scene, Collisions::CollisionData &data);

private:
	std::vector<const Collisions::Collider *> _collidersToCheck;
	std::vector<Entity *> _entitiesToCheck;
	std::vector<ColliderBehaviour *> _colliderBehavioursToCheck;

	TESTABLE()
};


