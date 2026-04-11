#pragma once
#include "JoltManager.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
JPH_SUPPRESS_WARNING_POP
#include "JoltListeners.h"


class JoltPhysicsInstance
{
private:
	struct JoltSystemData
	{
		//JPH::TempAllocatorImpl					tempAllocator;
		JPH::TempAllocatorImplWithMallocFallback	tempAllocator;
		JPH::JobSystemThreadPool					jobSystem;
		JPH::PhysicsSystem							physicsSystem;
		JPH::MyBodyActivationListener				bodyActivationListener;
		JPH::MyContactListener						contactListener;

		JoltSystemData(uint32_t maxJobs, uint32_t maxBarriers, uint32_t numThreads);
	};

	std::unique_ptr<JoltSystemData> _sys;

public:
	JoltPhysicsInstance() = default;
	~JoltPhysicsInstance() = default;

	[[nodiscard]] bool Initialize(JoltManager *manager);

	[[nodiscard]] JPH::BodyInterface &GetBodyInterface() { return _sys->physicsSystem.GetBodyInterface(); }
	[[nodiscard]] JPH::BodyInterface &GetBodyInterfaceNoLock() { return _sys->physicsSystem.GetBodyInterfaceNoLock(); }

	// For advanced use. GetBodyInterface() should be enough for most use cases.
	[[nodiscard]] JPH::PhysicsSystem &GetSystem() { return _sys->physicsSystem; }
};