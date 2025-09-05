#include "stdafx.h"
#include "Behaviours/PickupBehaviour.h"
#include "Behaviours/HideBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool PickupBehaviour::Start()
{
	if (_name.empty())
		_name = "PickupBehaviour"; // For categorization in ImGui.

	Entity *ent = GetEntity();
	HideBehaviour *hideBehaviour;
	if (ent->GetBehaviourByType<HideBehaviour>(hideBehaviour))
	{
		ErrMsg("Entity cannot have pickup and hide behaviour!");
		return false;
	}

	InteractableBehaviour *interactableBehaviour;
	if (!ent->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour))
	{
		interactableBehaviour = new InteractableBehaviour();
		if (!interactableBehaviour->Initialize(ent))
		{
			ErrMsg("Failed to initialize interactable behaviour!");
			return false;
		}
	}
	interactableBehaviour->AddInteractionCallback(std::bind(&PickupBehaviour::Pickup, this));

	QueueUpdate();

	return true;
}

bool PickupBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (_isHolding)
	{
		// { 0.7f, -0.4f, 0.8f }

		GetEntity()->SetParent(GetScene()->GetSceneHolder()->GetEntityByName("playerCamera"));
		_objectTransform->SetPosition(_offset);
	}
	else if (_objectTransform != nullptr)
	{
		GetEntity()->SetParent(nullptr);
		dx::XMFLOAT3A playerPos = _playerTransform->GetPosition(World);
		_objectTransform->Move(dx::XMFLOAT3A(playerPos.x - _offset.x, 1.0f - _offset.y, playerPos.z - _offset.z), World); // TODO: Change y to ground height

		dx::XMFLOAT4A playerRot = _playerTransform->GetRotation(World);
		playerRot.x = 0.0f;
		playerRot.z = 0.0f;
		Store(playerRot, dx::XMVector4Normalize(Load(playerRot)));
		_objectTransform->SetRotation(playerRot, World);
		_objectTransform = nullptr;
	}

	return true;
}

bool PickupBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{	
	return true;
}

bool PickupBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

void PickupBehaviour::Pickup()
{
	_isHolding = !_isHolding;

	_objectTransform = GetEntity()->GetTransform();
	_playerTransform = GetScene()->GetPlayerCamera()->GetTransform();
}
