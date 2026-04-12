#include "stdafx.h"
#include "JoltSphereColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltSphereColliderBehaviour::JoltSphereColliderBehaviour(float radius, JPH::EMotionType motionType, JPH::ObjectLayer layer) :
	JoltColliderBehaviour(motionType, layer), _radius(radius)
{ }

bool JoltSphereColliderBehaviour::Start()
{
	if (_name.empty())
		_name = "JoltSphereColliderBehaviour"; // For categorization in ImGui.

	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A wPos = transform->GetPosition(World);
	dx::XMFLOAT4A wRot = transform->GetRotation(World);
	dx::XMFLOAT3A scale = transform->GetScale();

	// Get the maximum scale component to apply to the radius
	float maxScale = max(scale.x, max(scale.y, scale.z));

	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	JPH::BodyCreationSettings sphereSettings(
		new JPH::SphereShape(_radius * maxScale),
		JPH::RVec3(wPos.x, wPos.y, wPos.z), 
		JPH::Quat(wRot.x, wRot.y, wRot.z, wRot.w),
		GetMotionType(), GetLayer()
	);
	SetBodyID(bodyInterface.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate));

	return JoltColliderBehaviour::Start();
}

bool JoltSphereColliderBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (!JoltColliderBehaviour::Update(time, input))
		return false;

#ifdef USE_IMGUI
	if (DoDebugDraw())
	{
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(GetBodyID());
		dx::XMFLOAT3A pos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
		float joltRadius = ((JPH::SphereShape *)(bodyInterface.GetShape(GetBodyID()).GetPtr()))->GetRadius();

		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawSphere(pos, joltRadius, 3, dx::XMFLOAT4(1, 0, 0, 0.2f));
	}
#endif

	return true;
}

void JoltSphereColliderBehaviour::RecalculatePhysicsBody()
{
	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A scale = transform->GetScale();

	// Get the maximum scale component to apply to the radius
	float maxScale = max(scale.x, max(scale.y, scale.z));

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();
	bodyInterface.SetShape(bodyID, new JPH::SphereShape(_radius * maxScale), false, JPH::EActivation::Activate);
}
void JoltSphereColliderBehaviour::SyncPhysics()
{
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);

	bodyInterface.SetPosition(bodyID, JPH::RVec3(currPos.x, currPos.y, currPos.z), JPH::EActivation::Activate);
	bodyInterface.SetRotation(bodyID, JPH::Quat(currRot.x, currRot.y, currRot.z, currRot.w), JPH::EActivation::Activate);
}
void JoltSphereColliderBehaviour::SyncTransform()
{
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(bodyID);
	JPH::Quat joltRot = bodyInterface.GetRotation(bodyID);

	dx::XMFLOAT3A newEntPos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
	dx::XMFLOAT4A newEntRot = dx::XMFLOAT4A(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());

	transform->SetPosition(newEntPos, World);
	transform->SetRotation(newEntRot, World);
}

#ifdef USE_IMGUI
bool JoltSphereColliderBehaviour::RenderUI()
{
	if (!JoltColliderBehaviour::RenderUI())
		return false;

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();

	// Radius
	if (ImGui::DragFloat("Radius", &_radius, 0.01f))
	{
		_radius = max(0.001f, _radius);
		RecalculatePhysicsBody();
	}
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif
