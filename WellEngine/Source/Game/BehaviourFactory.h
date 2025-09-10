#pragma once
#include <string>
#include "BehaviourRegistry.h"

class Behaviour;

namespace BehaviourFactory
{
	[[nodiscard]] Behaviour *CreateBehaviour(const std::string &name);
}
