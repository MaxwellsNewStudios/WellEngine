#include "stdafx.h"
#include "Behaviours/MonsterBehaviour.h"
#include "Scenes/Scene.h"
#include "Behaviours/MonsterStates/MonsterState.h"
#include "Behaviours/MonsterStates/MonsterIdle.h"
#include "Behaviours/MonsterStates/MonsterHunt.h"
#include "Behaviours/MonsterStates/MonsterHuntDone.h"
#include "Behaviours/MonsterStates/MonsterExtendedHunt.h"
#include "Behaviours/MonsterStates/MonsterCapture.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "Collision/Intersections.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

static std::string ToString(MonsterStatus state)
{
	switch (state)
	{
	case MonsterStatus::IDLE:			return "Idle";
	case MonsterStatus::HUNT:			return "Hunt";
	case MonsterStatus::HUNT_DONE:		return "Hunt Done";
	case MonsterStatus::EXTENDED_HUNT:	return "Extended Hunt";
	case MonsterStatus::CAPTURE:		return "Capture";
	default:							return "Unknown";
	}
}

bool MonsterBehaviour::AnyStateUpdate(TimeUtils &time)
{	
	// Update player noise range
	if (_playerMover->IsHiding())		
	{
		_playerNoiseRange = (int)NoiseRange::Hiding;
		_playerNoiseLevel = (int)NoiseLevel::Hiding;
	}
	else if (_playerMover->IsStill())		
	{
		_playerNoiseRange = (int)NoiseRange::Still;		
		_playerNoiseLevel = (int)NoiseLevel::Still;
	}
	else if (_playerMover->IsSneaking())	
	{
		_playerNoiseRange = (int)NoiseRange::Sneaking;	
		_playerNoiseLevel = (int)NoiseLevel::Sneaking;
	}
	else if (_playerMover->IsWalking())		
	{
		_playerNoiseRange = (int)NoiseRange::Walking;	
		_playerNoiseLevel = (int)NoiseLevel::Walking;
	}
	else if (_playerMover->IsRunning())		
	{
		_playerNoiseRange = (int)NoiseRange::Running;	
		_playerNoiseLevel = (int)NoiseLevel::Running;
	}

	XMFLOAT3A playerPos = _playerEntity->GetTransform()->GetPosition(World);
	_playerDistance = CalculateDistance(playerPos, GetTransform()->GetPosition(World));
	_playerInSight = HasLOS(playerPos) && _playerDistance <= _sightRange && !_playerMover->IsHiding();

	// Alert: Growth/Decay
	if (_playerInSight)
		GrowAlert(time.GetDeltaTime());
	else
		DecayAlert(time.GetDeltaTime());

	// Capture player if the criteria are met
	if (!_playerMover->IsFlying () && _playerDistance < _playerCaptureDistance)
	{
		if (!_playerMover->IsHiding())
			SetState(CAPTURE);
		else if (_sawPlayerHide)
			SetState(CAPTURE);
	}

	bool controlDistortion = true;
	PlayerMovementBehaviour *pmb;
	if (GetScene()->GetPlayer())
	{
		if (GetScene()->GetPlayer()->GetBehaviourByType<PlayerMovementBehaviour>(pmb))
		{
			controlDistortion = !pmb->IsCaught();
		}
	}

	if (controlDistortion)
	{
		// Update Distortion
		float distortionLerp = _alertLevel / 100.0f;

		_distortionStrength = _distortionAmplifier * _maxDistortion * std::lerp(distortionLerp, 0.65f, 1.0f);
		_distortionStrength = std::min<float>(_distortionStrength, _distortionAmplifier * _maxDistortion);

		if (_distortionStrength > 0.1f)
		{
			GetScene()->GetGraphics()->SetDistortionOrigin(GetTransform()->GetPosition(World));
			GetScene()->GetGraphics()->SetDistortionStrength(_distortionStrength);
		}
		else
		{
			GetScene()->GetGraphics()->SetDistortionOrigin({ 0, 0, 0 });
			GetScene()->GetGraphics()->SetDistortionStrength(0);
		}
	}

	// Set ambient listener
	_soundBehaviours[HUNTING_AMBIENT]->SetListenerPosition(playerPos);
	_soundBehaviours[HUNTING_AMBIENT]->Play();

	return true;
}

void MonsterBehaviour::MoveToPoint(TimeUtils &time, const XMFLOAT3A &point)
{
	XMFLOAT3A pos = GetTransform()->GetPosition(World);

	XMVECTOR posV = Load(pos),
			 pointV = Load(point);

	XMVECTOR moveDirV = XMVectorSubtract(pointV, posV);
	moveDirV = XMVector3Normalize(moveDirV);

	XMFLOAT4A moveDir;
	Store(moveDir, XMVectorScale(moveDirV, _moveSpeed * _movementFactor * time.GetDeltaTime()));

	GetTransform()->Move(moveDir, World);
}

float MonsterBehaviour::CalculateDistance(const XMFLOAT3A &point1, const XMFLOAT3A &point2)
{
	XMVECTOR pos1 = Load(point1),
			 pos2 = Load(point2);

	XMVECTOR diff = XMVectorSubtract(pos1, pos2);
	return XMVectorGetX(XMVector3LengthEst(diff));
}

bool MonsterBehaviour::HasLOS(XMFLOAT3A &point)
{
	// TODO: Cast multiple rays at player

	XMFLOAT3A origin = GetTransform()->GetPosition();
	origin.y += 1.6f; // offset from feet
	XMFLOAT3A playerPos = _playerEntity->GetTransform()->GetPosition();
	playerPos.y += 1.2f; // offset from feet
	XMFLOAT3 dir;
	XMVECTOR dirVec = Load(playerPos) - Load(origin);

	Store(dir, XMVector3Normalize(dirVec));
	float dist;
	XMStoreFloat(&dist, XMVector3LengthEst(dirVec));

	Collisions::Ray rayCollider = Collisions::Ray(origin, dir, dist);
	XMFLOAT3 p, n;
	float depth;
#ifdef DEBUG_BUILD
	//rayCollider.DebugRayRender(GetScene(), { 1.0f, 0.0f, 0.0f, 1.0f });
#endif

	bool hit = Collisions::TerrainRayIntersection(
		*GetScene()->GetTerrain(), rayCollider,
		n, p, depth
	);

	return !hit;
}

bool MonsterBehaviour::SetState(MonsterStatus newState)
{
	if (_state == newState)
		return true;

	switch (_state)
	{
	case MonsterStatus::IDLE:
	case MonsterStatus::HUNT:
	case MonsterStatus::HUNT_DONE:
	case MonsterStatus::CAPTURE:
		_monsterStates[_state]->InitialOnExit();
		break;
	default: 
		ErrMsg("Error while calling exit on current state: " + ToString(_state)); 
		return false;
	}

	_state = newState;

	switch (_state)
	{
	case MonsterStatus::IDLE:
	case MonsterStatus::HUNT:
	case MonsterStatus::HUNT_DONE:
	case MonsterStatus::CAPTURE:
		_monsterStates[_state]->InitialOnEnter();
		break;
	default: 
		ErrMsg("Error while calling enter on new state: " + ToString(_state)); 
		return false;
	}

	DbgMsg("State is now " + ToString(_state));

	return true;
}

bool MonsterBehaviour::DecayAlert(float deltaTime)
{
	if (_alertLevel > 0)
		_alertLevel -= deltaTime * _alertDecayRate;
	return true;
}

bool MonsterBehaviour::GrowAlert(float deltaTime)
{
	if (_alertLevel < 100.0f)
		_alertLevel += deltaTime * _alertGrowthRate;
	return true;
}

void MonsterBehaviour::GetRandomizedNodePos(XMFLOAT3A &pos, bool mineOnly)
{
	// Randomize path towards a node
	std::vector<GraphNodeBehaviour *> nodes;
	if (mineOnly)
		_graphManager->GetMineNodes(nodes);
	else
		_graphManager->GetNodes(nodes);

	if (nodes.size() > 0)
	{
		GraphNodeBehaviour *node = nodes[rand() % nodes.size()];
		pos = node->GetTransform()->GetPosition();
	}
}

void MonsterBehaviour::ResetMonster()
{
	if (_isResetting)
		return;

	_isResetting = true;

	_state = MonsterStatus::IDLE;

	_alertLevel = 0;

	// Reset position
	if (!_spawnPositions.empty())
	{
		GetTransform()->SetPosition(_spawnPositions[rand() % _spawnPositions.size()], World);
	}
	else
	{
		GetTransform()->SetPosition(_spawnPosition, World);
	}

	switch (_currentSound)
	{
	case HUNTING1:
	case HUNTING2:
	case HUNTING3:
	case HUNTING_AMBIENT:
	case HEART_SLOW:
	case HEART_MEDIUM:
	case HEART_FAST:
		_soundBehaviours[_currentSound]->Pause();
		_soundBehaviours[_currentSound]->SetEnabled(false);
		_currentSound = MonsterSound::NULL_SOUND;
		break;
	case SOUND_COUNT:
	case NULL_SOUND:
	default:
		break;
	}

	_isResetting = false;
}

void MonsterBehaviour::UpdatePathToPlayer()
{
	XMFLOAT3A pos = GetTransform()->GetPosition(World);

	Transform *t = _playerEntity->GetTransform();
	XMFLOAT3A playerPos = t->GetPosition(World);

	_path.clear();
	_graphManager->GetPath(pos, playerPos, &_path);

	_path.erase(_path.begin());
}

void MonsterBehaviour::UpdatePathIdle()
{
	XMFLOAT3A playerPos;
	if (!_playerEntity || !GetScene()->GetPlayer())
	{
		if (!GetScene()->GetPlayer())
			return;
	}
	playerPos = GetScene()->GetPlayer()->GetTransform()->GetPosition(World);

	//// -------------- Player Quadrant based
	//// Q1 | Q2
	//// ---+----
	//// Q3 | Q4

	//XMFLOAT3 randomizedPoint{ 0.0f, 0.0f, 0.0f };
	//if (playerPos.x < 0.0f && playerPos.z > 0.0f) // Q1
	//{
	//	randomizedPoint.x -= rand() % 200;
	//	randomizedPoint.z += rand() % 250;
	//}
	//else if (playerPos.x > 0.0f && playerPos.z > 0.0f) // Q2
	//{
	//	randomizedPoint.x += rand() % 300;
	//	randomizedPoint.z += rand() % 300;
	//}
	//else if (playerPos.x < 0.0f && playerPos.z < 0.0f) // Q3
	//{
	//	randomizedPoint.x -= rand() % 300;
	//	randomizedPoint.z -= rand() % 250;
	//}
	//else if (playerPos.x > 0.0f && playerPos.z < 0.0f) // Q4
	//{
	//	randomizedPoint.x += rand() % 300;
	//	randomizedPoint.z -= rand() % 300;
	//}
	//XMFLOAT3 monsterPos = GetTransform()->GetPosition(World);
	//XMFLOAT3 wanderPoint;
	//_graphManager->GetClosestPoint(randomizedPoint, wanderPoint);

	//_path.clear();
	//_graphManager->GetPath(monsterPos, wanderPoint, &_path);
	//_path.erase(_path.begin());

	// ------------ Player Radius based

	float radius = 130.0f - _alertLevel;

	// Generate a random angle (0 -> 2pi)
	float theta = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f * XM_PI;

	XMFLOAT3 randomizedPoint;
	randomizedPoint.x = playerPos.x + radius * std::cos(theta);
	randomizedPoint.y = playerPos.y;
	randomizedPoint.z = playerPos.z + radius * std::sin(theta);

	XMFLOAT3 wanderPoint;
	_graphManager->GetClosestPoint(randomizedPoint, wanderPoint);

	XMFLOAT3 monsterPos = GetTransform()->GetPosition();

	_path.clear();
	_graphManager->GetPath(monsterPos, wanderPoint, &_path);
	_path.erase(_path.begin());
}

void MonsterBehaviour::PathFind(TimeUtils &time)
{
	static bool nodeUpdate = false;

	// Traverse between nodes in _path (node vector)
	if (!_path.empty())
	{
		XMVECTOR posToPointVec = XMVectorSubtract(Load(_path[0]), Load(GetTransform()->GetPosition(World)));
		XMVECTOR lengthVec = XMVector3LengthEst(posToPointVec);

		float length = 0;
		XMStoreFloat(&length, lengthVec);

		if (length < 0.5f)
		{
			// Reaches a node -> Remove it
			if (!_path.empty())
			{
				_path.erase(_path.begin());
				nodeUpdate = true;
			}
		}
		else
		{
			nodeUpdate = false;
		}

		if (!_path.empty())
		{
			// if next node just got updated or if path just got updated
			if (nodeUpdate)
			{
				// Determine properties of prior node and next node
				_priorNodeWasCave = _nextNodeIsCave;
				if (_graphManager->isPoint(_path[0]) && !_graphManager->isMinePoint(_path[0])) _nextNodeIsCave = true;
				else _nextNodeIsCave = false;

				// Set movement penalty based on prior node and next node
				if (_priorNodeWasCave && _nextNodeIsCave) _movementFactor = _caveMovementFactor;
				else _movementFactor = _mineMovementFactor;
			}

			MoveToPoint(time, To3(_path[0]));

			// Display first node
			//XMFLOAT3 p1 = _path[0];
			//XMFLOAT3 p2 = p1;
			//p2.y += 10.0f;
			//GetScene()->GetDebugDrawer()->DrawLine(p1, p2, 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f }, false);
		}
	}
}


bool MonsterBehaviour::Start()
{
	if (_name.empty())
		_name = "MonsterBehaviour"; // For categorization in ImGui.

	_distortionStrength = 0;

	GetScene()->GetGraphics()->SetDistortionStrength(0);

	GetScene()->SetMonster(this);

	// Spawn position if serialized
	//_spawnPosition = GetTransform()->GetPosition();

	_spawnPosition = { -122.5f, -15.6f, 5.7f }; // For testing
	GetTransform()->SetPosition(_spawnPosition, World);

	_spawnPositions.emplace_back(-122.0f, -15.0f, 5.0f );
	_spawnPositions.emplace_back( -122.0f, -3.0f, 166.0f );
	_spawnPositions.emplace_back( 173.0f, 11.0f, 92.0f );
	_spawnPositions.emplace_back( 165.0f, 3.0f, -88.0f );
	_spawnPositions.emplace_back( -94.0f, -11.0f, -204.0f );
	_spawnPositions.emplace_back( -284.0f, -1.0f, -112.0f );
	
	// Get player
	_playerEntity = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity");

	_graphManager = GetScene()->GetGraphManager();

	// Get relevant player behaviours
	if (_playerEntity)
	{
		if (!_playerEntity->GetBehaviourByType<PlayerMovementBehaviour>(_playerMover))
		{
			ErrMsg("Player entity missing 'PlayerMovementBehaviour'");
			return false;
		}

		Entity* flashlight = GetScene()->GetSceneHolder()->GetEntityByName("Flashlight");
		if (!flashlight)
		{
			ErrMsg("Could not find flashlight");
			return false;
		}

		if (!flashlight->GetBehaviourByType<FlashlightBehaviour>(_playerFlashlight))
		{
			ErrMsg("Player entity missing 'FlashlightBehaviour'");
			return false;
		}
	}

	Content *content = GetScene()->GetContent();
	if (!content)
	{
		ErrMsg("Failed to get scene content.");
		return false;
	}
	
	// Set mesh
	UINT meshID = content->GetMeshID("DecimatedMan");
	Material mat{};
	mat.samplerID = content->GetShaderID("Point");
	mat.psID = content->GetShaderID("PS_Static");

	dx::BoundingOrientedBox bounds = content->GetMesh(meshID)->GetBoundingOrientedBox();
	
	GetEntity()->SetEntityBounds(bounds);
	
	_meshBehaviour = new MeshBehaviour(bounds, meshID, &mat);

	if (!_meshBehaviour->Initialize(GetEntity()))
	{
		ErrMsg("Failed to bind mesh to entity '" + GetName() + "'!");
		return false;
	}  
	_meshBehaviour->SetAlphaCutoff(0.4f);

	// Load states
	{
		_monsterStates[MonsterStatus::IDLE] = new MonsterIdle();
		_monsterStates[MonsterStatus::IDLE]->Intitalize(this);

		_monsterStates[MonsterStatus::HUNT] = new MonsterHunt();
		_monsterStates[MonsterStatus::HUNT]->Intitalize(this);

		_monsterStates[MonsterStatus::HUNT_DONE] = new MonsterHuntDone();
		_monsterStates[MonsterStatus::HUNT_DONE]->Intitalize(this);

		_monsterStates[MonsterStatus::EXTENDED_HUNT] = new MonsterExtendedHunt();
		_monsterStates[MonsterStatus::EXTENDED_HUNT]->Intitalize(this);

		_monsterStates[MonsterStatus::CAPTURE] = new MonsterCapture();
		_monsterStates[MonsterStatus::CAPTURE]->Intitalize(this);
	}

	// load sounds
	{
		_soundBehaviours[HUNTING1] = new SoundBehaviour("Monster 1 Raw", SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HUNTING1]->SetName("SoundBehaviour (Hunting1)");
		_soundBehaviours[HUNTING1]->SetSerialization(false);
		_soundVolumes[HUNTING1] = 5.0f;

		_soundBehaviours[HUNTING2] = new SoundBehaviour("Monster 2 Raw", SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HUNTING2]->SetName("SoundBehaviour (Hunting2)");
		_soundBehaviours[HUNTING2]->SetSerialization(false);
		_soundVolumes[HUNTING2] = 5.0f;

		_soundBehaviours[HUNTING3] = new SoundBehaviour("Monster 3 Raw", SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HUNTING3]->SetName("SoundBehaviour (Hunting3)");
		_soundBehaviours[HUNTING3]->SetSerialization(false);
		_soundVolumes[HUNTING3] = 5.0f;

		_soundBehaviours[HUNTING_AMBIENT] = new SoundBehaviour("Felt_Static", SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HUNTING_AMBIENT]->SetName("SoundBehaviour (Hunting Ambient)");
		_soundBehaviours[HUNTING_AMBIENT]->SetSerialization(false);
		_soundVolumes[HUNTING_AMBIENT] = 0.1f;

		_soundBehaviours[HEART_SLOW] = new SoundBehaviour("HeartbeatSlow", SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HEART_SLOW]->SetName("SoundBehaviour (Heart Slow)");
		_soundBehaviours[HEART_SLOW]->SetSerialization(false);
		_soundVolumes[HEART_SLOW] = 0.2f;

		_soundBehaviours[HEART_MEDIUM] = new SoundBehaviour("HeartbeatMedium", SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HEART_MEDIUM]->SetName("SoundBehaviour (Heart Medium)");
		_soundBehaviours[HEART_MEDIUM]->SetSerialization(false);
		_soundVolumes[HEART_MEDIUM] = 0.2f;

		_soundBehaviours[HEART_FAST] = new SoundBehaviour("HeartbeatFast", SoundEffectInstance_ReverbUseFilters, true);
		_soundBehaviours[HEART_FAST]->SetName("SoundBehaviour (Heart Fast)");
		_soundBehaviours[HEART_FAST]->SetSerialization(false);
		_soundVolumes[HEART_FAST] = 0.2f;

		for (int i = 0; i < SOUND_COUNT; i++)
		{
			SoundBehaviour *sound = _soundBehaviours[i];
			if (!sound->Initialize(GetEntity()))
			{
				ErrMsg("Failed to Initialize sound behaviour!");
				return false;
			}

			sound->SetVolume(_soundVolumes[i]);
		}

		_soundBehaviours[HUNTING1]->SetEnabled(false);
		_soundBehaviours[HUNTING2]->SetEnabled(false);
		_soundBehaviours[HUNTING3]->SetEnabled(false);

		_soundBehaviours[HEART_SLOW]->SetEnabled(false);
		_soundBehaviours[HEART_MEDIUM]->SetEnabled(false);
		_soundBehaviours[HEART_FAST]->SetEnabled(false);
	}

	// Set default state
	if (!SetState(MonsterStatus::IDLE))
	{
		ErrMsg("Failed to set initial state.");
		return false;
	}
	_alertDecayRate = _alertDecayRateIdle; // Needs to be set here as Idle->OnEnter does not run here

	_alertLevel = _baseAlertLevel;
	_moveSpeed = _baseMoveSpeed;

	// Start monster ambient sound
	_soundBehaviours[HUNTING_AMBIENT]->SetVolume(_soundVolumes[HUNTING_AMBIENT]);
	_soundBehaviours[HUNTING_AMBIENT]->SetEnabled(true);

	QueueUpdate();
	QueueFixedUpdate();

	return true;
}

bool MonsterBehaviour::Update(TimeUtils &time, const Input& input)
{
	if (!_playerEntity)
	{
		_playerEntity = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity");

		// Get relevant layer behaviours
		if (_playerEntity)
		{
			if (!_playerEntity->GetBehaviourByType<PlayerMovementBehaviour>(_playerMover))
			{
				ErrMsg("Player entity missing 'PlayerMovementBehaviour'");
				return false;
			}

			Entity *flashlight = GetScene()->GetSceneHolder()->GetEntityByName("Flashlight Holder");
			if (!flashlight)
			{
				ErrMsg("Could not find flashlight");
				return false;
			}

			if (!flashlight->GetBehaviourByType<FlashlightBehaviour>(_playerFlashlight))
			{
				ErrMsg("Player entity missing 'FlashlightBehaviour'");
				return false;
			}
		}
		else
		{
			ErrMsg("Still havent found player");
			return true;
		}
	}

	AnyStateUpdate(time);

	switch (_state)
	{
	case MonsterStatus::IDLE:
	case MonsterStatus::HUNT:
	case MonsterStatus::HUNT_DONE:
	case MonsterStatus::CAPTURE:
		_monsterStates[_state]->InitialOnUpdate(time);
		break;
	case MonsterStatus::MONSTER_STATE_COUNT:
	default:
		break;
	}

	if (_drawRadius)
	{
		DebugDrawer &debugDraw = DebugDrawer::Instance();
		dx::XMFLOAT3A pos = GetTransform()->GetPosition();
		debugDraw.DrawLine({ pos, _sightRange * 2, { 1.0f, 0.0f, 0.0f, 1 } }, { { pos.x, pos.y + .2f, pos.z }, _sightRange * 2, { 1.0f, 0.0f, 0.0f, 1 } });
		debugDraw.DrawLine({ pos, _hearingRange * 2, { 0.0f, 1.0f, 0.0f, 1 } }, { { pos.x, pos.y + .1f, pos.z }, _hearingRange * 2, { 0.0f, 1.0f, 0.0f, 1 } });
	}

	// Clamp alert level
	_alertLevel = std::clamp<float>(_alertLevel, _baseAlertLevel, 100);


	switch (_currentSound)
	{
	case HUNTING1:
	case HUNTING2:
	case HUNTING3:
	case HUNTING_AMBIENT:
	case HEART_SLOW:
	case HEART_MEDIUM:
	case HEART_FAST:
		_soundBehaviours[_currentSound]->Play();
		break;
	case SOUND_COUNT:
	case NULL_SOUND:
	default:
		break;
	}

	return true;
}

bool MonsterBehaviour::FixedUpdate(float deltaTime, const Input& input)
{
	return true;
}

#ifdef USE_IMGUI
bool MonsterBehaviour::RenderUI()
{
	ImGui::Text(std::format("Monster State: {}", ToString(_state)).c_str());

	std::string nodeCount = "Nodes left in path: " + std::to_string(_path.size());
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), nodeCount.c_str());

	if (!_path.empty())
	{
		std::string nextPoint = "Next Point: " + std::to_string(_path[0].x) + ", " + std::to_string(_path[0].y) + ", " + std::to_string(_path[0].z);
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), nextPoint.c_str());
	}

	XMFLOAT3A pos = GetTransform()->GetPosition();
	std::string position = "Monster Pos: " + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), position.c_str());

	if (_playerInSight) ImGui::Text("Player in sight: true");
	else ImGui::Text("Player in sight: false");

	ImGui::InputFloat("Sight Range", &_sightRange, 0.1f, 1.0f);
	_sightRange = round(_sightRange * 10) / 10;
	if (_sightRange < 0.1f)
		_sightRange = 0.1f;

	ImGui::InputFloat("Hearing Range", &_hearingRange, 0.1f, 1.0f);
	_hearingRange = round(_hearingRange * 10) / 10;
	if (_hearingRange < 0.1f)
		_hearingRange = 0.1f;

	if (_nextNodeIsCave)
		ImGui::Text("Next: Cave");
	else
		ImGui::Text("Next: Mine");


	if (_priorNodeWasCave)
		ImGui::Text("Prior: Cave");
	else
		ImGui::Text("Prior: Mine");

	ImGui::Text("Movement Factor: %f", _movementFactor);


	// Set player state (TESTING)
	if (ImGui::Button("Set playerState: Still"))
	{
		_playerMover->SetStill(true);
		_playerMover->SetSneaking(false);
		_playerMover->SetWalking(false);
		_playerMover->SetRunning(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Set playerState: Sneaking"))
	{
		_playerMover->SetStill(false);
		_playerMover->SetSneaking(true);
		_playerMover->SetWalking(false);
		_playerMover->SetRunning(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Set playerState: Walking"))
	{
		_playerMover->SetStill(false);
		_playerMover->SetSneaking(false);
		_playerMover->SetWalking(true);
		_playerMover->SetRunning(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Set playerState: Running"))
	{
		_playerMover->SetStill(false);
		_playerMover->SetSneaking(false);
		_playerMover->SetWalking(false);
		_playerMover->SetRunning(true);
	}
	if (_playerMover->IsStill())
		ImGui::Text("Player state: Still");
	else if (_playerMover->IsSneaking())
		ImGui::Text("Player state: Sneaking");
	else if (_playerMover->IsWalking())
		ImGui::Text("Player state: Walking");
	else if (_playerMover->IsRunning())
		ImGui::Text("Player state: Running");


	ImGui::Checkbox("Draw Alert Radius", &_drawRadius);

	ImGui::DragFloat("Move Speed", &_moveSpeed, 0.01f);

	ImGui::Text("Alert Level: %f", _alertLevel);

	ImGui::Text("Distortion Strength: %f", _distortionStrength);
	ImGui::DragFloat("Distortion Amplifier", &_distortionAmplifier, 0.001f);
	ImGuiUtils::LockMouseOnActive();

	if (ImGui::Button("Reset Monster"))
		ResetMonster();
	ImGui::SameLine();
	if (ImGui::Button("Start Hunt"))
		_alertLevel = 100;
	ImGui::SameLine();
	if (ImGui::Button("Reset Alert Level"))
		_alertLevel = 0;

	return true;
}
#endif

MonsterBehaviour::~MonsterBehaviour()
{
	for (int i = 0; i < MonsterStatus::MONSTER_STATE_COUNT; i++)
		delete _monsterStates[i];
}

// Returns true if alert was sent, false if origin was given and it was too far away or required LOS and it was obstructed
bool MonsterBehaviour::Alert(const float amount, std::optional<dx::XMFLOAT3A> alertOrigin, 
	std::optional<const float> distance, std::optional<bool> scaleWithDist, std::optional<bool> requireLOS)
{
	float scale = 1.0f;

	if (alertOrigin)
	{
		Transform *t = GetTransform();
		dx::XMFLOAT3A pos = t->GetPosition(World);
		dx::XMFLOAT3A origin = *alertOrigin;

		float rad = distance ? *distance : _hearingRange;
		float distSqr = 
			((origin.x - pos.x) * (origin.x - pos.x)) + 
			((origin.z - pos.z) * (origin.z - pos.z));

		if (distSqr > rad*rad)
			return false;

		float dist = sqrtf(distSqr);

		// Check if line of sight is required (should probably use HasLOS function)
		if (requireLOS)
		{
			if (*requireLOS)
			{
				XMFLOAT3 dir;
				Store(dir, XMVector3Normalize(Load(pos) - Load(origin)));

				Collisions::Ray rayCollider = Collisions::Ray(origin, dir, dist);
				XMFLOAT3 p, n;
				float depth;

				bool hit = Collisions::TerrainRayIntersection(
					*GetScene()->GetTerrain(), rayCollider,
					n, p, depth
				);

				if (hit)
					return false;
			}
		}

		if (scaleWithDist)
			scale = 1.0f - (dist / rad);
	}

	_alertLevel += amount * scale;

	return true;
}

bool MonsterBehaviour::soundAlert(const float amount, std::optional<dx::XMFLOAT3A> soundOrigin, 
	std::optional<const float> reach, std::optional<bool> scaleWithDist)
{
	// Should probably be more intricate than this (path in general direction of sound)

	float dist;
	XMStoreFloat(&dist, XMVector3LengthEst(Load(soundOrigin.value()) - Load(GetTransform()->GetPosition())));

	if (dist <= _hearingRange && dist <= reach)
	{
		_path.clear();
		_graphManager->GetPath(GetTransform()->GetPosition(), To3(soundOrigin.value()), &_path);

		float scale = 1.0f;
		if (scaleWithDist.value())
		{
			scale = std::clamp(dist / _hearingRange, 0.0f, 1.0f);
		}

		_alertLevel += amount * scale;
	}

	return false;
}

bool MonsterBehaviour::GetPath(XMFLOAT3 &dest)
{
	if (_path.empty())
	{
		return false;
	}
	else
	{
		dest = _path.back();
		return true;
	}
}
bool MonsterBehaviour::GetPath(std::vector<XMFLOAT3> *&path)
{
	if (_path.empty())
	{
		return false;
	}
	else
	{
		path = &_path;
		return true;
	}
}


bool MonsterBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Alert", _alertLevel, docAlloc);

	return true;
}
bool MonsterBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_alertLevel = obj["Alert"].GetFloat();

	return true;
}
