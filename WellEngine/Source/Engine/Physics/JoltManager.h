#pragma once
#include "Jolt/Jolt.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/RegisterTypes.h"
#include "Jolt/Core/Factory.h"
#include "Jolt/Core/TempAllocator.h"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSettings.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/SphereShape.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Body/BodyActivationListener.h"
JPH_SUPPRESS_WARNING_POP

namespace JPH
{
	namespace Layers
	{
		static constexpr ObjectLayer NON_MOVING = 0;
		static constexpr ObjectLayer MOVING = 1;
		static constexpr ObjectLayer NUM_LAYERS = 2;
	};

	namespace BroadPhaseLayers
	{
		static constexpr BroadPhaseLayer NON_MOVING(0);
		static constexpr BroadPhaseLayer MOVING(1);
		static constexpr uint NUM_LAYERS(2);
	};

	/*
	/// Class that determines if two object layers can collide
	class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override;
	};

	class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	/// Class that determines if an object layer can collide with a broadphase layer
	class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override;
	};
	*/
}

class JoltManager
{
private:
	//JPH::TempAllocatorMalloc _tempAllocator;
	//JPH::JobSystemThreadPool _jobSystem;
	//JPH::BPLayerInterfaceImpl _broadPhaseLayerInterface;
	//JPH::ObjectVsBroadPhaseLayerFilterImpl _objectVsBroadPhaseLayerFilter;
	//JPH::ObjectLayerPairFilterImpl _objectVsObjectLayerFilter;
	//JPH::PhysicsSystem _physicsSystem;

public:
	const uint32_t cMaxBodies = 65536u;
	const uint32_t cNumBodyMutexes = 0u;
	const uint32_t cMaxBodyPairs = 65536u;
	const uint32_t cMaxContactConstraints = 10240u;

	JoltManager();
	~JoltManager();

	[[nodiscard]] bool Initialize();
};