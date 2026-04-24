#include "stdafx.h"
#include "B_PhysicsForce.h"
#include "Colliders/B_Collider.h"
#include "Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


bool B_PhysicsForce::Start()
{
#ifdef USE_IMGUI
	QueueUpdate();
#endif
	QueuePhysicsUpdate();

	return true;
}

bool B_PhysicsForce::PhysicsUpdate(float deltaTime)
{
	if (B_Collider *collider; GetEntity()->GetBehaviourByType<B_Collider>(collider))
	{
		if (!collider->IsEnabled())
			return true;

		dx::XMFLOAT3 wForce = GetWorldForce();
		dx::XMFLOAT3 wPoint = GetWorldPoint();
		
		wForce.x *= deltaTime;
		wForce.y *= deltaTime;
		wForce.z *= deltaTime;

		collider->AddForce(wForce, wPoint);
	}

	return true;
}

bool B_PhysicsForce::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Force", SerializerUtils::SerializeVec(_force, docAlloc), docAlloc);
	obj.AddMember("Point", SerializerUtils::SerializeVec(_point, docAlloc), docAlloc);
	obj.AddMember("Local", _localSpace, docAlloc);
#ifdef USE_IMGUI
	obj.AddMember("DebugDraw", _debugDraw, docAlloc);
#endif

	return true;
}
bool B_PhysicsForce::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Force"))
		SerializerUtils::DeserializeVec(_force, obj["Force"]);

	if (obj.HasMember("Point"))
		SerializerUtils::DeserializeVec(_point, obj["Point"]);

	if (obj.HasMember("Local"))
		_localSpace = obj["Local"].GetBool();

#ifdef USE_IMGUI
	if (obj.HasMember("DebugDraw"))
		_debugDraw = obj["DebugDraw"].GetBool();
#endif

	return true;
}

#ifdef USE_IMGUI
bool B_PhysicsForce::Update(TimeUtils &time, const Input &input)
{
	if (_debugDraw)
	{
		dx::XMFLOAT3 wForce = GetWorldForce();
		dx::XMFLOAT3 wPoint = GetWorldPoint();

		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawRay(wPoint, wForce, 0.1f, {0, 1, 0, 1}, false);
	}

	return true;
}

bool B_PhysicsForce::RenderUI()
{
	ImGui::Checkbox("Debug Draw", &_debugDraw);
	ImGui::Checkbox("Local Space", &_localSpace);

	ImGui::DragFloat3("Force", &_force.x, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat3("Point", &_point.x, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif

dx::XMFLOAT3 B_PhysicsForce::GetWorldForce() const
{
	if (_localSpace) // Convert from local to world space
	{
		Transform *transform = GetTransform();
		dx::XMFLOAT4 rot = transform->GetRotation(World);

		dx::XMFLOAT3 rotatedForce;
		Store(rotatedForce, dx::XMVector3Rotate(Load(_force), Load(rot)));

		return rotatedForce;
	}

	return _force;
}
dx::XMFLOAT3 B_PhysicsForce::GetWorldPoint() const
{
	if (_localSpace) // Convert from local to world space
	{
		Transform *transform = GetTransform();

		return transform->PointLocalToWorld(To3(_point));
	}

	return _point;
}
