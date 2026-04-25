#include "stdafx.h"
#include "SystemManager.h"
#include "SystemRegistry.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


System *SystemManager::GetSystemByName(const std::string &name) const noexcept
{
	ZoneScopedXC(RandomUniqueColor());

	for (const std::unique_ptr<System> &system : _systems)
	{
		if (system->GetName() == name)
			return system.get();
	}
	return nullptr;
}

bool SystemManager::HasSystem(const std::string &name) const noexcept
{
	return GetSystemByName(name) != nullptr;
}

const std::vector<std::unique_ptr<System>> &SystemManager::GetSystems() const noexcept
{
	return _systems;
}


bool SystemManager::Initialize(Game *game)
{
	ZoneScopedC(RandomUniqueColor());

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
	ZoneScopedC(RandomUniqueColor());

	for (std::unique_ptr<System> &system : _systems)
	{
		system->InitialShutdown();
	}

	_systems.clear();
}

bool SystemManager::Update(TimeUtils &time, const Input &input)
{
	ZoneScopedC(RandomUniqueColor());

	for (std::unique_ptr<System> &system : _systems)
	{
		if (!system->InitialUpdate(time, input))
		{
			ErrMsgF("Failed to update system: {}", system->GetName().data());
			return false;
		}
	}

	return true;
}

bool SystemManager::OnSceneChange(Scene *prev, Scene *next)
{
	for (std::unique_ptr<System> &system : _systems)
	{
		if (!system->InitialOnSceneChange(prev, next))
		{
			ErrMsgF("Failed to register scene change in system: {}", system->GetName().data());
			return false;
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool SystemManager::RenderUI()
{
	ZoneScopedC(RandomUniqueColor());

	for (std::unique_ptr<System> &system : _systems)
	{
		std::string header = std::format("{}##{}", system->GetName(), (size_t)(system.get()));
		if (ImGui::CollapsingHeader(header.c_str()))
		{
			ImGui::BeginChild("SystemChild", ImVec2(0,0), true | ImGuiChildFlags_ResizeY);
			if (!system->InitialRenderUI())
			{
				ErrMsgF("Failed to render UI for system: {}", system->GetName());
				return false;
			}
			ImGui::EndChild();
		}
	}

	return true;
}
#endif // USE_IMGUI
