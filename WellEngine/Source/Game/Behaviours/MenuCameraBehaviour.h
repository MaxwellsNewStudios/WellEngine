#pragma once
#include "Behaviour.h"
#include "Collision/Raycast.h"

class [[register_behaviour]] MenuCameraBehaviour : public Behaviour
{
private:
	CameraBehaviour *_mainCamera = nullptr;
	Entity *_spotLight = nullptr;
	Entity *_buttons[5] = { nullptr };

	bool _drawPointer = false;

	[[nodiscard]] bool RayCastFromMouse(RaycastOut &out, dx::XMFLOAT3A &pos, const Input &input); // Casts a ray from mouse position on nearplane along the z-axis
	
protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] virtual bool Start();

	// Update runs every frame.
	[[nodiscard]] virtual bool Update(TimeUtils &time, const Input &input);

public:
	MenuCameraBehaviour() = default;
	~MenuCameraBehaviour() = default;

	// Serializes the behaviour to a string.
	[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

	// Deserializes the behaviour from a string.
	[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene);
};

