#include "stdafx.h"
#include "Behaviours/PlayerViewBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Collision/Intersections.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

PlayerViewBehaviour::PlayerViewBehaviour(PlayerMovementBehaviour *movementBehaviour)
{
	_movement = movementBehaviour;
}

// Casts a ray from _camera in the direction of _camera
bool PlayerViewBehaviour::RayCastFromCamera(RaycastOut &out, CameraBehaviour *camera)
{
	const XMFLOAT3A
		camPos = camera->GetTransform()->GetPosition(World),
		camDir = camera->GetTransform()->GetForward(World);

	return GetScene()->GetSceneHolder()->RaycastScene(
		{ camPos.x, camPos.y, camPos.z },
		{ camDir.x, camDir.y, camDir.z },
		out);
}

bool PlayerViewBehaviour::Start()
{
	if (_name.empty())
		_name = "PlayerViewBehaviour"; // For categorization in ImGui.

	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();
	Entity *playerEnt = sceneHolder->GetEntityByName("Player Entity");
	if (!playerEnt)
	{
		ErrMsg("Failed getting player entity!");
		return true;
	}

	if (!playerEnt->GetBehaviourByType<PlayerMovementBehaviour>(_movement))
	{
		ErrMsg("Failed getting player movement behaviour!");
		return true;
	}

	if (!playerEnt->GetBehaviourByType<InteractorBehaviour>(_interactor))
	{
		ErrMsg("Failed getting interactor behaviour!");
		return true;
	}


	Entity *cameraEnt = GetEntity();
	if (!cameraEnt)
	{
		ErrMsg("Failed getting player camera entity!");
		return true;
	}
	if (!cameraEnt->GetBehaviourByType<CameraBehaviour>(_camera))
	{
		ErrMsg("Failed getting player camera behaviour!");
		return true;
	}

	QueueUpdate();
	QueueFixedUpdate();

	return true;
}

bool PlayerViewBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (!_movement || !_interactor)
	{
		SceneHolder *sceneHolder = GetScene()->GetSceneHolder();
		Entity *playerEnt = sceneHolder->GetEntityByName("Player Entity");
		
		if (!playerEnt)
		{
			ErrMsg("Failed getting player entity!");
			return true;
		}

		if (!playerEnt->GetBehaviourByType<PlayerMovementBehaviour>(_movement))
		{
			ErrMsg("Failed getting player movement behaviour!");
			return true;
		}

		if (!playerEnt->GetBehaviourByType<InteractorBehaviour>(_interactor))
		{
			ErrMsg("Failed getting interactor behaviour!");
			return true;
		}
	}

	if (!_camera)
	{
		Entity *cameraEnt = GetEntity();
		if (!cameraEnt)
		{
			ErrMsg("Failed getting player camera entity!");
			return true;
		}
		if (!cameraEnt->GetBehaviourByType<CameraBehaviour>(_camera))
		{
			ErrMsg("Failed getting player camera behaviour!");
			return true;
		}
	}

	XMVECTOR start = Load(_startPos);
	XMVECTOR target = Load(_targetPos);

	_targetLerpTime += time.GetDeltaTime();
	XMVECTOR lerp = XMVectorLerp(start, target, min(_targetLerpTime * _targetLerpTimeTotalInverse, 1.0f));

	XMFLOAT3A lerpPos;
	Store(lerpPos, lerp);
	GetTransform()->SetPosition(lerpPos, World);
	GetTransform()->SetRotation(_movement->GetPlayerCameraDesiredPos()->GetRotation(World), World);

	// Run OnHover of the hit entity
	static Entity *lastEnt = nullptr;
	RaycastOut out;
	bool hasHit = RayCastFromCamera(out, _camera);
	if (hasHit && out.entity)
	{
		bool canHover = !_movement->IsHiding() && out.distance < _interactor->GetInteractionRange();
		if (canHover)
		{
			if (!out.entity->InitialOnHover() && !out.entity->IsRemoved())
			{
				ErrMsg("OnHover Failed!");
				return false;
			}
		}
		if ((out.entity != lastEnt || !canHover))
		{
			if (lastEnt && !lastEnt->IsRemoved() && !lastEnt->InitialOffHover())
			{
				ErrMsg("OffHover Failed!");
				return false;
			}
		}
		lastEnt = out.entity;
	}
	else
	{
		if (lastEnt)
		{
			if (!lastEnt->InitialOffHover())
			{
				ErrMsg("OffHover Failed!");
				return false;
			}
		}
		lastEnt = nullptr;
	}

	return true;
}

bool PlayerViewBehaviour::FixedUpdate(float deltaTime, const Input &input)
{
	if (!_movement)
		return true;

	Transform *desiredTransform = _movement->GetPlayerCameraDesiredPos();
	_targetPos = desiredTransform->GetPosition(World);
	_startPos = GetTransform()->GetPosition(World);

	_targetLerpTime = 0.0f;
	_targetLerpTimeTotalInverse = 1.0f / (deltaTime * _moveInertia);
	return true;
}

#ifdef USE_IMGUI
bool PlayerViewBehaviour::RenderUI()
{
	ImGui::InputFloat("Lerp Speed", &_moveInertia, 0.01f, 0.1f);
	return true;
}
#endif

bool PlayerViewBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool PlayerViewBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();
	Entity *playerEnt = sceneHolder->GetEntityByName("Player Entity");
	if (playerEnt)
		playerEnt->GetBehaviourByType<PlayerMovementBehaviour>(_movement);
	return true;
}
