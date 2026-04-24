#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "Game/Behaviour.h"
#include "Engine/Content/Content.h"
#include "Engine/Physics/JoltManager.h"

namespace WellEngine
{
	// Abstract base class for Jolt colliders
	class B_Collider : public Behaviour
	{
	private:
		JPH::BodyID _bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);

		JPH::EMotionType _motionType = JPH::EMotionType::Dynamic;
		JPH::ObjectLayer _layer = JPH::Layers::MOVING;
		float _friction = 0.5f;
		float _gravityFactor = 1.0f;
		float _restitution = 0.3f;

		bool _resampleGoal = false;
		float _lerpTime = 0.0f;
		dx::XMFLOAT3A _lastPosLerpGoal = { 0, 0, 0 };
		dx::XMFLOAT4A _lastRotLerpGoal = { 0, 0, 0, 1 };
		dx::XMFLOAT3A _posLerpGoal = { 0, 0, 0 };
		dx::XMFLOAT4A _rotLerpGoal = { 0, 0, 0, 1 };

		dx::XMFLOAT3A _lastEntScale = { 1, 1, 1 };


	#ifdef USE_IMGUI
		bool _debugDraw = false;
		bool _debugDrawInterpolation = false;
	#endif

	protected:
		[[nodiscard]] virtual bool Start() override;
		[[nodiscard]] virtual bool Update(TimeUtils &time, const Input &input) override;
		[[nodiscard]] virtual bool LateUpdate(TimeUtils &time, const Input &input) override;
		[[nodiscard]] virtual bool PhysicsUpdate(float deltaTime) override;
	#ifdef USE_IMGUI
		[[nodiscard]] virtual bool RenderUI() override;
		[[nodiscard]] bool DoDebugDraw() const { return _debugDraw; }
	#endif

		virtual void OnEnable() override;
		virtual void OnDisable() override;

		[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
		[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene) override;
		[[nodiscard]] virtual bool PostDeserialize() override;

		[[nodiscard]] const JPH::BodyInterface &GetBodyInterface() const;
		[[nodiscard]] JPH::BodyInterface &GetBodyInterface();

		void SetBodyID(const JPH::BodyID &bodyID) { _bodyID = bodyID; }
		void DestroyBody();
	
		// Calculate physics body position & rotation in world-space
		virtual void CalcBodyLocation(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot) = 0; 
	
		// Recalculate the physics body proportions 
		virtual void RecalculatePhysicsBody() = 0;
	
		// Apply entity transform to physics body
		virtual void SyncPhysics() = 0; 
	
		// Apply physics body transform to entity
		virtual void SyncTransform() = 0; 

		void SetLerpGoal(const dx::XMFLOAT3A &posGoal, const dx::XMFLOAT4A &rotGoal) { _posLerpGoal = posGoal; _rotLerpGoal = rotGoal; }

		void CalcLerp(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot) const;

		[[nodiscard]] virtual bool OnEditTransformRec() override;

	public:
		B_Collider();
		B_Collider(JPH::EMotionType motionType, JPH::ObjectLayer layer, float friction, float gravityFactor, float restitution);
		~B_Collider();

		[[nodiscard]] inline const JPH::BodyID &GetBodyID() const	{ return _bodyID; }
		[[nodiscard]] inline JPH::EMotionType GetMotionType() const	{ return _motionType; }
		[[nodiscard]] inline JPH::ObjectLayer GetLayer() const		{ return _layer; }
		[[nodiscard]] inline float GetFriction() const				{ return _friction; }
		[[nodiscard]] inline float GetGravityFactor() const			{ return _gravityFactor; }
		[[nodiscard]] inline float GetRestitution() const			{ return _restitution; }
		[[nodiscard]] float GetMass() const;
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
}
