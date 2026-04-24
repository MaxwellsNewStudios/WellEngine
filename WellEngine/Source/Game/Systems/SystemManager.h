#pragma once

#include <vector>

#include "System.h"

// TODO: Hook into Game.h

namespace WellEngine
{
	// SystemManager handles the lifetime and execution of systems.
	class SystemManager
	{
	private:
		std::vector<std::unique_ptr<System>> _systems;

	public:
		SystemManager() = default;
		~SystemManager() = default;

		[[nodiscard]] bool Initialize(Game *game);
		void Shutdown();

		[[nodiscard]] bool Update();

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI();
#endif // USE_IMGUI
	};
}
