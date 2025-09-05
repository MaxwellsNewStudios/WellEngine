#include "stdafx.h"
#include "Behaviours/MonsterHintBehaviour.h"
#include "Math/GameMath.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

static dx::XMFLOAT3A VecToPointXZ(const dx::XMFLOAT3A& from, const dx::XMFLOAT3A& to)
{
	// Returns a XMFLOAT3A representing the vector starting at 'from' and ending at 'to' projected onto the XZ plane.
	// The Y component of the returned vector is equal to that of 'from'.
	return { to.x - from.x, from.y, to.z - from.z };
}

static float Magnitude(const dx::XMFLOAT3A& vec)
{
	dx::XMVECTOR v = dx::XMLoadFloat3(&vec);
	return dx::XMVectorGetX(dx::XMVector3LengthEst(v));
}

static dx::XMFLOAT3A Normalize(const dx::XMFLOAT3A &vec)
{
	dx::XMVECTOR v = Load(vec);
	v = dx::XMVector3Normalize(v);
	dx::XMFLOAT3A result;
	Store(result, v);
	return result;
}

bool MonsterHintBehaviour::Start()
{
	if (_name.empty())
		_name = "MonsterHintBehaviour";

    std::array<std::string, 6> fileNames = { "monster_hint_02", "DistantThud", "Echo_Cymbal_01", "Echo_Cymbal_02", "Echo_Cymbal_03", "monster_hint_01" };
	for (const auto &fileName : fileNames)
	{
		Entity* emitterEntity = nullptr;
		if (!GetScene()->CreateEntity(&emitterEntity, "Hint Emitter Entity", { {0,0,0}, {0.1f,0.1f,0.1f}, {0,0,0,1} }, false))
		{
			ErrMsg("Failed to initialize player!");
			return false;
		}

		SoundBehaviour *sound = new SoundBehaviour(fileName, dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters, false, 400.0f, 0.0f);
		if (!sound->Initialize(emitterEntity))
		{
			ErrMsg("Failed to initialize sound!");
			return false;
		}
		sound->SetSerialization(false);
		sound->SetVolume(0.7f);
		sound->SetEnabled(false);

		_sounds.emplace_back(sound);
	}

	_timer = RandomFloat(_minDelay, _maxDelay);

	QueueUpdate();

    return true;
}

bool MonsterHintBehaviour::Update(TimeUtils &time, const Input& input)
{
	if (_timer > 0)
	{
		_timer -= time.GetDeltaTime();
	}
	else
	{
		// Only get the monster behaviour once
		if (!_monsterBehaviour)
		{
			_monsterBehaviour = GetScene()->GetMonster();
			if (!_monsterBehaviour)
			{
				ErrMsg("Failed to get monster behaviour!");
				return false;
			}
		}

		// Get a normalized vector between the player and monster
		dx::XMFLOAT3A playerPos = GetTransform()->GetPosition(World);
		dx::XMFLOAT3A monsterPos = _monsterBehaviour->GetTransform()->GetPosition(World);
		dx::XMFLOAT3A direction = VecToPointXZ(playerPos, monsterPos);

		// Get the distance between the player and monster
		float distance = Magnitude(direction);
		
		// Set the emitter position to 50 units away from the player, in the direction of the monster.
		if (distance > _minDistance)
		{
			direction = Normalize(direction);
			direction = dx::XMFLOAT3A{ direction.x * _minDistance, direction.y, direction.z * _minDistance };
			dx::XMFLOAT3A newEmitterPosition = dx::XMFLOAT3A{ playerPos.x + direction.x, playerPos.y + direction.y + 1.0f, playerPos.z + direction.z };

			int soundIndex = rand() % _sounds.size();

			// Play the sound
			_sounds[soundIndex]->GetTransform()->SetPosition(newEmitterPosition, World);
			_sounds[soundIndex]->SetEmitterPosition(newEmitterPosition);
			_sounds[soundIndex]->SetEnabled(true);
			_sounds[soundIndex]->Play();

			// Reset the timer
			_timer = RandomFloat(_minDelay, _maxDelay);
		}

	}

#ifdef USE_IMGUI
	for (auto sound : _sounds)
	{
		if (_drawEmitters)
		{
			dx::XMFLOAT3A pos = GetTransform()->GetPosition();
			pos.y += .5f;
			dx::XMFLOAT3 emitterPos = sound->GetEmitterPosition();
			DebugDrawer::Instance().DrawLine({ pos, .1f, { 1.0f, 0.0f, 0.0f, 1 } }, { emitterPos, .1f, { 0.0f, 0.0f, 1.0f, 1 } });
		}
	}
#endif
	return true;
}

#ifdef USE_IMGUI
bool MonsterHintBehaviour::RenderUI()
{
	ImGui::Checkbox("Draw emitter positions", &_drawEmitters);

	return true;
}
#endif
