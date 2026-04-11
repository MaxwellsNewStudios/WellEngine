#include "stdafx.h"
#include "JoltPhysicsInstance.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltPhysicsInstance::JoltSystemData::JoltSystemData(JPH::uint maxJobs, JPH::uint maxBarriers, JPH::uint numThreads) :
	tempAllocator(10 * 1024 * 1024), 
	jobSystem(maxJobs, maxBarriers, numThreads) 
{

}

bool JoltPhysicsInstance::Initialize(JoltManager *manager)
{
	if (manager == nullptr)
	{
		ErrMsg("Failed to initialize JoltPhysicsInstance: JoltManager is null!");
		return false;
	}

	_sys = std::make_unique<JoltSystemData>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, max(std::thread::hardware_concurrency() - 2, 1));

	_sys->physicsSystem.Init(
		manager->cMaxBodies, manager->cNumBodyMutexes, manager->cMaxBodyPairs, manager->cMaxContactConstraints,
		manager->GetBroadPhaseLayerInterface(),
		manager->GetObjectVsBroadPhaseLayerFilter(),
		manager->GetObjectVsObjectLayerFilter()
	);

	_sys->physicsSystem.SetBodyActivationListener(&_sys->bodyActivationListener);
	_sys->physicsSystem.SetContactListener(&_sys->contactListener);

	return true;
}

bool JoltPhysicsInstance::Update(float deltaTime)
{
	int cCollisionSteps = int(ceilf(deltaTime / (1.0f / 60.0f)));

	// Step the world
	JPH::EPhysicsUpdateError err = _sys->physicsSystem.Update(deltaTime, cCollisionSteps, &(_sys->tempAllocator), &(_sys->jobSystem));
	if (err != JPH::EPhysicsUpdateError::None)
	{
		ErrMsgF("Failed to update physics system: {}", (int)err);
		return false;
	}

	return true;
}
