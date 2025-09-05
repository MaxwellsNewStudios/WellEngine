#pragma once
#include "Behaviour.h"
#include "Behaviours/BreadcrumbBehaviour.h"
#include "Behaviours/CompassBehaviour.h"
#include "Behaviours/PictureBehaviour.h"

enum class HandState
{
	Empty,
	Breadcrumbs,
	Compass,
	PicturePiece,

	Count
};

// Behaviour for the player
class [[register_behaviour]] InventoryBehaviour : public Behaviour
{
private:
	HandState _handState = HandState::Empty;

	UINT _breadcrumbCount = 15;
	BreadcrumbColor _breadcrumbColor = BreadcrumbColor::Red;
	
	MeshBehaviour *_heldBreadcrumb = nullptr;
	CompassBehaviour *_compass = nullptr;
	Behaviour *_heldItem = nullptr;

	InteractorBehaviour *_interactor = nullptr;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

public:
	InventoryBehaviour() = default;
	~InventoryBehaviour() = default;

	void AddBreadcrumbs(UINT count) { _breadcrumbCount += count; }

	void SetBreadcrumbColor(BreadcrumbColor color);

	bool SetInventoryItemParents(Entity* parent);
	void SetHeldItem(bool cycle = true, HandState handState = HandState::Empty);
	void SetHeldEnabled(bool state);

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};

