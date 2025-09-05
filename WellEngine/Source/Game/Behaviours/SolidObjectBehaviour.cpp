#include "stdafx.h"
#include "Behaviours/SolidObjectBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Debug/ErrMsg.h"
#include "ImGui/imgui.h"
#include "Math/GameMath.h"
#include "Behaviours/ColliderBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace Collisions;

bool SolidObjectBehaviour::Start()
{
	if (_name.empty())
		_name = "SolidObjectBehaviour"; // For categorization in ImGui.

	Entity *ent = GetEntity();

	ColliderBehaviour *colB;
	ent->GetBehaviourByType<ColliderBehaviour>(colB);

	if (colB)
		colB->AddOnIntersection([this](const Collisions::CollisionData &data) { AdjustForCollision(data); });

	return true;
}

bool SolidObjectBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}
bool SolidObjectBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

void SolidObjectBehaviour::AdjustForCollision(const Collisions::CollisionData &data)
{
	using namespace DirectX;

	XMFLOAT3A move;
	Store(move, XMVectorScale(XMVector3Normalize(XMLoadFloat3(&data.normal)), data.depth/2));

	Transform *t = GetEntity()->GetTransform();
	t->Move(move, World);
}

