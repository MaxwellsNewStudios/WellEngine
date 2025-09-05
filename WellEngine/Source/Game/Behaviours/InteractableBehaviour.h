#pragma once

#include "Behaviour.h"
#include <vector>
#include <functional>
#include "Behaviours/InteractorBehaviour.h"

// Behaviour for iteractable objects
class [[register_behaviour]] InteractableBehaviour : public Behaviour
{
private:
	std::vector<std::function<void()>> _interactionCallbacks;
	bool _isHovered = false;
	int _defaultAmbientId = CONTENT_NULL;
	float _interactionRange = 5.0f;

	InteractorBehaviour *_interactor = nullptr;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] bool OnHover() override;
	[[nodiscard]] bool OffHover() override;

public:
	InteractableBehaviour() = default;
	~InteractableBehaviour() = default;

	void AddInteractionCallback(std::function<void(void)> callback);

	void OnInteraction();

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};