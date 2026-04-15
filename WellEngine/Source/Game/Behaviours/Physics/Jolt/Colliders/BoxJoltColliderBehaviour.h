#pragma once
#include "JoltColliderBehaviour.h"

class [[register_behaviour]] BoxJoltColliderBehaviour final : public JoltColliderBehaviour
{
private:
	dx::XMFLOAT3A 
		_halfExtents = { 1, 1, 1 }, 
		_offset = { 0, 0, 0 };

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void CalcBodyLocation(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot) override;
	void RecalculatePhysicsBody() override;
	void SyncPhysics() override;
	void SyncTransform() override;

public:
	BoxJoltColliderBehaviour(const dx::XMFLOAT3A &halfExtents = { 1, 1, 1 }, const dx::XMFLOAT3A &offset = { 0, 0, 0 });
	BoxJoltColliderBehaviour(const dx::XMFLOAT3A &halfExtents, const dx::XMFLOAT3A &offset,
		JPH::EMotionType motionType, JPH::ObjectLayer layer,
		float friction, float gravityFactor, float restitution);

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};
