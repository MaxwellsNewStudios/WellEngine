#pragma once

#include "Game/Game.h"

namespace WellEngine
{
	class EngineCore
	{
	private:
		Game _game{};
		size_t _frameCount = 0;

	public:
		EngineCore();
		~EngineCore();

		int Init();
		int Run();

		TESTABLE
	};
}
