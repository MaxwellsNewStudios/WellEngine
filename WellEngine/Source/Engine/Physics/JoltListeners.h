#pragma once
#include "Jolt/Jolt.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
JPH_SUPPRESS_WARNING_POP

namespace JPH
{
	class MyContactListener : public ContactListener
	{
	public:
		// See: ContactListener
		virtual ValidateResult	OnContactValidate(const Body &inBody1, const Body &inBody2, RVec3Arg inBaseOffset, const CollideShapeResult &inCollisionResult) override;

		virtual void OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override;

		virtual void OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings) override;

		virtual void OnContactRemoved(const SubShapeIDPair &inSubShapePair) override;
	};

	class MyBodyActivationListener : public BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData) override;

		virtual void OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData) override;
	};
}
