#pragma once
#include "Behaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"

class FlashlightBehaviour;

enum class BreadcrumbColor
{
	Red,
	Green,
	Blue
};

class [[register_behaviour]] BreadcrumbBehaviour : public Behaviour
{
private:
	BillboardMeshBehaviour *_flare = nullptr;
	BreadcrumbColor _color = BreadcrumbColor::Red;
	FlashlightBehaviour *_flashlight = nullptr;

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

public:
	BreadcrumbBehaviour() = default;
	~BreadcrumbBehaviour() = default;

	BreadcrumbBehaviour(BreadcrumbColor color);

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};