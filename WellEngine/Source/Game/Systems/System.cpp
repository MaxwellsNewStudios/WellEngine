#include "stdafx.h"
#include "System.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool System::Initialize() { return true; }
bool System::InitialInitialize()
{
	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	return Initialize();
}

void System::Shutdown() { }
void System::InitialShutdown()
{
	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	Shutdown();
}

bool System::Update(TimeUtils &time, const Input &input) { return true; }
bool System::InitialUpdate(TimeUtils &time, const Input &input)
{
	if (!IsEnabled())
		return true;

	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	return Update(time, input);
}

bool System::OnSceneChange(Scene *prev, Scene *next) { return true; }
bool System::InitialOnSceneChange(Scene *prev, Scene *next)
{
	if (!IsEnabled())
		return true;

	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	return OnSceneChange(prev, next);
}

#ifdef USE_IMGUI
bool System::RenderUI() { return true; }
bool System::InitialRenderUI()
{
	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	ImGui::Checkbox("Active", &_enabled);

	ImGui::Separator();

	return RenderUI();
}
#endif // USE_IMGUI
