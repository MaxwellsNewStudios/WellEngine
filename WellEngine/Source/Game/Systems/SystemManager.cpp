#include "stdafx.h"
#include "SystemManager.h"
#include "SystemRegistry.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool SystemManager::Initialize(Game *game)
{
	std::vector<System *> systems = SystemRegistry::GetSystems(game);

	_systems.reserve(systems.size());
	for (System *system : systems)
	{
		std::unique_ptr<System> systemPtr(system);
		if (!systemPtr->InitialInitialize())
		{
			ErrMsgF("Failed to initialize system: {}", system->GetName().data());
			return false;
		}

		_systems.push_back(std::move(systemPtr));
	}

	return true;
}

void SystemManager::Shutdown()
{
	for (std::unique_ptr<System> &system : _systems)
	{
		system->InitialShutdown();
	}

	_systems.clear();
}

bool SystemManager::Update()
{
	for (std::unique_ptr<System> &system : _systems)
	{
		if (!system->InitialUpdate())
		{
			ErrMsgF("Failed to update system: {}", system->GetName().data());
			return false;
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool SystemManager::RenderUI()
{
	for (std::unique_ptr<System> &system : _systems)
	{
		std::string header = std::format("{}##{}", system->GetName(), (size_t)(system.get()));
		if (ImGui::CollapsingHeader(header.c_str()))
		{
			if (!system->InitialRenderUI())
			{
				ErrMsgF("Failed to render UI for system: {}", system->GetName());
				return false;
			}
		}
	}

	return true;
}
#endif // USE_IMGUI