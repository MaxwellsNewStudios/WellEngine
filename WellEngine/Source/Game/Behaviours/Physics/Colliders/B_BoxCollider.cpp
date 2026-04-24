#include "stdafx.h"
#include "B_BoxCollider.h"
#include "Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


B_BoxCollider::B_BoxCollider(const dx::XMFLOAT3A &halfExtents, const dx::XMFLOAT3A &offset)
	: B_Collider(), _halfExtents(halfExtents), _offset(offset)
{ }

B_BoxCollider::B_BoxCollider(const dx::XMFLOAT3A &halfExtents, const dx::XMFLOAT3A &offset,
	JPH::EMotionType motionType, JPH::ObjectLayer layer, float friction, float gravityFactor, float restitution) 
	: B_Collider(motionType, layer, friction, gravityFactor, restitution), _halfExtents(halfExtents), _offset(offset)
{ }

bool B_BoxCollider::Start()
{
	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A wPos = transform->GetPosition(World);
	dx::XMFLOAT4A wRot = transform->GetRotation(World);
	dx::XMFLOAT3A scale = transform->GetScale();

	// Offset is applied locally, so we must calculate the world offset by applying the rotation to it.
	dx::XMFLOAT3A offset = _offset;
	{
		dx::XMMATRIX rotMat = dx::XMMatrixRotationQuaternion(Load(wRot));
		dx::XMVECTOR offsetVec = Load(offset);
		offsetVec = dx::XMVector3Transform(offsetVec, rotMat);
		Store(offset, offsetVec);

		offset.x *= scale.x;
		offset.y *= scale.y;
		offset.z *= scale.z;
	}

	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	JPH::BodyCreationSettings boxSettings(
		new JPH::BoxShape(JPH::Vec3Arg(
			MAX(0.0001f, _halfExtents.x * fabsf(scale.x)),
			MAX(0.0001f, _halfExtents.y * fabsf(scale.y)),
			MAX(0.0001f, _halfExtents.z * fabsf(scale.z))
		)),
		JPH::RVec3(wPos.x + offset.x, wPos.y + offset.y, wPos.z + offset.z),
		JPH::Quat(wRot.x, wRot.y, wRot.z, wRot.w),
		GetMotionType(), GetLayer()
	);
	boxSettings.mAllowDynamicOrKinematic = true;

	SetBodyID(bodyInterface.CreateAndAddBody(boxSettings, JPH::EActivation::Activate));

	return B_Collider::Start();
}


bool B_BoxCollider::Update(TimeUtils &time, const Input &input)
{
	if (!B_Collider::Update(time, input))
		return false;

#ifdef USE_IMGUI
	if (DoDebugDraw())
	{
		JPH::BodyInterface &bodyInterface = GetBodyInterface();

		JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(GetBodyID());
		JPH::Vec3 joltExt = ((JPH::BoxShape *)(bodyInterface.GetShape(GetBodyID()).GetPtr()))->GetHalfExtent();
		JPH::Quat joltRot = bodyInterface.GetRotation(GetBodyID());

		dx::XMFLOAT3A pos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
		dx::XMFLOAT3A ext = dx::XMFLOAT3A(joltExt.GetX(), joltExt.GetY(), joltExt.GetZ());
		dx::XMFLOAT4A rot = dx::XMFLOAT4A(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());

		dx::BoundingOrientedBox obb = dx::BoundingOrientedBox(pos, ext, rot);

		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawBoxOBB(obb, dx::XMFLOAT4(1, 0, 0, 0.2f));
	}
#endif

	return true;
}


bool B_BoxCollider::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	if (!B_Collider::Serialize(docAlloc, obj))
		return false;

	obj.AddMember("HalfExtents", SerializerUtils::SerializeVec(_halfExtents, docAlloc), docAlloc);
	obj.AddMember("Offset", SerializerUtils::SerializeVec(_offset, docAlloc), docAlloc);

	return true;
}
bool B_BoxCollider::Deserialize(const json::Value &obj, Scene *scene)
{
	if (!B_Collider::Deserialize(obj, scene))
		return false;

	SerializerUtils::DeserializeVec(_halfExtents, obj["HalfExtents"]);
	SerializerUtils::DeserializeVec(_offset, obj["Offset"]);

	return true;
}


void B_BoxCollider::CalcBodyLocation(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot)
{
	ZoneScopedC(RandomUniqueColor());

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	JPH::RVec3 joltPos = bodyInterface.GetCenterOfMassPosition(bodyID);
	JPH::Quat joltRot = bodyInterface.GetRotation(bodyID);

	dx::XMFLOAT3A newEntPos = dx::XMFLOAT3A(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
	dx::XMFLOAT4A newEntRot = dx::XMFLOAT4A(joltRot.GetX(), joltRot.GetY(), joltRot.GetZ(), joltRot.GetW());
	dx::XMFLOAT3A scale = transform->GetScale();

	dx::XMFLOAT3A offset = _offset;
	{
		dx::XMMATRIX rotMat = dx::XMMatrixRotationQuaternion(Load(newEntRot));
		dx::XMVECTOR offsetVec = Load(offset);
		offsetVec = dx::XMVector3Transform(offsetVec, rotMat);
		Store(offset, offsetVec);

		offset.x *= scale.x;
		offset.y *= scale.y;
		offset.z *= scale.z;
	}
	newEntPos.x -= offset.x;
	newEntPos.y -= offset.y;
	newEntPos.z -= offset.z;

	pos = newEntPos;
	rot = newEntRot;
}

void B_BoxCollider::RecalculatePhysicsBody()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();

	Transform *transform = GetEntity()->GetTransform();
	dx::XMFLOAT3A wPos = transform->GetPosition(World);
	dx::XMFLOAT4A wRot = transform->GetRotation(World);
	dx::XMFLOAT3A scale = transform->GetScale();

	dx::XMFLOAT3A offset = _offset;
	{
		dx::XMMATRIX rotMat = dx::XMMatrixRotationQuaternion(Load(wRot));
		dx::XMVECTOR offsetVec = Load(offset);
		offsetVec = dx::XMVector3Transform(offsetVec, rotMat);
		Store(offset, offsetVec);

		offset.x *= scale.x;
		offset.y *= scale.y;
		offset.z *= scale.z;
	}

	JPH::Vec3Arg extents = JPH::Vec3Arg(
		MAX(0.0001f, _halfExtents.x * fabsf(scale.x)),
		MAX(0.0001f, _halfExtents.y * fabsf(scale.y)),
		MAX(0.0001f, _halfExtents.z * fabsf(scale.z))
	);

	bodyInterface.SetShape(bodyID, new JPH::BoxShape(extents), true, JPH::EActivation::DontActivate);
	bodyInterface.SetPosition(bodyID, JPH::RVec3(wPos.x + offset.x, wPos.y + offset.y, wPos.z + offset.z), JPH::EActivation::Activate);
}
void B_BoxCollider::SyncPhysics()
{
	ZoneScopedC(RandomUniqueColor());

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	Transform *transform = GetEntity()->GetTransform();
	const JPH::BodyID &bodyID = GetBodyID();

	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);
	dx::XMFLOAT3A scale = transform->GetScale();

	dx::XMFLOAT3A offset = _offset;
	{
		dx::XMMATRIX rotMat = dx::XMMatrixRotationQuaternion(Load(currRot));
		dx::XMVECTOR offsetVec = Load(offset);
		offsetVec = dx::XMVector3Transform(offsetVec, rotMat);
		Store(offset, offsetVec);

		offset.x *= scale.x;
		offset.y *= scale.y;
		offset.z *= scale.z;
	}

	bodyInterface.SetPosition(bodyID, JPH::RVec3(currPos.x + offset.x, currPos.y + offset.y, currPos.z + offset.z), JPH::EActivation::Activate);
	bodyInterface.SetRotation(bodyID, JPH::Quat(currRot.x, currRot.y, currRot.z, currRot.w), JPH::EActivation::Activate);
}
void B_BoxCollider::SyncTransform()
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
bool B_BoxCollider::RenderUI()
{
	if (!B_Collider::RenderUI())
		return false;

	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	JPH::BodyID bodyID = GetBodyID();

	// Extents
	if (ImGui::DragFloat3("Half Extents", &_halfExtents.x, 0.01f))
	{
		_halfExtents.x = MAX(0.0001f, _halfExtents.x);
		_halfExtents.y = MAX(0.0001f, _halfExtents.y);
		_halfExtents.z = MAX(0.0001f, _halfExtents.z);

		RecalculatePhysicsBody();
	}
	ImGuiUtils::LockMouseOnActive();

	// Offset
	if (ImGui::DragFloat3("Offset", &_offset.x, 0.01f))
	{
		RecalculatePhysicsBody();
	}
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif
