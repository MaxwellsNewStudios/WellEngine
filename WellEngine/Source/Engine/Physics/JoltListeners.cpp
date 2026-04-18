#include "stdafx.h"
JPH_SUPPRESS_WARNING_PUSH
#include "JoltListeners.h"
JPH_SUPPRESS_WARNING_POP

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JPH::ValidateResult JPH::MyContactListener::OnContactValidate(const Body &inBody1, const Body &inBody2, RVec3Arg inBaseOffset, const CollideShapeResult &inCollisionResult)
{
	// Contact validate callback
	// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
	return ValidateResult::AcceptAllContactsForThisBodyPair;
}

void JPH::MyContactListener::OnContactAdded(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings)
{
	// A contact was added
}

void JPH::MyContactListener::OnContactPersisted(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, ContactSettings &ioSettings)
{
	// A contact was persisted
}

void JPH::MyContactListener::OnContactRemoved(const SubShapeIDPair &inSubShapePair)
{
	// A contact was removed
}


void JPH::MyBodyActivationListener::OnBodyActivated(const BodyID &inBodyID, uint64 inBodyUserData)
{
	// A body got activated
}

void JPH::MyBodyActivationListener::OnBodyDeactivated(const BodyID &inBodyID, uint64 inBodyUserData)
{
	// A body went to sleep
}
