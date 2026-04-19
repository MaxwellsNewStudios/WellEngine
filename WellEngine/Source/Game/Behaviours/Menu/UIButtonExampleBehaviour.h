#pragma once

#include "ButtonBehaviours.h"

namespace WellEngine
{
	class UIButtonExampleBehaviour : public UIButtonBehaviour
	{
	protected:
		[[nodiscard]] bool Start() override;
		[[nodiscard]] bool OnSelect() override;
		[[nodiscard]] bool OnHover() override;

	public:
		UIButtonExampleBehaviour() = default;
		~UIButtonExampleBehaviour() = default;
	};
}
