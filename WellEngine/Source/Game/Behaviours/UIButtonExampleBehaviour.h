#pragma once

#include "Behaviours/ButtonBehaviours.h"

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