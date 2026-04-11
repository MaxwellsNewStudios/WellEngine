#include "stdafx.h"
#include "JoltSphereColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltSphereColliderBehaviour::JoltSphereColliderBehaviour(float radius, JPH::EMotionType type, JPH::ObjectLayer layer) :
	JoltColliderBehaviour(type, layer), _radius(radius)
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
	dx::XMFLOAT4A wRot = transform->GetRotation(World);

	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	JPH::BodyCreationSettings sphereSettings(
		new JPH::SphereShape(_radius),
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

		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawSphere(pos, _radius, 3, dx::XMFLOAT4(1, 0, 0, 0.2f));
	}
#endif

	return true;
}

bool JoltSphereColliderBehaviour::LateUpdate(TimeUtils &time, const Input &input)
{
	if (!JoltColliderBehaviour::LateUpdate(time, input))
		return false;

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

	return true;
}
#endif
