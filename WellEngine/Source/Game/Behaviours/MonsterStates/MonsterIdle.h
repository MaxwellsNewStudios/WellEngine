#pragma once

#include "MonsterState.h"

class MonsterIdle : public MonsterState
{
public:
	MonsterIdle() = default;
	~MonsterIdle() = default;

protected:
	bool OnEnter() override;
	bool OnUpdate(TimeUtils &time) override;
	bool OnExit() override;
};
