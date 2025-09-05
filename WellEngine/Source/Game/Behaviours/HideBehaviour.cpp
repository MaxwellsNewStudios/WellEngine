#include "stdafx.h"
#include "Behaviours/HideBehaviour.h"
#include "Behaviours/PickupBehaviour.h"
#include "Scenes/Scene.h"
#include "Behaviours/PlayerMovementBehaviour.h"
#include "Behaviours/FlashlightBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool HideBehaviour::Start()
{
	if (_name.empty())
		_name = "HideBehaviour"; // For categorization in ImGui.

	_objectTransform = GetEntity()->GetTransform();
	_playerEntity = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity");

	Entity *ent = GetEntity();
	PickupBehaviour *pickupBehaviour;
	if (ent->GetBehaviourByType<PickupBehaviour>(pickupBehaviour))
	{
		ErrMsg("Entity cannot have hide and pickup behaviour!");
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
	interactableBehaviour->AddInteractionCallback(std::bind(&HideBehaviour::Hide, this));

	SoundBehaviour *sound = new SoundBehaviour("LockerDoorSound", dx::SOUND_EFFECT_INSTANCE_FLAGS::SoundEffectInstance_Default);
	if (!sound->Initialize(GetEntity()))
	{
		ErrMsg("Failed to create locker door sound!");
		return false;
	}
	_sfx = sound;
	_sfx->SetSerialization(false);
	_sfx->SetVolume(0.1f);
	_sfx->SetEnabled(false);

	return true;
}

bool HideBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool HideBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

void HideBehaviour::Hide()
{
	if (!_playerEntity) 
		_playerEntity = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity");

	if (!_playerEntity)
	{
		ErrMsg("Could not find player entity!");
		return;
	}

	PlayerMovementBehaviour *pmb;
	_playerEntity->GetBehaviourByType<PlayerMovementBehaviour>(pmb);
	if (!pmb)
	{
		ErrMsg("Could not find PlayerMovementBehaviour!");
		return;
	}

	ColliderBehaviour *playerCollider = nullptr;
	GetScene()->GetSceneHolder()->GetEntityByName("Player Entity")->GetBehaviourByType<ColliderBehaviour>(playerCollider);
	if (!playerCollider)
	{
		ErrMsg("Could not find player collider!");
		return;
	}

	Entity *pCamera = GetScene()->GetSceneHolder()->GetEntityByName("playerCamera");
	if (!playerCollider)
	{
		ErrMsg("Could not find player camera!");
		return;
	}

	if (pmb->IsHiding()) // Player is already hiding -> Exit hiding spot
	{
		pmb->SetHiding(false);

		dx::XMFLOAT3A objectPos = _objectTransform->GetPosition(World);
		dx::XMFLOAT3A objectFor = _objectTransform->GetForward(World);

		// Move player forward 1 meter in front of hiding spot
		dx::XMFLOAT3A newPlayerPos = {};
		newPlayerPos.x = objectPos.x + objectFor.x;
		newPlayerPos.y = objectPos.y + objectFor.y;
		newPlayerPos.z = objectPos.z + objectFor.z;
		_playerEntity->GetTransform()->SetPosition(dx::XMFLOAT3A(newPlayerPos.x, newPlayerPos.y, newPlayerPos.z), World); // Change y to ground height

		playerCollider->SetEnabled(true);

		if (_sfx->IsEnabledSelf())
		{
			_sfx->ResetSound();
			_sfx->Play();
		}
		else
		{
			_sfx->SetEnabled(true);
		}
	}
	else if (!pmb->IsHiding()) // Player is not hiding -> Enter hiding spot
	{
		pmb->SetHiding(true);

		dx::XMFLOAT3A objectPos = _objectTransform->GetPosition(World);
		objectPos.y += 0.2f; // does nothing apparently

		// Set Player transform to hiding position
		_playerEntity->GetTransform()->SetPosition(objectPos, World);
		_playerEntity->GetTransform()->SetRotation(_objectTransform->GetRotation(World)); // fixing playerEntity rotation to match hiding object
		pCamera->GetTransform()->SetRotation(_objectTransform->GetRotation(World)); // fixing playerCamera rotation to match hiding object

		playerCollider->SetEnabled(false);

		if (_sfx->IsEnabledSelf())
		{
			_sfx->ResetSound();
			_sfx->Play();
		}
		else
		{
			_sfx->SetEnabled(true);
		}
	}
}
