#pragma once

#include "Engine/Timing/TimeUtils.h"
#include "../MonsterBehaviour.h"

namespace WellEngine
{
	class MonsterState
	{
	public:
		MonsterState() = default;
		virtual ~MonsterState() = default;

		void Intitalize(MonsterBehaviour *monsterBehaviour);

		bool InitialOnEnter();
		bool InitialOnUpdate(TimeUtils &time);
		bool InitialOnExit();

	protected:
		virtual bool OnEnter();
		virtual bool OnUpdate(TimeUtils &time);
		virtual bool OnExit();

		MonsterBehaviour *_mb = nullptr;
	};
}
