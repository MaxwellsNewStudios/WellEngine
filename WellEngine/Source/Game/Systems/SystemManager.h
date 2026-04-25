#pragma once

#include <vector>

#include "System.h"

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

		[[nodiscard]] bool HasSystem(const std::string &name) const noexcept;
		[[nodiscard]] System *GetSystemByName(const std::string &name) const noexcept;
		[[nodiscard]] const std::vector<std::unique_ptr<System>> &GetSystems() const noexcept;

		[[nodiscard]] bool Initialize(Game *game);
		void Shutdown();

		[[nodiscard]] bool Update(TimeUtils &time, const Input &input);

		[[nodiscard]] bool OnSceneChange(Scene *prev, Scene *next);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI();
#endif // USE_IMGUI
	};
}
