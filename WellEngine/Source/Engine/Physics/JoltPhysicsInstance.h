#pragma once

#include <DirectXMath.h>

#include "JoltManager.h"
#include "JoltListeners.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
JPH_SUPPRESS_WARNING_POP

// TODO: 
// - Fix issue where if physics update takes longer than physics time-step, the amount of physics updates per frame increases infinitely, causing massive stutter.
// - More collider shape behaviours (capsule, cylinder, triangle, convexHull, mesh, heightField, compound)
// - Separate colliders and rigidbodies (currently the collider behaviour also creates a rigidbody)
// - Collider events (enter, stay, exit, sleep, wake)
// - Scene-wide raycast & intersection queries
// 

namespace WellEngine
{
	namespace dx = DirectX;

	class JoltPhysicsInstance
	{
	private:
		struct JoltSystemData
		{
			JPH::TempAllocatorImplWithMallocFallback	tempAllocator;
			JPH::JobSystemThreadPool					jobSystem;
			JPH::PhysicsSystem							physicsSystem;
			JPH::MyBodyActivationListener				bodyActivationListener;
			JPH::MyContactListener						contactListener;

			JoltSystemData(JPH::uint maxJobs, JPH::uint maxBarriers, JPH::uint numThreads);
		};
		std::unique_ptr<JoltSystemData> _sys;

		JPH::PhysicsSettings _settings;
		dx::XMFLOAT3 _gravity{ 0, -9.81f, 0 };
		bool _paused = false;

	public:
		JoltPhysicsInstance() = default;
		~JoltPhysicsInstance() = default;

		[[nodiscard]] bool Initialize(JoltManager *manager);

		[[nodiscard]] bool GetPaused() const { return _paused; }
		[[nodiscard]] const dx::XMFLOAT3 &GetGravity() const { return _gravity; }

		void SetPaused(bool state) { _paused = state; }
		void SetGravity(const dx::XMFLOAT3 &gravity);

		[[nodiscard]] JPH::BodyInterface &GetBodyInterface() { return _sys->physicsSystem.GetBodyInterface(); }
		[[nodiscard]] JPH::BodyInterface &GetBodyInterfaceNoLock() { return _sys->physicsSystem.GetBodyInterfaceNoLock(); }

		// For advanced use. GetBodyInterface() should be enough for most use cases.
		[[nodiscard]] JPH::PhysicsSystem &GetSystem() { return _sys->physicsSystem; }

		[[nodiscard]] bool Update(float deltaTime);

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI();
	#endif
	};
}
