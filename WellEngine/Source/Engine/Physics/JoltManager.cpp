#include "stdafx.h"
#include "JoltManager.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

namespace JPH
{
	/*
	static void TraceImpl(const char *inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);

		// Print to the TTY
		DbgMsg(buffer);
	}

#ifdef JPH_ENABLE_ASSERTS
	// Callback for asserts, connect this to your own assert handler if you have one
	static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
	{
		// Print to the TTY
		ErrMsgF("{}:{}: ({}) {}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");

		// Breakpoint
		return true;
	};
#endif // JPH_ENABLE_ASSERTS

	bool ObjectLayerPairFilterImpl::ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}

	BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(ObjectLayer inLayer) const
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	const char *BPLayerInterfaceImpl::GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const
	{
		switch ((BroadPhaseLayer::Type)inLayer)
		{
		case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		default:													JPH_ASSERT(false); return "INVALID";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
	*/
}


JoltManager::JoltManager() //: _jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 2)
{
}

JoltManager::~JoltManager()
{
	JPH::UnregisterTypes();

	/*
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	*/
}

bool JoltManager::Initialize()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::RegisterDefaultAllocator();

	/*
	JPH::Trace = JPH::TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JPH::AssertFailedImpl;)

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	_physicsSystem.Init(
		cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, 
		_broadPhaseLayerInterface, _objectVsBroadPhaseLayerFilter, _objectVsObjectLayerFilter
	);
	*/

	return true;
}
