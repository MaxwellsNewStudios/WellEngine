#pragma once

#include "MonsterState.h"

namespace WellEngine
{
	class MonsterHuntDone : public MonsterState
	{
	public:
		MonsterHuntDone() = default;
		~MonsterHuntDone() = default;

	protected:
		bool OnEnter() override;
		bool OnUpdate(TimeUtils &time) override;
		bool OnExit() override;
	};
}
