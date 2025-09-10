#include "stdafx.h"
#include "MonsterIdle.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool MonsterIdle::OnEnter()
{
	_mb->_alertDecayRate = _mb->_alertDecayRateIdle;
	_mb->UpdatePathIdle();

    return true;
}

bool MonsterIdle::OnUpdate(TimeUtils &time)
{
	if (time.GetDeltaTime() > 1.0f) // HACK: Done to avoid running on the first frame, only works because first frame is slow
		return true;

	// Pathing
	if (_mb->_path.size() == 0) _mb->UpdatePathIdle();
	_mb->PathFind(time);

	// Alert level and hunt triggering
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

		// Alert level generation: Based on distance and player noise level
		float distanceFactor = (_mb->_hearingRange - _mb->_playerDistance) / _mb->_hearingRange;
		distanceFactor = std::clamp<float>(distanceFactor, 0, 1);

		_mb->_alertLevel += distanceFactor * _mb->_playerNoiseLevel * time.GetDeltaTime();
	}

	return true;
}

bool MonsterIdle::OnExit()
{
    return true;
}
