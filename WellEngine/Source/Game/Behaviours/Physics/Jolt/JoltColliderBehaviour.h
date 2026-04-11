#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

#include "Source/Game/Behaviour.h"
#include "Source/Engine/Content/Content.h"
#include "Source/Engine/Physics/JoltManager.h"

// Abstract base class for Jolt colliders
class JoltColliderBehaviour : public Behaviour
{
private:
	JPH::BodyID _bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
	JPH::EMotionType _type;
	JPH::ObjectLayer _layer;

	dx::XMFLOAT3A _lastEntPos = { 0, 0, 0 };
	dx::XMFLOAT4A _lastEntRot = { 0, 0, 0, 1 };

#ifdef USE_IMGUI
	bool _debugDraw = false;
#endif

protected:
	[[nodiscard]] virtual bool Start() override;
	[[nodiscard]] virtual bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] virtual bool LateUpdate(TimeUtils &time, const Input &input) override;
#ifdef USE_IMGUI
	[[nodiscard]] virtual bool RenderUI() override;
	[[nodiscard]] bool DoDebugDraw() const { return _debugDraw; }
#endif

	[[nodiscard]] JPH::BodyInterface &GetBodyInterface();

	void SetBodyID(const JPH::BodyID &bodyID) { _bodyID = bodyID; }
	void SetMotionType(JPH::EMotionType type) { _type = type; }
	void SetLayer(JPH::ObjectLayer layer) { _layer = layer; }

	void DestroyBody();

public:
	JoltColliderBehaviour(JPH::EMotionType type = JPH::EMotionType::Dynamic, JPH::ObjectLayer layer = JPH::Layers::MOVING);
	~JoltColliderBehaviour();

	[[nodiscard]] const JPH::BodyID &GetBodyID() const { return _bodyID; }
	[[nodiscard]] JPH::EMotionType GetMotionType() const { return _type; }
	[[nodiscard]] JPH::ObjectLayer GetLayer() const { return _layer; }
};
