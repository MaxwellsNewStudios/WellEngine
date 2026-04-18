#pragma once
#include "Jolt/Jolt.h"
JPH_SUPPRESS_WARNING_PUSH
#include "Jolt/Physics/PhysicsSystem.h"
JPH_SUPPRESS_WARNING_POP


namespace JPH
{
	namespace Layers
	{
		static constexpr ObjectLayer NON_MOVING = 0;
		static constexpr ObjectLayer MOVING = 1;
		static constexpr ObjectLayer SENSOR = 2;
		static constexpr ObjectLayer NUM_LAYERS = 3;
	};

	namespace BroadPhaseLayers
	{
		static constexpr BroadPhaseLayer NON_MOVING(0);
		static constexpr BroadPhaseLayer MOVING(1);
		static constexpr BroadPhaseLayer SENSOR(2);
		static constexpr uint NUM_LAYERS(3);
	};

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
			mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::SENSOR;
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
}


class JoltManager
{
private:
	JPH::BPLayerInterfaceImpl				_broadPhaseLayerInterface;
	JPH::ObjectVsBroadPhaseLayerFilterImpl	_objectVsBroadPhaseLayerFilter;
	JPH::ObjectLayerPairFilterImpl			_objectVsObjectLayerFilter;

public:
	const JPH::uint cMaxBodies = 65536;
	const JPH::uint cNumBodyMutexes = 0;
	const JPH::uint cMaxBodyPairs = 65536;
	const JPH::uint cMaxContactConstraints = 10240;

	JoltManager();
	~JoltManager();

	[[nodiscard]] JPH::BPLayerInterfaceImpl &GetBroadPhaseLayerInterface() { return _broadPhaseLayerInterface; }
	[[nodiscard]] JPH::ObjectVsBroadPhaseLayerFilterImpl &GetObjectVsBroadPhaseLayerFilter() { return _objectVsBroadPhaseLayerFilter; }
	[[nodiscard]] JPH::ObjectLayerPairFilterImpl &GetObjectVsObjectLayerFilter() { return _objectVsObjectLayerFilter; }
};