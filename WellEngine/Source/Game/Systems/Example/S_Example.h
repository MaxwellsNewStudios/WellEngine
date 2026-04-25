#pragma once

#include "../System.h"

namespace WellEngine
{
	class [[register_system]] S_Example : public System
	{
	public:
		S_Example(Game *game) : System(game) {}
		std::string_view GetName() const override { return "Example"; }

	protected:
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif // USE_IMGUI
	};
}
