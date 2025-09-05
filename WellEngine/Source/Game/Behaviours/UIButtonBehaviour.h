#pragma once

#include "Behaviour.h"

// Abstract class intended as a base class for UI buttons (ex: play, exit, options...)
 class UIButtonBehaviour : public Behaviour
{
public:
	UIButtonBehaviour() = default;
	virtual ~UIButtonBehaviour() = default;
};
