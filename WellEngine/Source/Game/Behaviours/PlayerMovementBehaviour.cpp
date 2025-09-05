#include "stdafx.h"
#include "Behaviours/PlayerMovementBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Collision/Intersections.h"
#include "Behaviours/MonsterBehaviour.h"
#include "Behaviours/InventoryBehaviour.h"
#include "Behaviours/RestrictedViewBehaviour.h"
#include "PlayerViewBehaviour.h"
#include <cmath>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


using namespace DirectX;

bool PlayerMovementBehaviour::Start()
{
	if (_name.empty())
		_name = "PlayerMovementBehaviour"; // For categorization in ImGui.

	//_spawnPosition = GetTransform()->GetPosition(World);
	//_spawnPosition = { 67.0f, -1.0f, 10.0f };
	//GetTransform()->SetPosition(_spawnPosition);

	_spawnPositions.emplace_back(-119.0f, 5.5f, -283.0f);
	_spawnPositions.emplace_back(-1.0f, 5.0f, -10.0f);
	_spawnPositions.emplace_back(-200.0f, -4.0f, 74.0f);
	_spawnPositions.emplace_back(-54.0f, 14.0f, 183.0f);
	_spawnPositions.emplace_back(131.0f, -3.0f, -17.0f);
	_spawnPositions.emplace_back(-35.0f, -32.0f, -187.0f);

	// Initialize Audio
	SoundBehaviour *sound = new SoundBehaviour("footstep_concrete_000", SoundEffectInstance_Default);
	_steps.emplace_back(sound);

	sound = new SoundBehaviour("footstep_concrete_001", SoundEffectInstance_Default);
	_steps.emplace_back(sound);

	sound = new SoundBehaviour("footstep_concrete_002", SoundEffectInstance_Default);
	_steps.emplace_back(sound);

	sound = new SoundBehaviour("footstep_concrete_003", SoundEffectInstance_Default);
	_steps.emplace_back(sound);

	sound = new SoundBehaviour("footstep_concrete_004", SoundEffectInstance_Default);
	_steps.emplace_back(sound);

	for (int i = 0; i < _steps.size(); i++)
	{
		if (!_steps[i]->Initialize(GetEntity()))
		{
			ErrMsg("Failed to initialize footstep sound!");
			return false;
		}
		_steps[i]->SetSerialization(false);
		_steps[i]->SetVolume(0.01f);
		_steps[i]->SetEnabled(false);
	}

	_collapseSound = new SoundBehaviour("BodyCollapse", SoundEffectInstance_ReverbUseFilters, false, 75.0f, 0.5f);
	if (!_collapseSound->Initialize(GetEntity()))
	{
		ErrMsg("Failed to initialize collapse sound!");
		return false;
	}
	_collapseSound->SetSerialization(false);
	_collapseSound->SetVolume(0.02f);

	// Breath sound initialization
	{
		std::array<std::string, 2> breathSounds = { "normal_recovery", "depleted_recovery" };

		for (const auto &sound : breathSounds)
		{
			SoundBehaviour *soundBeh = new SoundBehaviour("breath_" + sound, SoundEffectInstance_Default, false);
			_breathSounds.try_emplace(sound, soundBeh);
			if (!_breathSounds[sound]->Initialize(GetEntity()))
			{
				ErrMsg("Failed to initialize breath sound " + sound + "!");
				return false;
			}
			_breathSounds[sound]->SetSerialization(false);
			_breathSounds[sound]->SetVolume(0.05f);
			_breathSounds[sound]->SetEnabled(false);
		}
	}

	GetTerrain();

	QueueUpdate();
	QueueLateUpdate();

	return true;
}

bool PlayerMovementBehaviour::Update(TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();

	if (!_flashlight)
	{
		_flashlight = sceneHolder->GetEntityByName("Flashlight");
	}

	if (!_playerCamera)
	{
		Entity *playerCamEnt = sceneHolder->GetEntityByName("playerCamera");
		CameraBehaviour *playerCam;

		if (!playerCamEnt->GetBehaviourByType<CameraBehaviour>(playerCam))
			return true;

		if (!SetPlayerCamera(playerCam))
		{
			ErrMsg("Failed to set player camera!");
			return false;
		}
	}

	if (!_headTrackerEntity)
	{
		_headTrackerEntity = sceneHolder->GetEntityByName("Player Head Tracker");

		if (!_headTrackerEntity)
			return true;
	}

	if (!_flyMode && (!_terrainFloor || !_terrainRoof || !_terrainWalls))
	{
		if (!GetTerrain())
			return true;
	}

	if (_isCaught)
	{
		_currentBreathSound = nullptr;
		PlayCaughtAnimnation(time);
	}
	else if (!_isLocked && input.IsInFocus() && _playerCamera == scene->GetViewCamera())
	{
		// Keyboard input
		bool forward = BindingCollection::IsTriggered(InputBindings::InputAction::WalkForward);
		bool backward = BindingCollection::IsTriggered(InputBindings::InputAction::WalkBackward);
		bool left = BindingCollection::IsTriggered(InputBindings::InputAction::StrafeLeft);
		bool right = BindingCollection::IsTriggered(InputBindings::InputAction::StrafeRight);
		bool sneak = BindingCollection::IsTriggered(InputBindings::InputAction::Sneak);
		bool run = BindingCollection::IsTriggered(InputBindings::InputAction::Sprint);
		bool pressedMove = forward || backward || left || right;

		// Play the current breath sound
		if (_currentBreathSound)
		{
			if (!_currentBreathSound->IsEnabled())
			{
				_currentBreathSound->SetEnabled(true);
			}

			_currentBreathSound->Play();
		}

		// State determination
		if (sneak) // Sneaking
		{
			_velocity = _sneakVelocity;
			_isSneaking = true;
			_isWalking = false;
			_isRunning = false; 

			_headTrackerEntity->GetTransform()->SetPosition({ 0, _cameraHeight * 0.425f, 0 });
		}
		else 
		{
			// Headbobbing synced with steps
			_stepProgress += time.GetDeltaTime();
			const float offset = abs(cos((_stepProgress + _stepInterval) * M_PI/_stepInterval));

			if (run && _canRun && !backward) // Running
			{
				_velocity = _runVelocity;
				_isSneaking = false;
				_isWalking = false;
				_isRunning = true;

				// Stamina drain
				_stamina -= _staminaDrainRate * time.GetDeltaTime();
				if (_stamina <= 0.0f) 
					_canRun = false;
				
				const float bobbingFactor = 0.3f;
				_headTrackerEntity->GetTransform()->SetPosition({ 0, 0.95f * _cameraHeight - bobbingFactor * offset, 0});
			}
			else // Walking
			{
				_velocity = _walkVelocity;
				_isSneaking = false;
				_isWalking = true;
				_isRunning = false;

				const float bobbingFactor = 0.15f;
				_headTrackerEntity->GetTransform()->SetPosition({ 0, 0.90f * _cameraHeight - bobbingFactor * offset, 0 });
			}
		}

		if (_isHiding) 
		{
			_headTrackerEntity->GetTransform()->SetPosition({ 0, _cameraHeight * 0.95f, 0 });
		}

		if (_flyMode)
		{
			_canRun = true;
			_stamina = _staminaMax; // In fly mode, stamina is always maxed out
		}
		else if (!_isRunning && _stamina < _staminaMax)
		{
			// Recover stamina when not running
			if (_canRun)
			{
				_currentBreathSound = _breathSounds["normal_recovery"];
			}
			else
			{
				_currentBreathSound = _breathSounds["depleted_recovery"];
			}

			_stamina += _staminaRecoveryRate * time.GetDeltaTime();
			if (_stamina > _staminaMax)
			{
				_stamina = _staminaMax;
				_currentBreathSound = nullptr;
			}

			if (_stamina > _staminaMax * 0.4f)
			{
				_canRun = true;
			}
		}

		Transform *playerT = GetTransform();
		Transform *headTrackerT = _headTrackerEntity->GetTransform();

		// Mouse Input
		if (_playerCamera && !_isHiding)
		{
			if (input.IsCursorLocked())
			{
				const MouseState mState = input.GetMouse();

				if (mState.delta.x != 0.0f)
				{
					XMFLOAT3A up = playerT->GetUp();
					float invert = SIGN(XMVectorGetX(XMVector3Dot(Load(up), { 0.0f, 1.0f, 0.0f, 0.0f })));
					playerT->RotateAxis(up, (mState.delta.x / 360.0f) * invert, Local);
				}

				if (mState.delta.y != 0)
				{
					// Limit up and down movement to 90 degrees
					XMFLOAT3A camFwd = headTrackerT->GetForward();
					bool rotate = true;

					if (mState.delta.y < 0)	// up rotation
					{
						if (camFwd.y <= 0.99f) 
						{
							rotate = true;
						}
						else if (camFwd.y >= -0.99f) // down rotation
						{
							rotate = true;
						}
					}

					headTrackerT->RotatePitch(mState.delta.y / 360.0f, Local);
					if (headTrackerT->GetUp().y < 0.01f)
					{
						headTrackerT->RotatePitch(-mState.delta.y / 360.0f, Local); // Rotate back if too far
					}
				}
			}
		}

		// Calculate movement vector in entity local space
		XMFLOAT3A	localForward =		_flyMode ? headTrackerT->GetForward(World) : playerT->GetForward(Local);
		XMFLOAT3A	localRight =		playerT->GetRight(Local);
		XMVECTOR	localForwardVec =	XMVector3Normalize(Load(localForward));
		XMVECTOR	localRightVec =		XMVector3Normalize(Load(localRight));

		XMVECTOR moveVec = { 0.0f, 0.0f, 0.0f };
		if (forward)	moveVec = XMVectorAdd(moveVec, localForwardVec);
		if (backward)	moveVec = XMVectorAdd(moveVec, XMVectorScale(localForwardVec, -1.0f));
		if (left)		moveVec = XMVectorAdd(moveVec, XMVectorScale(localRightVec, -1.0f));
		if (right)		moveVec = XMVectorAdd(moveVec, localRightVec);

		if (_flyMode)
		{
			bool up = input.GetKey(KeyCode::Space) == KeyState::Held;
			bool down = input.GetKey(KeyCode::X) == KeyState::Held;
			pressedMove = pressedMove || up || down;

			XMFLOAT3A	localUp =		headTrackerT->GetUp(World);
			XMVECTOR	localUpVec =	XMVector3Normalize(Load(localUp));

			if (up)		moveVec = XMVectorAdd(moveVec, localUpVec);
			if (down)	moveVec = XMVectorAdd(moveVec, XMVectorScale(localUpVec, -1.0f));
		}

		if (pressedMove) // if there is movement input in any direction
		{
			_isStill = false;
			moveVec = XMVector3Normalize(moveVec); // Normalized movement direction in entity local space

			// Calculate velocity multiplier based on input direction in relation to local forward
			float dot; Store(dot, XMVector3Dot(moveVec, localForwardVec));

			float dirMultiplier = 0.0f;
			if (_flyMode)
			{
				dirMultiplier = 2.5f; // In fly mode, always move at full speed
			}
			else
			{
				if (dot >= 0.0f)
					dirMultiplier = _sidewaysMultiplier + dot * (_forwardsMultiplier - _sidewaysMultiplier);			// interpolate (sideways -> forwards)
				else
					dirMultiplier = _backwardsMultiplier + (dot + 1.0f) * (_sidewaysMultiplier - _backwardsMultiplier);	// interpolate (sideways -> backwards)
			}

			float moveScalar = (_velocity * time.GetDeltaTime()) * dirMultiplier;
			moveVec = XMVectorScale(XMVector3Normalize(moveVec), moveScalar);
			XMFLOAT3A move; Store(move, moveVec);

			// Move the entity
			if (!_isHiding)
			{
				playerT->Move(move, Local);
				_movementDir = _lastMoveDir = move;
				_lastMoveSpeed = moveScalar;
			}		

			if (!_flyMode)
			{
				float playerRadius = static_cast<const Collisions::Capsule *>(_playerCollider->GetCollider())->radius;
				XMFLOAT3 playerPos = playerT->GetPosition(World);

				Collisions::Circle c = { {playerPos.x, playerPos.z}, playerRadius * 0.5f };
				XMFLOAT2 n1;
				float d;
				if (Collisions::CircleTerrainWallIntersection(c, *_terrainWalls, d, n1))
				{
					playerT->Move({ n1.x * d, 0, n1.y * d }, World);
				}

				// Footstep audio
				if (_isRunning)			_stepInterval = 0.3f;
				else if (_isSneaking)	_stepInterval = 0.6f;
				else					_stepInterval = 0.45f;

				_stepTimer -= time.GetDeltaTime();
				if (_stepTimer <= 0)
				{
					// Get a random index
					int soundIndex = rand() % _steps.size();

					// Play the sound
					_steps[soundIndex]->SetEnabled(true);
					_steps[soundIndex]->Play();

					// Reset the timer
					_stepTimer = 0;
					_stepTimer += _stepInterval;
				}
			}
		}
		else
		{
			// Entity is not moving
			_velocity = 0.0f;
			_isStill = true;
			_isWalking = false;
			_isRunning = false;
			_movementDir = { 0.0f, 0.0f, 0.0f };
			if (!_isSneaking && !_isHiding) 
			{
				// reset head bobbin when idle
				headTrackerT->SetPosition({ 0, _cameraHeight * 0.85f, 0 });
				_stepProgress = 0;

			}
		}

#ifdef DEBUG_BUILD
		if (input.GetKey(KeyCode::Z) == KeyState::Pressed)
		{
			_flyMode = !_flyMode;
			_currentBreathSound = nullptr;
		}
#endif
	}
	else
	{
		if (_currentBreathSound)
		{
			_currentBreathSound->Pause();
		}
	}

	return true;
}

bool PlayerMovementBehaviour::LateUpdate(TimeUtils &time, const Input &input)
{
	if (_flyMode)
		return true;

	GetTransform()->Move({ 0, -5.0f * std::clamp(time.GetDeltaTime(), 0.0f, 0.05f), 0}, World); // "Gravity"

	Transform *playerT = GetTransform();
	float playerHeight = static_cast<const Collisions::Capsule *>(_playerCollider->GetCollider())->height;

	if (_terrainRoof)
	{
		XMFLOAT3 playerPos = playerT->GetPosition(World);
		XMFLOAT3 rayPos = playerPos;

		XMFLOAT3A n{}, roofPoint{};
		float d;
		bool under;

		rayPos.y -= 200;
		Collisions::Ray rayUp = Collisions::Ray(rayPos, { 0, 1.0f, 0 }, 1000);
		bool collidingWithRoof = Collisions::TerrainRayIntersectionVertical(*_terrainRoof, rayUp, n, roofPoint, d, under);

		float newY = roofPoint.y - playerHeight;
		if (collidingWithRoof && playerPos.y > newY)
		{
			playerT->SetPosition({ playerPos.x, newY, playerPos.z });
		}
	}

	if (_terrainFloor)
	{
		XMFLOAT3 playerPos = playerT->GetPosition(World);
		XMFLOAT3 rayPos = playerPos;

		XMFLOAT3A n{}, floorPoint{};
		float d;
		bool under;

		rayPos.y += 200;
		Collisions::Ray rayDown = Collisions::Ray(rayPos, { 0, -1.0f, 0 }, 1000);
		bool collidingWithFloor = Collisions::TerrainRayIntersectionVertical(*_terrainFloor, rayDown, n, floorPoint, d, under);
		
		if (collidingWithFloor && playerPos.y < floorPoint.y)
		{
			playerT->SetPosition({ playerPos.x, floorPoint.y, playerPos.z }, World);
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool PlayerMovementBehaviour::RenderUI()
{
	ImGui::Checkbox("Fly Mode", &_flyMode);

	// Value modification
	ImGui::InputFloat("Walk Velocity", &_walkVelocity, 0.01f, 1.0f);
	ImGui::InputFloat("Run Velocity", &_runVelocity, 0.01f, 1.0f);
	ImGui::InputFloat("Sneak Velocity", &_sneakVelocity, 0.01f, 1.0f);
	ImGui::InputFloat("Stamina Drain Rate", &_staminaDrainRate, 1.0f, 1.0f);
	ImGui::InputFloat("Stamina Recovery Rate", &_staminaRecoveryRate, 1.0f, 1.0f);

	ImGui::InputFloat("Forwards Multiplier", &_forwardsMultiplier, 0.01f, 1.0f);
	ImGui::InputFloat("Sideways Multiplier", &_sidewaysMultiplier, 0.01f, 1.0f);
	ImGui::InputFloat("Backwards Multiplier", &_backwardsMultiplier, 0.01f, 1.0f);

	ImGui::SliderFloat("Solid Footing Dot", &_solidFootingDot, 0.0f, 1.0f);

	// Value display
	std::string staminaText = "Stamina: " + std::to_string(_stamina);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), staminaText.c_str());

	std::string speedText = "Velocity: " + std::to_string(_velocity);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), speedText.c_str());

	std::string directionText = "Direction: " + std::to_string(_movementDir.x) + ", " + std::to_string(_movementDir.y) + ", " + std::to_string(_movementDir.z);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), directionText.c_str());

	Transform *playerT = GetEntity()->GetTransform();
	XMFLOAT3A	localForward = playerT->GetForward();
	std::string forwardText = "Forward: " + std::to_string(localForward.x) + ", " + std::to_string(localForward.y) + ", " + std::to_string(localForward.z);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), forwardText.c_str());

	if (ImGui::Button("Reset Player"))
	{
		XMFLOAT3A point{};
		GetScene()->GetGraphManager()->GetClosestPoint(playerT->GetPosition(World), point);
		GetTransform()->SetPosition(point, World);
	}
	
	if (ImGui::Button("Simulated Catch"))
	{
		if (!Catch())
			Warn("Failed to simulate catch!");
	}

	return true;
}
#endif

bool PlayerMovementBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Stamina", _stamina, docAlloc);
	obj.AddMember("Hiding", _isHiding, docAlloc);

	return true;
}
bool PlayerMovementBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_stamina	= obj["Stamina"].GetFloat();
	_isHiding	= obj["Hiding"].GetBool();

	return true;
}
void PlayerMovementBehaviour::PostDeserialize()
{
	// Find player view camera, sibling of this entity

	Entity *ent = GetEntity();
	if (!ent)
		return;

	Entity *parent = ent->GetParent();
	if (!parent)
		return;

	SetHeadTracker(GetScene()->GetSceneHolder()->GetEntityByName("Player Head Tracker"));

	const std::vector<Entity *> *children = parent->GetChildren();
	for (Entity *child : *children)
	{
		if (child == ent)
			continue; // Skip self

		if (!child->HasBehaviourOfType<PlayerViewBehaviour>())
			continue;

		CameraBehaviour *playerCamera = nullptr;
		if (!child->GetBehaviourByType<CameraBehaviour>(playerCamera))
			continue;

		// Found the player camera
		SetPlayerCamera(playerCamera);
		break; 
	}
}

bool PlayerMovementBehaviour::IsLocked() const
{
	return _isLocked;
}
void PlayerMovementBehaviour::Lock()
{
	_isLocked = true;
}
void PlayerMovementBehaviour::Unlock()
{
	_isLocked = false;
}

void PlayerMovementBehaviour::AdjustForCollision(const Collisions::CollisionData &data)
{
	if (_flyMode)
		return;

	using namespace DirectX;

	bool hasSolidFooting = data.normal.y > 1.0f - _solidFootingDot;
	float div = 1.0f;

	XMFLOAT3A move = {0, 0, 0};
	if (hasSolidFooting) // Prevent the player from sliding by converting horizontal movement to additional vertical movement
	{
		move.y = data.depth * div;
	}
	else
	{
		Store(move, XMVectorScale(XMVector3Normalize(XMLoadFloat3(&data.normal)), data.depth * div));
	}

	Transform *t = GetEntity()->GetTransform();
	t->Move(move, World);
}
bool PlayerMovementBehaviour::GetTerrain()
{
	auto sceneHolder = GetScene()->GetSceneHolder();
	Entity *ent = nullptr;

	if (ent = sceneHolder->GetEntityByName("Terrain Floor"))
	{
		if (ent->GetBehaviourByType<const ColliderBehaviour>(_floorColliderBehaviour))
		{
			_terrainFloor = dynamic_cast<const Collisions::Terrain *>(_floorColliderBehaviour->GetCollider());
		}
	}

	if (ent = sceneHolder->GetEntityByName("Terrain Roof"))
	{
		if (ent->GetBehaviourByType<const ColliderBehaviour>(_roofColliderBehaviour))
		{
			_terrainRoof = dynamic_cast<const Collisions::Terrain *>(_roofColliderBehaviour->GetCollider());
		}
	}

	if (ent = sceneHolder->GetEntityByName("Terrain Walls"))
	{
		if (ent->GetBehaviourByType<const ColliderBehaviour>(_wallsColliderBehaviour))
		{
			_terrainWalls = dynamic_cast<const Collisions::Terrain *>(_wallsColliderBehaviour->GetCollider());
		}
	}

	return _terrainFloor != nullptr && _terrainRoof != nullptr && _terrainWalls != nullptr;
}

void PlayerMovementBehaviour::RespawnPlayer()
{
	if (!_spawnPositions.empty())
	{
		GetTransform()->SetPosition(_spawnPositions[rand() % _spawnPositions.size()], World);
	}
	else
	{
		GetTransform()->SetPosition(_spawnPosition);
	}

	_canRun = true;
	_isStill = true;
	_isSneaking = false;
	_isWalking = false;
	_isRunning = false;
	_isHiding = false;
}

void PlayerMovementBehaviour::SetHiding(bool value)
{
	_isHiding = value;
}
void PlayerMovementBehaviour::SetStill(bool value)
{
	_isStill = value;
}
void PlayerMovementBehaviour::SetSneaking(bool value)
{
	_isSneaking = value;
}
void PlayerMovementBehaviour::SetWalking(bool value)
{
	_isWalking = value;
}
void PlayerMovementBehaviour::SetRunning(bool value)
{
	_isRunning = value;
}
bool PlayerMovementBehaviour::IsHiding() const
{
	return _isHiding;
}
bool PlayerMovementBehaviour::IsStill() const
{
	return _isStill;
}
bool PlayerMovementBehaviour::IsSneaking() const
{
	return _isSneaking;
}
bool PlayerMovementBehaviour::IsWalking() const
{
	return _isWalking;
}
bool PlayerMovementBehaviour::IsRunning() const
{
	return _isRunning;
}
bool PlayerMovementBehaviour::IsFlying() const
{
	return _flyMode;
}

bool PlayerMovementBehaviour::MoveToPoint(TimeUtils &time, dx::XMFLOAT3 point, float velocity, float threshold2)
{
	XMVECTOR moveVec = XMVectorSubtract(Load(point), Load(GetTransform()->GetPosition(World)));
	if (XMVectorGetX(XMVector3Dot(moveVec, moveVec)) < threshold2)
		return true;

	moveVec = XMVector3Normalize(moveVec);
	float moveScalar = (velocity * time.GetDeltaTime());

	moveVec = XMVectorScale(XMVector3Normalize(moveVec), moveScalar);

	// Move the entity
	if (!_collidingWithWall && !_isHiding)
	{
		XMFLOAT3A move; Store(move, moveVec);
		GetTransform()->Move(move, Local);
		_movementDir = move;
		_lastMoveDir = move;
		_lastMoveSpeed = moveScalar;
	}

	if (_isRunning || velocity >= _runVelocity)
	{
		_stepInterval = 0.3f;
	}
	else if (_isSneaking || velocity <= _sneakVelocity)
	{
		_stepInterval = 0.6f;
	}
	else
	{
		_stepInterval = 0.45f;
	}
	
	_stepTimer -= time.GetDeltaTime();
	if (_stepTimer <= 0)
	{
		// Get a random index
		int soundIndex = rand() % _steps.size();

		// Play the sound
		_steps[soundIndex]->SetEnabled(true);
		_steps[soundIndex]->Play();

		// Reset the timer
		_stepTimer = 0;
		_stepTimer += _stepInterval;
	}

	return false;
}

bool PlayerMovementBehaviour::SetPlayerCamera(CameraBehaviour *camera)
{
	if (!camera)
	{
		ErrMsg("Failed to set player camera, camera is nullptr");
		return false;
	}

	_playerCamera = camera;
	GetEntity()->GetBehaviourByType<ColliderBehaviour>(_playerCollider);

	if (_playerCollider)
	{
		_playerCollider->AddOnIntersection([this](const Collisions::CollisionData &data) { 
			AdjustForCollision(data); 
		});
	}

	return true;
}
void PlayerMovementBehaviour::SetHeadTracker(Entity *ent)
{
	_headTrackerEntity = ent;
}

Transform *PlayerMovementBehaviour::GetPlayerCameraDesiredPos() const
{
	return _headTrackerEntity->GetTransform();
}
float PlayerMovementBehaviour::GetCameraHeight() const
{
	return _cameraHeight;
}

bool PlayerMovementBehaviour::IsCaught() const
{
	return _isCaught;
}
bool PlayerMovementBehaviour::Catch()
{
	if (_flyMode)
		return false;

	_caughtStage = 0;
	_isCaught = true;
	_caughtStageTimer = 0.25f;
	GetScene()->GetPlayerCamera()->BeginScreenFade(_caughtStageTimer);
	_collapseSound->ResetSound();
	_collapseSound->Play();
	return true;
}
void PlayerMovementBehaviour::PlayCaughtAnimnation(TimeUtils &time)
{
	Scene *scene = GetScene();

	switch (_caughtStage)
	{
	case 0: 
		// Wait for fade to black
		if (_caughtStageTimer <= 0)
		{
			// End of stage, prepare next stage
			_caughtStage++;
			_caughtStageTimer += 3.0f;

			if (_flashlight)
				_flashlight->Disable();

			InventoryBehaviour* inventory = nullptr;
			if (GetEntity()->GetBehaviourByType<InventoryBehaviour>(inventory))
			{
				inventory->SetHeldEnabled(false);
				inventory->SetEnabled(false);
			}
		}
		break;

	case 1: 
		// Sustain black screen
		if (_caughtStageTimer <= 0)
		{
			// End of stage, prepare next stage
			RespawnPlayer();

			auto playerCam = scene->GetPlayerCamera();
			auto animCam = scene->GetAnimationCamera();

			animCam->SetScreenFadeManual(1.0f);
			animCam->BeginScreenFade(-2.0f);
			animCam->GetEntity()->Enable();

			if (scene->GetViewCamera() == playerCam)
			{
				scene->SetViewCamera(animCam);

				auto animCamEnt = animCam->GetEntity();
				auto animCamTrans = animCam->GetTransform();

				animCamTrans->SetPosition({ -269.3f, -1.95f, -73.98f }, World);
				animCamTrans->SetEuler({ -12.4802f * DEG_TO_RAD, -70.137f * DEG_TO_RAD, 20.0f * DEG_TO_RAD }, World);

				RestrictedViewBehaviour *restrictedViewBehaviour;
				if (animCamEnt->GetBehaviourByType<RestrictedViewBehaviour>(restrictedViewBehaviour))
				{
					restrictedViewBehaviour->SetEnabled(true);
					restrictedViewBehaviour->SetAllowedOffset(15.0f);
					restrictedViewBehaviour->SetViewDirection(
						animCamTrans->GetEuler(World), 
						animCamTrans->GetForward(World), 
						animCamTrans->GetUp(World)
					);
				}

				SimplePointLightBehaviour *lightBehaviour;
				if (animCamEnt->GetBehaviourByType<SimplePointLightBehaviour>(lightBehaviour))
				{
					lightBehaviour->SetEnabled(true);
				}

				SoundBehaviour *dragSound;
				if (animCamEnt->GetBehaviourByType<SoundBehaviour>(dragSound))
				{
					dragSound->SetEnabled(true);
					dragSound->ResetSound();
					dragSound->Play();
				}
			}

			_caughtStage++;
			_caughtStageTimer += 7.5f;
		}
		break;

	case 2:
		// Dragging animation
		{
			auto animCam = scene->GetAnimationCamera();

			float moveSpeed = time.GetDeltaTime() * powf(
				std::clamp(
					(sinf((_caughtStageTimer - 0.5f) * 2.75f - 0.5f) + 0.9f) / 1.9f,
					0.0f, 1.0f),
				0.25f
			);

			auto animCamTrans = animCam->GetTransform();
			animCamTrans->Move({ 1.25f * moveSpeed, 0, 0 }, World);

			SoundBehaviour *dragSound;
			if (animCam->GetEntity()->GetBehaviourByType<SoundBehaviour>(dragSound))
			{
				dragSound->Play();
			}

			XMFLOAT3A distortionOrigin = animCamTrans->GetPosition(World);
			distortionOrigin.y += 1.5f;
			distortionOrigin.x += 1.5f;

			auto graphics = scene->GetGraphics();
			graphics->SetDistortionOrigin(distortionOrigin);
			graphics->SetDistortionStrength(10.0f);

			if (_caughtStageTimer <= 2.0f)
			{
				animCam->BeginScreenFade(2.0f);
			}
		}

		if (_caughtStageTimer <= 0)
		{
			// End of stage, prepare next stage
			auto animCam = scene->GetAnimationCamera();
			auto animCamEnt = animCam->GetEntity();

			RestrictedViewBehaviour *restrictedViewBehaviour;
			if (animCamEnt->GetBehaviourByType<RestrictedViewBehaviour>(restrictedViewBehaviour))
			{
				restrictedViewBehaviour->SetEnabled(false);
			}

			SimplePointLightBehaviour *lightBehaviour;
			if (animCamEnt->GetBehaviourByType<SimplePointLightBehaviour>(lightBehaviour))
			{
				lightBehaviour->SetEnabled(false);
			}

			_caughtStage++;
			_caughtStageTimer += 2.0f;
		}
		break;

	case 3: 
		// Reset camera, wait
		if (_caughtStageTimer <= 0)
		{
			auto animCam = scene->GetAnimationCamera();

			// End of stage, prepare next stage
			if (scene->GetViewCamera() == animCam)
			{
				auto playerCam = scene->GetPlayerCamera();
				playerCam->SetScreenFadeManual(1.0f);
				scene->SetViewCamera(playerCam);
			}

			animCam->SetScreenFadeManual(0.0f);
			animCam->GetEntity()->Disable();

			_caughtStage++;
			_caughtStageTimer += 1.0f;
		}
		break;

	case 4: 
		// Fade in
		if (_caughtStageTimer <= 0)
		{
			scene->GetPlayerCamera()->BeginScreenFade(-2.5f);

			// End of stage, finish caught sequence
			_caughtStage = 0;
			_isCaught = false;
			_caughtStageTimer = 0.0f;

			if (_flashlight)
			{
				_flashlight->Enable();
			}

			InventoryBehaviour* inventory = nullptr;
			if (GetEntity()->GetBehaviourByType<InventoryBehaviour>(inventory))
			{
				inventory->SetHeldEnabled(true);
				inventory->SetEnabled(true);
			}

			auto monster = GetScene()->GetMonster();
			monster->GetEntity()->Enable();
			monster->ResetMonster();
		}
		break;
	}

	_caughtStageTimer -= time.GetDeltaTime();
}
