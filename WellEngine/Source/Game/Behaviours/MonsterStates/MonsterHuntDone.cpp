#include "stdafx.h"
#include "MonsterHuntDone.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool MonsterHuntDone::OnEnter()
{	
	_mb->_alertDecayRate = _mb->_alertDecayRateHuntDone;
	_mb->_path.clear();

	if (_mb->_playerMover->IsHiding())
	{
		MonsterSound sound = MonsterSound::HEART_SLOW;
		_mb->_soundBehaviours[sound]->SetVolume(_mb->_soundVolumes[sound]);
		_mb->_soundBehaviours[sound]->SetEnabled(true);
		_mb->_soundBehaviours[sound]->Play();
	}

	return true;
}

bool MonsterHuntDone::OnUpdate(TimeUtils &time)
{
	if (_mb->_alertLevel < 25)
		if (!_mb->SetState(MonsterStatus::IDLE))
		{
			ErrMsg("Failed to update monster state.");
			return false;
		}
	
	if (!_mb->_playerMover->IsHiding())
	{
		// Hunt trigger: Alert level
		if (_mb->_alertLevel > _mb->_huntTriggerAlertLevel)
		{
			if (!_mb->SetState(MonsterStatus::HUNT))
			{
				ErrMsg("Failed to update monster state.");
				return false;
			}
		}

		// Hunt trigger: Can see player
		if (_mb->_playerInSight)
		{
			if (!_mb->SetState(MonsterStatus::HUNT))
			{
				ErrMsg("Failed to update monster state.");
				return false;
			}
		}
	}

	// Alert level generation: Based on distance and player noise level
	float distanceFactor = (_mb->_hearingRange - _mb->_playerDistance) / _mb->_hearingRange;
	distanceFactor = std::clamp<float>(distanceFactor, 0, 1);

	_mb->_alertLevel += distanceFactor * _mb->_playerNoiseLevel * time.GetDeltaTime();

	// Hiding with flashlight on
	if (_mb->_playerMover->IsHiding() && _mb->_playerFlashlight->IsOn())
		_mb->_alertLevel += 25.0f * time.GetDeltaTime();

	// Decrement sound
	_mb->_currentSoundVolume -=  2 * _mb->_soundVolumes[_mb->_currentSound] * _mb->_soundVolumeIncreaseRate * time.GetDeltaTime();
	_mb->_currentSoundVolume = std::max<float>(_mb->_currentSoundVolume, 0);

	if (_mb->_currentSound != NULL_SOUND)
	{
		_mb->_soundBehaviours[_mb->_currentSound]->SetVolume(_mb->_currentSoundVolume);

		_mb->_soundBehaviours[_mb->_currentSound]->SetEmitterPosition(_mb->_playerEntity->GetTransform()->GetPosition(World));
		_mb->_soundBehaviours[_mb->_currentSound]->SetListenerPosition(_mb->_playerEntity->GetTransform()->GetPosition(World));
	}

	return true;
}

bool MonsterHuntDone::OnExit()
{
	_mb->_soundBehaviours[_mb->_currentSound]->Pause();
	_mb->_soundBehaviours[_mb->_currentSound]->SetEnabled(false);

	MonsterSound sound = MonsterSound::HEART_SLOW;
	_mb->_soundBehaviours[sound]->Pause();
	_mb->_soundBehaviours[sound]->SetEnabled(false);

	_mb->_currentSound = MonsterSound::NULL_SOUND;

	return true;
}
