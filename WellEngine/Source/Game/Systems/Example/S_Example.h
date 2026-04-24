#pragma once

#include "../System.h"

namespace WellEngine
{
	class [[register_system]] S_Example : public System
	{
	public:
		std::string_view GetName() const override { return "Example"; }

	protected:
		[[nodiscard]] bool Initialize() override;
		void Shutdown() override;

		[[nodiscard]] bool Update() override;

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif // USE_IMGUI

	public:
		S_Example(Game *game) : System(game) {}
	};
}
