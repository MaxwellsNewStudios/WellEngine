#pragma once
#include "Behaviour.h"
#include "Behaviours/MeshBehaviour.h"

class [[register_behaviour]] CompassBehaviour final : public Behaviour
{
private:
	Transform *_compassNeedle = nullptr;

	Entity* _compassBodyMesh = nullptr;
	Entity* _compassNeedleMesh = nullptr;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool LateUpdate(TimeUtils &time, const Input& input) override;
	virtual void OnEnable() override;
	virtual void OnDisable() override;

public:
	CompassBehaviour() = default;
	~CompassBehaviour() = default;
};