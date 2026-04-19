#pragma once

#include "JoltColliderBehaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] SphereJoltColliderBehaviour final : public JoltColliderBehaviour
	{
	private:
		float _radius = 1.0f;

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
		SphereJoltColliderBehaviour(float radius = 1.0f);
		SphereJoltColliderBehaviour(float radius, 
			JPH::EMotionType motionType, JPH::ObjectLayer layerG, 
			float friction, float gravityFactor, float restitution);

		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
	};
}
