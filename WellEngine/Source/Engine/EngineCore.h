#pragma once

#include "Game.h"

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

	TESTABLE()
};