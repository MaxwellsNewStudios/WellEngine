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

	virtual void OnEnable() override;
	virtual void OnDisable() override;

	[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene) override;
	virtual void PostDeserialize() override;

	[[nodiscard]] const JPH::BodyInterface &GetBodyInterface() const;
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

	[[nodiscard]] inline const JPH::BodyID &GetBodyID() const	{ return _bodyID; }
	[[nodiscard]] inline JPH::EMotionType GetMotionType() const	{ return _motionType; }
	[[nodiscard]] inline JPH::ObjectLayer GetLayer() const		{ return _layer; }
	[[nodiscard]] inline float GetFriction() const				{ return _friction; }
	[[nodiscard]] inline float GetGravityFactor() const			{ return _gravityFactor; }
	[[nodiscard]] inline float GetRestitution() const			{ return _restitution; }
	[[nodiscard]] JPH::EBodyType GetBodyType() const;
	[[nodiscard]] dx::XMFLOAT3 GetCenterOfMass() const;
	[[nodiscard]] dx::XMFLOAT3 GetLinearVelocity() const;
	[[nodiscard]] dx::XMFLOAT3 GetAngularVelocity() const;
	[[nodiscard]] dx::XMFLOAT3 GetPointVelocity(const dx::XMFLOAT3 &point) const; // World space
	[[nodiscard]] bool IsBodyActive() const;
	[[nodiscard]] bool IsSensor() const;

	void SetBodyActive(bool active);
	void SetMotionType(JPH::EMotionType motionType);
	void SetLayer(JPH::ObjectLayer layer);
	void SetFriction(float friction);
	void SetGravityFactor(float gravityFactor);
	void SetRestitution(float restitution);
	void SetPosition(const dx::XMFLOAT3 &position);
	void SetRotation(const dx::XMFLOAT4 &rotation);
	void SetLinearVelocity(const dx::XMFLOAT3 &velocity);
	void SetAngularVelocity(const dx::XMFLOAT3 &velocity);
	void SetPosRotVel(const dx::XMFLOAT3 &position, const dx::XMFLOAT4 &rotation, const dx::XMFLOAT3 &linVel, const dx::XMFLOAT3 &angVel);

	void AddLinearVelocity(const dx::XMFLOAT3 &velocity);
	void AddAngularVelocity(const dx::XMFLOAT3 &velocity);
	void AddForce(const dx::XMFLOAT3 &force);
	void AddForce(const dx::XMFLOAT3 &force, const dx::XMFLOAT3 &point);
	void AddTorque(const dx::XMFLOAT3 &torque);
	void AddImpulse(const dx::XMFLOAT3 &impulse);
	void AddImpulse(const dx::XMFLOAT3 &impulse, const dx::XMFLOAT3 &point);
	void AddAngularImpulse(const dx::XMFLOAT3 &impulse);
	void MoveKinematic(const dx::XMFLOAT3 &position, const dx::XMFLOAT4 &rotation, float time);

};
