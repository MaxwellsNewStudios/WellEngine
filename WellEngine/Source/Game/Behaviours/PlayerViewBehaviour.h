#pragma once
#include "Behaviour.h"
#include "Behaviours/ColliderBehaviour.h"
#include "Behaviours/PlayerMovementBehaviour.h"
#include "Behaviours/InteractorBehaviour.h"
#include "Collision/Raycast.h"

class [[register_behaviour]] PlayerViewBehaviour : public Behaviour
{
private:
	PlayerMovementBehaviour *_movement = nullptr;
	InteractorBehaviour *_interactor = nullptr;
	CameraBehaviour *_camera = nullptr;
	dx::XMFLOAT3A _startPos = { 0.0f, 0.0f, 0.0f };
	dx::XMFLOAT3A _targetPos = { 0.0f, 0.0f, 0.0f };
	float _moveInertia = 1.25f;
	float _targetLerpTime = 0.0f;
	float _targetLerpTimeTotalInverse = 1.0f;

	bool RayCastFromCamera(RaycastOut &out, CameraBehaviour *camera);

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	[[nodiscard]] bool FixedUpdate(float deltaTime, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

public:
	PlayerViewBehaviour() = default;
	~PlayerViewBehaviour() = default;

	PlayerViewBehaviour(PlayerMovementBehaviour *movementBehaviour);

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};