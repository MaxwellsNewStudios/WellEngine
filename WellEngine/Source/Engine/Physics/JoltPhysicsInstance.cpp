#include "stdafx.h"
#include "JoltPhysicsInstance.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltPhysicsInstance::JoltSystemData::JoltSystemData(JPH::uint maxJobs, JPH::uint maxBarriers, JPH::uint numThreads) :
	tempAllocator(10 * 1024 * 1024), 
	jobSystem(maxJobs, maxBarriers, numThreads) 
{ }

bool JoltPhysicsInstance::Initialize(JoltManager *manager)
{
	if (manager == nullptr)
	{
		ErrMsg("Failed to initialize JoltPhysicsInstance: JoltManager is null!");
		return false;
	}

#ifndef _DEPLOY
	_paused = true;
#endif

	_settings.mBaumgarte = 0.5f;

	_sys = std::make_unique<JoltSystemData>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, max(std::thread::hardware_concurrency() - 2, 1));

	_sys->physicsSystem.Init(
		manager->cMaxBodies, manager->cNumBodyMutexes, manager->cMaxBodyPairs, manager->cMaxContactConstraints,
		manager->GetBroadPhaseLayerInterface(),
		manager->GetObjectVsBroadPhaseLayerFilter(),
		manager->GetObjectVsObjectLayerFilter()
	);

	_sys->physicsSystem.SetBodyActivationListener(&_sys->bodyActivationListener);
	_sys->physicsSystem.SetContactListener(&_sys->contactListener);

	_sys->physicsSystem.SetGravity(JPH::Vec3(_gravity.x, _gravity.y, _gravity.z));
	_sys->physicsSystem.SetPhysicsSettings(_settings);

	return true;
}

bool JoltPhysicsInstance::Update(float deltaTime)
{
	if (_paused)
		return true;

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


void JoltPhysicsInstance::SetGravity(const dx::XMFLOAT3 &gravity)
{
	_gravity = gravity;
	_sys->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
}


#ifdef USE_IMGUI
bool JoltPhysicsInstance::RenderUI()
{
	ImGui::Checkbox("Pause Simulation", &_paused);

	if (ImGui::Button("Wake All Bodies"))
	{
		JPH::BodyIDVector activeBodies;
		_sys->physicsSystem.GetBodies(activeBodies);

		JPH::BodyInterface &bodyInterface = _sys->physicsSystem.GetBodyInterface();

		for (const JPH::BodyID &id : activeBodies)
			bodyInterface.ActivateBody(id);
	}

	if (ImGui::DragFloat3("Gravity", &_gravity.x, 0.01f))
		_sys->physicsSystem.SetGravity(JPH::Vec3(_gravity.x, _gravity.y, _gravity.z));
	ImGuiUtils::LockMouseOnActive();

	if (ImGui::TreeNode("Physics Settings"))
	{
		bool changed = false;

		changed |= ImGui::SliderFloat("Baumgarte", &_settings.mBaumgarte, 0.0f, 1.0f);

		changed |= ImGui::DragFloat("Speculative Contact Distance", &_settings.mSpeculativeContactDistance, 0.001f, 0.0f);
		ImGuiUtils::LockMouseOnActive();

		changed |= ImGui::DragFloat("Penetration Slop", &_settings.mPenetrationSlop, 0.001f, 0.0f);
		ImGuiUtils::LockMouseOnActive();

		changed |= ImGui::DragFloat("Linear Cast Threshold", &_settings.mLinearCastThreshold, 0.01f, 0.0f);
		ImGuiUtils::LockMouseOnActive();

		changed |= ImGui::DragFloat("Time Before Sleep", &_settings.mTimeBeforeSleep, 0.1f, 0.0f);
		ImGuiUtils::LockMouseOnActive();

		changed |= ImGui::Checkbox("Deterministic Simulation", &_settings.mDeterministicSimulation);

		changed |= ImGui::Checkbox("Allow Sleeping", &_settings.mAllowSleeping);

		if (changed)
			_sys->physicsSystem.SetPhysicsSettings(_settings);

		ImGui::TreePop();
	}

	return true;
}
#endif
