#pragma once
#include "Behaviour.h"

// Behaviour for the player
class [[register_behaviour]] InteractorBehaviour : public Behaviour
{
private:
	float _interactionRange = 2.0f;
	
	Entity *_holdingEnt = nullptr;
	Entity *_hidingEnt = nullptr;
	bool _isHolding = false;
	bool _isHiding = false;

	bool _showingPic = false;
	UINT _totalCollected = 0;
	UINT _totalPieces = 5;
	std::vector<Entity *> _picPieces;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void PostDeserialize() override;

public:
	InteractorBehaviour() = default;
	~InteractorBehaviour() = default;

	float GetInteractionRange() const;

	void ShowPicture();
	void HidePicture();

	UINT GetCollectedPieces() const;
};

