#pragma once
#include <string>
#include "BehaviourRegistry.h"

class Behaviour;

namespace BehaviourFactory
{
	[[nodiscard]] Behaviour *CreateBehaviour(std::string_view name);
}
