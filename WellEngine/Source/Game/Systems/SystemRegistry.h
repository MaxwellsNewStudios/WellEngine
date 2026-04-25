#pragma once

#include <vector>

namespace WellEngine
{
	class System;
	class Game;
}

// SystemRegistry is resposible for the knowledge of all systems in the game.
// NOTE: The source file is generated automatically during the build.
namespace WellEngine::SystemRegistry
{
	std::vector<System *> GetSystems(Game *game);
}
