#pragma region Includes, Usings & Defines
#include "stdafx.h"
#include "BehaviourFactory.h"
#include "Behaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif
#pragma endregion

Behaviour *BehaviourFactory::CreateBehaviour(std::string_view name)
{
	auto &map = BehaviourRegistry::Get();

	if (!map.contains(name))
	{
#ifdef DEBUG_BUILD
		WarnF("Unrecognized behaviour name '{}'! Is the behaviour definition missing a [[register_behaviour]] attribute?", name);
#endif

		return nullptr;
	}

	return map.at(name)();
}
