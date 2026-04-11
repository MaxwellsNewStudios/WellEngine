#include "stdafx.h"
#include "JoltColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltColliderBehaviour::JoltColliderBehaviour(JPH::EMotionType type, JPH::ObjectLayer layer) :
	_type(type), _layer(layer)
{ }

JoltColliderBehaviour::~JoltColliderBehaviour()
{
	// If quitting, just return
	if (GetScene()->IsDestroyed())
		return;

	// if body is valid, remove it from the physics system
	if (!_bodyID.IsInvalid())
		DestroyBody();
}

bool JoltColliderBehaviour::Start()
{
	QueueUpdate();
	QueueLateUpdate();

	return true;
}

bool JoltColliderBehaviour::Update(TimeUtils &time, const Input &input)
{
	return true;
}

bool JoltColliderBehaviour::LateUpdate(TimeUtils &time, const Input &input)
{
	// If entity transform has moved or rotated since the last frame, apply the new transform to the physics body.
	// Otherwise, apply the current physics body transform to the entity to keep them in sync.

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();

	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);

	if (currPos.x != _lastEntPos.x || currPos.y != _lastEntPos.y || currPos.z != _lastEntPos.z ||
		currRot.x != _lastEntRot.x || currRot.y != _lastEntRot.y || currRot.z != _lastEntRot.z || currRot.w != _lastEntRot.w)
	{
		// Entity transform has changed, apply it to the physics body.
		bodyInterface.SetPosition(_bodyID, JPH::RVec3(currPos.x, currPos.y, currPos.z), JPH::EActivation::DontActivate);
		bodyInterface.SetRotation(_bodyID, JPH::Quat(currRot.x, currRot.y, currRot.z, currRot.w), JPH::EActivation::DontActivate);

		_lastEntPos = currPos;
		_lastEntRot = currRot;
	}
	else
	{
		// Entity transform has not changed, apply physics body transform to entity.
		JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(_bodyID);
		JPH::Quat joltRot = bodyInterface.GetRotation(_bodyID);

		dx::XMFLOAT3A newEntPos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
		dx::XMFLOAT4A newEntRot = dx::XMFLOAT4A(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());

		transform->SetPosition(newEntPos, World);
		transform->SetRotation(newEntRot, World);

		_lastEntPos = newEntPos;
		_lastEntRot = newEntRot;
	}

	return true;
}

#ifdef USE_IMGUI
bool JoltColliderBehaviour::RenderUI()
{
	ImGui::Checkbox("Debug Draw", &_debugDraw);

	// Motion type
	const char *motionTypes[] = { "Static", "Kinematic", "Dynamic" };
	int motionTypeIndex = (int)GetMotionType();
	if (ImGui::Combo("Motion Type", &motionTypeIndex, motionTypes, IM_ARRAYSIZE(motionTypes)))
	{
		SetMotionType((JPH::EMotionType)motionTypeIndex);

		// Update body motion type in physics system
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetMotionType(_bodyID, GetMotionType(), JPH::EActivation::DontActivate);
	}

	// TODO: Layer

	return true;
}
#endif

[[nodiscard]] JPH::BodyInterface &JoltColliderBehaviour::GetBodyInterface() 
{ 
	return GetScene()->GetPhysicsInstance()->GetBodyInterface(); 
}

void JoltColliderBehaviour::DestroyBody()
{
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.RemoveBody(_bodyID);
	bodyInterface.DestroyBody(_bodyID);
	_bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
}