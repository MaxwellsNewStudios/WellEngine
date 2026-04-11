#include "stdafx.h"
#include "JoltSphereColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltSphereColliderBehaviour::JoltSphereColliderBehaviour(float radius, dx::XMFLOAT3 offset, JPH::EMotionType type, JPH::ObjectLayer layer) :
	JoltColliderBehaviour(type, layer), _radius(radius), _offset(offset)
{ }

JoltSphereColliderBehaviour::~JoltSphereColliderBehaviour()
{

}

bool JoltSphereColliderBehaviour::Start()
{
	if (_name.empty())
		_name = "JoltSphereColliderBehaviour"; // For categorization in ImGui.

	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A wPos = transform->GetPosition(World);

	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	JPH::BodyCreationSettings sphereSettings(
		new JPH::SphereShape(_radius),
		JPH::RVec3(wPos.x + _offset.x, wPos.y + _offset.y, wPos.z + _offset.z), 
		JPH::Quat::sIdentity(),
		GetMotionType(), GetLayer()
	);
	SetBodyID(bodyInterface.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate));

	QueueUpdate();

	return true;
}

bool JoltSphereColliderBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (!JoltColliderBehaviour::Update(time, input))
		return false;

	// Snap transform to physics body position
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(GetBodyID());
	dx::XMFLOAT3A pos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
	dx::XMFLOAT3A entPos = dx::XMFLOAT3A(pos.x - _offset.x, pos.y - _offset.y, pos.z - _offset.z);
	GetEntity()->GetTransform()->SetPosition(entPos, World);

#ifdef USE_IMGUI
	if (DoDebugDraw())
	{
		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawSphere(pos, _radius, 3, dx::XMFLOAT4(1, 0, 0, 0.2f));
	}
#endif

	return true;
}

#ifdef USE_IMGUI
bool JoltSphereColliderBehaviour::RenderUI()
{
	if (!JoltColliderBehaviour::RenderUI())
		return false;

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();

	// Radius
	if (ImGui::DragFloat("Radius", &_radius, 0.01f, 0.01f))
	{
		_radius = max(0.001f, _radius);
		bodyInterface.SetShape(bodyID, new JPH::SphereShape(_radius), false, JPH::EActivation::DontActivate);
	}
	ImGuiUtils::LockMouseOnActive();

	// Offset
	if (ImGui::DragFloat3("Offset", &_offset.x, 0.01f))
	{
		Transform *transform = GetEntity()->GetTransform();
		dx::XMFLOAT3A wPos = transform->GetPosition(World);
		JPH::RVec3 newPos = JPH::RVec3(wPos.x + _offset.x, wPos.y + _offset.y, wPos.z + _offset.z);
		bodyInterface.SetPosition(bodyID, newPos, JPH::EActivation::DontActivate);
	}
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif
