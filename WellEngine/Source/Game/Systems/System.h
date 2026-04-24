/*
	NOTE:
		All non-abstract systems deriving from System should have the [[register_system]] attribute.
		This exposes the system to the game loop. Leaving out the attribute means the system will not be
		initialized, executed and destructed automatically.
*/

#pragma once

#include <string_view>

#include "Engine/Utils/ReferenceHelper.h"

namespace WellEngine
{
	class Game;

	// Systems are global, inter-scene scripts that hook into the main game loop automatically.
	// They can be used for singleton-like functionality that should persist across scenes.
	class System : public IRefTarget<System>
	{
	public:
		virtual std::string_view GetName() const = 0;

	private:
		Game *_game = nullptr;

	protected:
		Game *GetGame() const noexcept { return _game; }

		[[nodiscard]] virtual bool Initialize();
		virtual void Shutdown();

		[[nodiscard]] virtual bool Update();

#ifdef USE_IMGUI
		[[nodiscard]] virtual bool RenderUI();
#endif // USE_IMGUI

	public:
		System(Game *game) : _game(game) {}
		~System() = default;

		[[nodiscard]] bool InitialInitialize();
		void InitialShutdown();

		[[nodiscard]] virtual bool InitialUpdate();

#ifdef USE_IMGUI
		[[nodiscard]] bool InitialRenderUI();
#endif // USE_IMGUI

	};
}
