#include "stdafx.h"
#include "B_SphereCollider.h"
#include "Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


B_SphereCollider::B_SphereCollider(float radius)
	: B_Collider(), _radius(radius)
{ }

B_SphereCollider::B_SphereCollider(float radius,
	JPH::EMotionType motionType, JPH::ObjectLayer layer, float friction, float gravityFactor, float restitution)
	: B_Collider(motionType, layer, friction, gravityFactor, restitution), _radius(radius)
{ }

bool B_SphereCollider::Start()
{
	if (_name.empty())
		_name = "B_SphereCollider"; // For categorization in ImGui.

	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A wPos = transform->GetPosition(World);
	dx::XMFLOAT4A wRot = transform->GetRotation(World);
	dx::XMFLOAT3A scale = transform->GetScale();

	// Get the maximum scale component to apply to the radius
	float maxScale = MAX(0.0001f, MAX(fabsf(scale.x), MAX(fabsf(scale.y), fabsf(scale.z))));

	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	JPH::BodyCreationSettings sphereSettings(
		new JPH::SphereShape(_radius * maxScale),
		JPH::RVec3(wPos.x, wPos.y, wPos.z), 
		JPH::Quat(wRot.x, wRot.y, wRot.z, wRot.w),
		GetMotionType(), GetLayer()
	);
	sphereSettings.mAllowDynamicOrKinematic = true;
	SetBodyID(bodyInterface.CreateAndAddBody(sphereSettings, JPH::EActivation::Activate));

	return B_Collider::Start();
}


bool B_SphereCollider::Update(TimeUtils &time, const Input &input)
{
	if (!B_Collider::Update(time, input))
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


bool B_SphereCollider::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	if (!B_Collider::Serialize(docAlloc, obj))
		return false;

	obj.AddMember("Radius", _radius, docAlloc);

	return true;
}
bool B_SphereCollider::Deserialize(const json::Value &obj, Scene *scene)
{
	if (!B_Collider::Deserialize(obj, scene))
		return false;

	_radius = obj["Radius"].GetFloat();

	return true;
}


void B_SphereCollider::CalcBodyLocation(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot)
{
	ZoneScopedC(RandomUniqueColor());

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(bodyID);
	JPH::Quat joltRot = bodyInterface.GetRotation(bodyID);

	pos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
	rot = dx::XMFLOAT4A(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());
}
void B_SphereCollider::RecalculatePhysicsBody()
{
	ZoneScopedC(RandomUniqueColor());

	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A scale = transform->GetScale();

	// Get the maximum scale component to apply to the radius
	float maxScale = MAX(0.0001f, MAX(fabsf(scale.x), MAX(fabsf(scale.y), fabsf(scale.z))));

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();
	bodyInterface.SetShape(bodyID, new JPH::SphereShape(_radius * maxScale), false, JPH::EActivation::Activate);
}
void B_SphereCollider::SyncPhysics()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);

	bodyInterface.SetPosition(bodyID, JPH::RVec3(currPos.x, currPos.y, currPos.z), JPH::EActivation::Activate);
	bodyInterface.SetRotation(bodyID, JPH::Quat(currRot.x, currRot.y, currRot.z, currRot.w), JPH::EActivation::Activate);
}
void B_SphereCollider::SyncTransform()
{
	ZoneScopedC(RandomUniqueColor());

	dx::XMFLOAT3A newEntPos;
	dx::XMFLOAT4A newEntRot;
	CalcBodyLocation(newEntPos, newEntRot);

	Transform *transform = GetTransform();
	transform->SetPosition(newEntPos, World);
	transform->SetRotation(newEntRot, World);
}


#ifdef USE_IMGUI
bool B_SphereCollider::RenderUI()
{
	if (!B_Collider::RenderUI())
		return false;

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();

	// Radius
	if (ImGui::DragFloat("Radius", &_radius, 0.01f))
	{
		_radius = MAX(0.001f, _radius);
		RecalculatePhysicsBody();
	}
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif
