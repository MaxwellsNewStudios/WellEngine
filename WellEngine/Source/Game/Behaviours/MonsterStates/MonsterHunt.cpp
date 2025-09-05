#include "stdafx.h"
#include "MonsterHunt.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool MonsterHunt::OnEnter()
{
	_mb->_alertDecayRate = _mb->_alertDecayRateHunt;
	_mb->_alertLevel = 100.0f;

	_mb->_moveSpeed = _mb->_baseHuntSpeed;
	_mb->_distortionAmplifier *= 1.5f;

	_mb->_currentSound = MonsterSound((UINT)MonsterSound::HUNTING1 + rand() % 3);

	switch (_mb->_currentSound)
	{
	case HUNTING1:
	case HUNTING2:
	case HUNTING3:
		_mb->_currentSoundVolume = 1.0f;
		_mb->_soundBehaviours[_mb->_currentSound]->SetVolume(_mb->_currentSoundVolume);
		_mb->_soundBehaviours[_mb->_currentSound]->SetEnabled(true);
		_mb->_soundBehaviours[_mb->_currentSound]->Play();
		break;
	case HUNTING_AMBIENT:
	case SOUND_COUNT:
	default:
		_mb->_currentSound = HUNTING1;
		ErrMsg("Incorrect Sound State");
		return false;
	}

	return true;
}

bool MonsterHunt::OnUpdate(TimeUtils &time)
{
	// Pathing: Based on sight or player noise (updates twice per second)
	static float updateTimer = 0.0f;
	updateTimer += time.GetDeltaTime();
	if (_mb->_playerInSight)
	{
		// Monster can see player
		if (updateTimer >= 0.5f)
		{
			_mb->UpdatePathToPlayer();
			updateTimer = 0.0f;
		}
	}
	else
	{
		// Monster can not see player -> Check if it can hear player
		if (updateTimer >= 0.05f)
		{
			if (_mb->_playerDistance <= _mb->_playerNoiseRange)
			{
				_mb->UpdatePathToPlayer();
			}
			updateTimer = 0.0f;
		}
	}
	_mb->PathFind(time);


	// Hunt end trigger: Alert level or hiding
	if (_mb->_alertLevel < 50 || (_mb->_playerMover->IsHiding() && !_mb->_playerFlashlight->IsOn()))
	{
		if (!_mb->SetState(MonsterStatus::HUNT_DONE))
		{
			ErrMsg("Failed to update monster state.");
			return false;
		}
	}

	// Animation
	_mb->GetTransform()->RotateYaw(5.0f * time.GetDeltaTime());

	// Sound
	_mb->_currentSoundVolume += _mb->_soundVolumeIncreaseRate * time.GetDeltaTime();
	_mb->_currentSoundVolume = std::min<float>(_mb->_currentSoundVolume, 1);

	_mb->_soundBehaviours[_mb->_currentSound]->SetVolume(_mb->_soundVolumes[_mb->_currentSound] * _mb->_currentSoundVolume);
	_mb->_soundBehaviours[_mb->_currentSound]->SetEmitterPosition(_mb->GetTransform()->GetPosition(World));
	_mb->_soundBehaviours[_mb->_currentSound]->SetListenerPosition(_mb->_playerEntity->GetTransform()->GetPosition(World));

	return true;
}

bool MonsterHunt::OnExit()
{
	_mb->_moveSpeed = _mb->_baseMoveSpeed;
	_mb->_distortionAmplifier /= 1.5f;

    return true;
}
