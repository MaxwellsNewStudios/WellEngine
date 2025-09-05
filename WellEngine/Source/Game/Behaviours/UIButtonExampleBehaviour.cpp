#include "stdafx.h"
#include "Behaviours/UIButtonExampleBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool UIButtonExampleBehaviour::Start()
{
	if (_name.empty())
		_name = "UIButtonExampleBehaviour"; // For categorization in ImGui.

	return true;
}

bool UIButtonExampleBehaviour::OnHover()
{
	// Trigger event here when hovered
	return true;
}

bool UIButtonExampleBehaviour::OnSelect()
{
	// Trigger event here when clicked
	Transform *t = GetEntity()->GetTransform();
	t->RotateYaw(dx::XM_PI/2);
	return true;
}
