#pragma once

#include <string>

#include "BehaviourRegistry.h"

namespace WellEngine
{
	class Behaviour;

	namespace BehaviourFactory
	{
		[[nodiscard]] Behaviour *CreateBehaviour(const std::string &name);
	}
}
