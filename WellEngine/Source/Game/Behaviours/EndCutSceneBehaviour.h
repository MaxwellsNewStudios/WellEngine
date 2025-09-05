#pragma once

#include "Behaviours/InteractableBehaviour.h"
#include "Behaviour.h"

class [[register_behaviour]] EndCutSceneBehaviour : public Behaviour
{
private:
	const std::string _cutSceneName = "endscene5";

	Transform* _objectTransform = nullptr;
	Entity* _playerEntity = nullptr;

	bool _cutSceneFinished = false;
	bool _startCutScene = false;

	//BillboardMeshBehaviour* _flare = nullptr;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input& input) override;

public:

	void CutScene();
	void Hide() = delete;
	bool IsHiding() const = delete;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};
