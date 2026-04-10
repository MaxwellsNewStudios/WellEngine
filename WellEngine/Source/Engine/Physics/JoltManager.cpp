#include "stdafx.h"
#include "JoltManager.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


static void TraceImpl(const char *inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print
	DbgMsg(buffer);
}
JPH::TraceFunction JPH::Trace = TraceImpl;

#ifdef JPH_ENABLE_ASSERTS
// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint32_t inLine)
{
	ErrMsgF("{}:{}: ({}) {}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
	return true;
}
JPH::AssertFailedFunction JPH::AssertFailed = AssertFailedImpl;
#endif // JPH_ENABLE_ASSERTS


namespace JPH
{
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
		default:													JPH_ASSERT(false); 
			return "INVALID";
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
}


JoltManager::JoltData::JoltData(uint32_t maxJobs, uint32_t maxBarriers, uint32_t numThreads) : jobSystem(maxJobs, maxBarriers, numThreads) { }

JoltManager::JoltManager()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::RegisterDefaultAllocator();

	JPH::Factory::sInstance = new JPH::Factory();

	_d = std::make_unique<JoltData>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 2);

	JPH::RegisterTypes();
}

JoltManager::~JoltManager()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::UnregisterTypes();

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
}

bool JoltManager::Initialize()
{
	ZoneScopedC(RandomUniqueColor());

	_d->physicsSystem.Init(
		cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, 
		_d->broadPhaseLayerInterface, _d->objectVsBroadPhaseLayerFilter, _d->objectVsObjectLayerFilter
	);

	return true;
}
