#pragma once
#include "JoltColliderBehaviour.h"

class [[register_behaviour]] JoltBoxColliderBehaviour final : public JoltColliderBehaviour
{
private:
	dx::XMFLOAT3A _halfExtents, _offset;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void SyncPhysics() override;
	void SyncTransform() override;

public:
	JoltBoxColliderBehaviour(const dx::XMFLOAT3A &halfExtents = {1,1,1}, const dx::XMFLOAT3A &offset = {}, JPH::EMotionType motionType = JPH::EMotionType::Dynamic, JPH::ObjectLayer layer = JPH::Layers::MOVING);
};
