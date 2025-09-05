#include "stdafx.h"
#include "MonsterCapture.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool MonsterCapture::OnEnter()
{
	_mb->GetEntity()->Disable();

	if (_mb->_playerMover->Catch())
		_mb->SetState(IDLE);
	else
		_mb->GetEntity()->Enable();

    return true;
}

bool MonsterCapture::OnUpdate(TimeUtils &time)
{
    return true;
}

bool MonsterCapture::OnExit()
{
	_mb->ResetMonster();

	return true;
}
