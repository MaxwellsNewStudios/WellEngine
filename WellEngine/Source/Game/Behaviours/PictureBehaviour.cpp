#include "stdafx.h"
#include "Behaviours/PictureBehaviour.h"
#include "Behaviours/PickupBehaviour.h"
#include "Behaviours/HideBehaviour.h"
#include "Scenes/Scene.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool PictureBehaviour::Start()
{
	if (_name.empty())
		_name = "PictureBehaviour"; // For categorization in ImGui.

	_ent = GetEntity();
	_t = _ent->GetTransform();
	PickupBehaviour *pickupBehaviour;
	if (_ent->GetBehaviourByType<PickupBehaviour>(pickupBehaviour))
	{
		ErrMsg("Entity cannot have picture and pickup behaviour!");
		return false;
	}

	HideBehaviour *hideBehaviour;
	if (_ent->GetBehaviourByType<HideBehaviour>(hideBehaviour))
	{
		ErrMsg("Entity cannot have picture and hide behaviour!");
		return false;
	}

	InteractableBehaviour *interactableBehaviour;
	if (!_ent->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour))
	{
		interactableBehaviour = new InteractableBehaviour();
		if (!interactableBehaviour->Initialize(_ent))
		{
			ErrMsg("Failed to initialize interactable behaviour!");
			return false;
		}
	}
	interactableBehaviour->AddInteractionCallback(std::bind(&PictureBehaviour::Pickup, this));

	SoundBehaviour *sound = new SoundBehaviour("CollectPicture1");
	if (!sound->Initialize(GetEntity()))
	{
		ErrMsg("Failed to initialize sound to picture!");
		return false;
	}
	_sound = sound;
	_sound->SetVolume(5.0f);
	_sound->SetSerialization(false);

	if (!_isGeneric)
	{
		MeshBehaviour *mesh = nullptr;
		GetEntity()->GetBehaviourByType<MeshBehaviour>(mesh);
		mesh->SetOverlay(true);
		_ent->Disable();
	}

	return true;
}

void PictureBehaviour::OnEnable()
{
	_ent->SetParent(GetScene()->GetSceneHolder()->GetEntityByName("playerCamera"));
	_t->SetPosition(_offset);
}

void PictureBehaviour::OnDisable()
{

}

void PictureBehaviour::Pickup()
{
	_ent->SetParent(GetScene()->GetSceneHolder()->GetEntityByName("playerCamera"));
	_t->SetPosition(_offset);
	_sound->Play();
}

bool PictureBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	json::Value offsetArr = SerializerUtils::SerializeVec(_offset, docAlloc);
	obj.AddMember("Offset", offsetArr, docAlloc);
	obj.AddMember("Generic", _isGeneric, docAlloc);

	return true;
}

bool PictureBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	SerializerUtils::DeserializeVec(_offset, obj["Offset"]);
	_isGeneric = obj["Generic"].GetBool();

	return true;
}
