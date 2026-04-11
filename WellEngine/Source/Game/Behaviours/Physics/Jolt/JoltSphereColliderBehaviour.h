#pragma once
#include "JoltColliderBehaviour.h"

class [[register_behaviour]] JoltSphereColliderBehaviour final : public JoltColliderBehaviour
{
private:
	float _radius;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void SyncPhysics() override;
	void SyncTransform() override;

public:
	JoltSphereColliderBehaviour(float radius = 1.0f, JPH::EMotionType motionType = JPH::EMotionType::Dynamic, JPH::ObjectLayer layer = JPH::Layers::MOVING);
};

