// Automatically generated during build by SystemRegistration.
// Scans for all system definitions and includes them here for the System Manager to use.
// NOTE: DO NOT MODIFY MANUALLY!

#include "stdafx.h"
#include "SystemRegistry.h"
#include "Game/Game.h"
#include "System.h"
#include "Game/Systems/Example/S_Example.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

std::vector<System *> WellEngine::SystemRegistry::GetSystems(Game *game)
{
	std::vector<System *> systemList = {
		new S_Example(game),
	};

	return systemList;
};
