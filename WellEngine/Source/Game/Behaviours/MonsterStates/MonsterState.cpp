#include "stdafx.h"
#include "MonsterState.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

void MonsterState::Intitalize(MonsterBehaviour *monsterBehaviour)
{
    _mb = monsterBehaviour;
}

bool MonsterState::InitialOnEnter()
{
    return OnEnter();
}

bool MonsterState::InitialOnUpdate(TimeUtils &time)
{
    return OnUpdate(time);
}

bool MonsterState::InitialOnExit()
{
    return OnExit();
}

bool MonsterState::OnEnter()
{
    return true;
}

bool MonsterState::OnUpdate(TimeUtils &time)
{
    return true;
}

bool MonsterState::OnExit()
{
    return true;
}
