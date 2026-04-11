#pragma once
#include "JoltColliderBehaviour.h"

class [[register_behaviour]] JoltSphereColliderBehaviour final : public JoltColliderBehaviour
{
private:
	float _radius;
	dx::XMFLOAT3 _offset;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

public:
	JoltSphereColliderBehaviour(float radius = 0.5f, dx::XMFLOAT3 offset = {0,0,0}, JPH::EMotionType type = JPH::EMotionType::Dynamic, JPH::ObjectLayer layer = JPH::Layers::MOVING);
	~JoltSphereColliderBehaviour();
};

