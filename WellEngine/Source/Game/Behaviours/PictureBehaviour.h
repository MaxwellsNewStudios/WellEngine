#pragma once
#include "Behaviour.h"
#include "Behaviours/InteractableBehaviour.h"
#include "Behaviours/SoundBehaviour.h"

class [[register_behaviour]] PictureBehaviour : public Behaviour
{
private:
	Entity *_ent = nullptr;
	Transform *_t = nullptr;
	bool _isGeneric = false;

	dx::XMFLOAT3A _offset = { 0.0f, 0.0f, 1.0f }; // Local offset from player camera

	SoundBehaviour *_sound = nullptr;
	
protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] virtual bool Start();

	// OnEnable runs immediately after the behaviour is enabled.
	virtual void OnEnable();

	// OnEnable runs immediately after the behaviour is disabled.
	virtual void OnDisable();

public:
	PictureBehaviour() = default;
	PictureBehaviour(bool isGeneric) : _isGeneric(isGeneric) {}
	PictureBehaviour(dx::XMFLOAT3A offset) : _offset(offset) {}
	~PictureBehaviour() = default;

	void Pickup();

	// Serializes the behaviour to a string.
	[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

	// Deserializes the behaviour from a string.
	[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene);
};

