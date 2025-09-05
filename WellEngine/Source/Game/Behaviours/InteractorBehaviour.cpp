#include "stdafx.h"
#include "Behaviours/InteractorBehaviour.h"
#include "Behaviours/InteractableBehaviour.h"
#include "Behaviours/PickupBehaviour.h"
#include "Behaviours/HideBehaviour.h"
#include "Scenes/Scene.h"
#include "Behaviours/PictureBehaviour.h"
#include "Behaviours/InventoryBehaviour.h"
#include <format>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool InteractorBehaviour::Start()
{
	if (_name.empty())
		_name = "InteractorBehaviour"; // For categorization in ImGui.

	QueueUpdate();

	return true;
}

bool InteractorBehaviour::Update(TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();

	if (_picPieces.size() == 0) // Post start
	{
		for (UINT i = 0; i < _totalPieces; i++)
		{
			std::string name = std::format("PicPiece{}", i + 1);
			_picPieces.emplace_back(std::move(sceneHolder->GetEntityByName(name)));

			if (!_picPieces[i])
				continue;

			_picPieces[i]->Disable();

			MeshBehaviour* mesh = nullptr;

			if (_picPieces[i]->GetBehaviourByType<MeshBehaviour>(mesh))
			{
				Material meshMat = *mesh->GetMaterial();
				meshMat.vsID = scene->GetContent()->GetShaderID("VS_Geometry");

				if (!mesh->SetMaterial(&meshMat))
					Warn("Failed to set material for picture piece mesh!");
			}
		}

		for (UINT i = 0; i < _totalCollected; i++)
		{
			if (!_picPieces[i])
				continue;

			_picPieces[i]->Enable();
			MeshBehaviour *mesh = nullptr;

			if (_picPieces[i]->GetBehaviourByType<MeshBehaviour>(mesh))
			{
				BoundingOrientedBox box = BoundingOrientedBox({ 0,1,0 }, { 0.01f, 0.01f, 0.01f }, { 0,0,0,1 });
				mesh->SetBounds(box);
			}
		}
	}
	
	if (input.GetKey(KeyCode::E) == KeyState::Pressed)
	{
		if (input.IsCursorLocked())
		{
			Transform *transform = scene->GetViewCamera()->GetTransform();

			// Interact with any object
			RaycastOut out;
			InteractableBehaviour *interactableBehaviour = nullptr;
			if (sceneHolder->RaycastScene(transform->GetPosition(World), transform->GetForward(World), out) &&
				out.entity->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour) &&
				out.distance <= _interactionRange)
			{
				PictureBehaviour *picture = nullptr;
				if (!std::count(_picPieces.begin(), _picPieces.end(), out.entity) &&
					out.entity->GetBehaviourByType<PictureBehaviour>(picture))
				{
					InventoryBehaviour* inventory = nullptr;
					GetEntity()->GetBehaviourByType<InventoryBehaviour>(inventory);
					if(inventory)
						inventory->SetHeldItem(false, HandState::PicturePiece);
					else
					{
						ErrMsg("Failed to get inventory behaviour!");
						return false;
					}

					if (!sceneHolder->RemoveEntity(out.entity))
					{
						ErrMsg("Failed to remove entity!");
						return false;
					}
					_totalCollected = _totalCollected > _totalPieces ? _totalPieces : _totalCollected + 1;
					for (UINT i = 0; i < _totalCollected; i++)
					{
						_picPieces[i]->Enable();
					}
				}

				interactableBehaviour->OnInteraction();

				PickupBehaviour *pickup = nullptr;
				if (out.entity->GetBehaviourByType<PickupBehaviour>(pickup))
				{
					_isHolding = true;
					_holdingEnt = out.entity;
				}

				HideBehaviour *hide = nullptr;
				if (out.entity->GetBehaviourByType<HideBehaviour>(hide))
				{
					_isHiding = true;
					_hidingEnt = out.entity;
				}
			}

			interactableBehaviour = nullptr;
			const std::vector<Entity*> *children = sceneHolder->GetEntityByName("playerCamera")->GetChildren();
			UINT i = 0;
			for (i; i < children->size(); i++)
			{
				if (children->at(i) == _holdingEnt)
				{
					children->at(i)->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour);
					break;
				}
			}
			if (_isHolding && !_isHiding && children->size() > 1 && interactableBehaviour) // Drop holding object
			{
				interactableBehaviour->OnInteraction();

				if (children->at(i) == _holdingEnt)
				{
					_isHolding = false;
				}

				return true;
			}

			if (_isHiding)
			{
				_isHiding = false;
			}
		}
	}

	return true;
}

float InteractorBehaviour::GetInteractionRange() const
{
	return _interactionRange;
}

bool InteractorBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	UINT holdingID = _holdingEnt ? _holdingEnt->GetID() : CONTENT_NULL;
	UINT hidingID = _hidingEnt ? _hidingEnt->GetID() : CONTENT_NULL;

	obj.AddMember("Showing Picture", _showingPic, docAlloc); // TODO: Shorten name
	obj.AddMember("Collected", _totalCollected, docAlloc);
	obj.AddMember("Holding", _isHolding, docAlloc);
	//obj.AddMember("Hold ID", holdingID, docAlloc);
	//obj.AddMember("Hiding", _isHiding, docAlloc);
	//obj.AddMember("Hide ID", hidingID, docAlloc);

	return true;
}

bool InteractorBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_showingPic		= obj["Showing Picture"].GetBool(); // TODO: Shorten name
	_totalCollected = obj["Collected"].GetUint();
	_isHolding		= obj["Holding"].GetBool();

	return true;
}

void InteractorBehaviour::PostDeserialize()
{
	if (!_isHolding)
		return;
	
	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();
	Transform *playerTransform = GetEntity()->GetTransform();

	const std::vector<Entity *> *children = sceneHolder->GetEntityByName("playerCamera")->GetChildren();

	for (Entity *entity : *children)
	{
		std::string name = entity->GetName();
		if (name == "Flashlight" ||
			name == "PicPiece1" ||
			name == "PicPiece2" ||
			name == "PicPiece3" ||
			name == "PicPiece4" ||
			name == "PicPiece5")
			continue;

		Transform *entTransform = entity->GetTransform();
			
		entity->SetParent(nullptr);
		dx::XMFLOAT3A _offset = { -0.9f, -0.6f, 0.8f }; // Local offset from player camera
		dx::XMFLOAT3A playerPos = playerTransform->GetPosition(World);

		entTransform->Move(dx::XMFLOAT3A(
			playerPos.x - _offset.x, 
			1.0f - _offset.y,				// TODO: Change y to ground height
			playerPos.z - _offset.z
		), World);

		dx::XMFLOAT4A playerRot = playerTransform->GetRotation(World);
		playerRot.x = 0.0f;
		playerRot.z = 0.0f;
		Store(playerRot, dx::XMVector4Normalize(Load(playerRot)));
		entTransform->SetRotation(playerRot, World);
		break;
	}
	
}

void InteractorBehaviour::ShowPicture()
{
	for (UINT i = 0; i < _totalCollected; i++)
	{
		_picPieces[i]->Enable();
	}
}

void InteractorBehaviour::HidePicture()
{
	for (UINT i = 0; i < _totalCollected; i++)
	{
		_picPieces[i]->Disable();
	}
}

UINT InteractorBehaviour::GetCollectedPieces() const
{
	return _totalCollected;
}
