#pragma once
#include <string>

class Behaviour;

namespace BehaviourRegistry
{
	[[nodiscard]] const std::map<std::string_view, std::function<Behaviour *(void)>> &Get();
#ifdef DEBUG_BUILD
	[[nodiscard]] const std::map<std::string_view, std::string> &GetCategories();
#endif
}