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

	JPH::EMotionType _motionType = JPH::EMotionType::Dynamic;
	JPH::ObjectLayer _layer = JPH::Layers::MOVING;
	float _friction = 0.2f;
	float _gravityFactor = 1.0f;
	float _restitution = 0.2f;

	dx::XMFLOAT3A _lastEntPos = { 0, 0, 0 };
	dx::XMFLOAT4A _lastEntRot = { 0, 0, 0, 1 };
	dx::XMFLOAT3A _lastEntScale = { 1, 1, 1 };

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

	void DestroyBody();
	
	// Recalculate the physics body proportions 
	virtual void RecalculatePhysicsBody() = 0;
	
	// Apply entity transform to physics body
	virtual void SyncPhysics() = 0; 
	
	// Apply physics body transform to entity
	virtual void SyncTransform() = 0; 

public:
	JoltColliderBehaviour();
	JoltColliderBehaviour(JPH::EMotionType motionType, JPH::ObjectLayer layer, float friction, float gravityFactor, float restitution);
	~JoltColliderBehaviour();

	[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene) override;
	virtual void PostDeserialize() override;

	[[nodiscard]] const JPH::BodyID &GetBodyID() const { return _bodyID; }
	[[nodiscard]] JPH::EMotionType GetMotionType() const { return _motionType; }
	[[nodiscard]] JPH::ObjectLayer GetLayer() const { return _layer; }

	void SetMotionType(JPH::EMotionType motionType);
	void SetLayer(JPH::ObjectLayer layer);
};
