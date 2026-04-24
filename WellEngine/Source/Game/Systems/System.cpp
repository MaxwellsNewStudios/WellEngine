#include "stdafx.h"
#include "System.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


bool System::Initialize() { return true; }

bool System::InitialInitialize()
{
	return Initialize();
}


void System::Shutdown() { }

void System::InitialShutdown()
{
	Shutdown();
}


bool System::Update() { return true; }

bool System::InitialUpdate()
{
	return Update();
}


#ifdef USE_IMGUI
bool System::RenderUI() { return true; }

bool System::InitialRenderUI()
{
	return RenderUI();
}
#endif // USE_IMGUI
