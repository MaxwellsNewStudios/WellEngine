#pragma once

#include "MonsterState.h"

class MonsterExtendedHunt : public MonsterState
{
public:
	MonsterExtendedHunt() = default;
	~MonsterExtendedHunt() = default;

protected:
	bool OnEnter() override;
	bool OnUpdate(TimeUtils &time) override;
	bool OnExit() override;
};
