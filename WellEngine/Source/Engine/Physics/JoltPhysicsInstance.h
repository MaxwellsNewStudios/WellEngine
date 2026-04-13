#pragma once
#include "JoltManager.h"
#include "JoltListeners.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"


// TODO: 
// - Fix issue where if physics update takes longer than physics time-step, the amount of physics updates per frame increases infinitely, causing massive stutter.
// - More collider shape behaviours (capsule, cylinder, triangle, convexHull, mesh, heightField, compound)
// - Separate colliders and rigidbodies (currently the collider behaviour also creates a rigidbody)
// - Collider events (enter, stay, exit, sleep, wake)
// - Scene-wide physics settings UI
// - Scene-wide raycast & intersection queries
// 

class JoltPhysicsInstance
{
private:
	struct JoltSystemData
	{
		//JPH::TempAllocatorImpl					tempAllocator;
		JPH::TempAllocatorImplWithMallocFallback	tempAllocator;
		JPH::JobSystemThreadPool					jobSystem; // TODO: Replace with custom job system
		JPH::PhysicsSystem							physicsSystem;
		JPH::MyBodyActivationListener				bodyActivationListener;
		JPH::MyContactListener						contactListener;

		JoltSystemData(JPH::uint maxJobs, JPH::uint maxBarriers, JPH::uint numThreads);
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

	[[nodiscard]] bool Update(float deltaTime);
};