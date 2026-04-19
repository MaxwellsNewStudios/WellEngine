#pragma once

#include "MonsterState.h"

namespace WellEngine
{
	class MonsterHunt : public MonsterState
	{
	public:
		MonsterHunt() = default;
		~MonsterHunt() = default;

	protected:
		bool OnEnter() override;
		bool OnUpdate(TimeUtils &time) override;
		bool OnExit() override;
	};
}
