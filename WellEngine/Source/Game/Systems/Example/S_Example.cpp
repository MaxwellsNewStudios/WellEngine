#include "stdafx.h"
#include "S_Example.h"

bool S_Example::Initialize()
{
	return false;
}

void S_Example::Shutdown()
{
}

bool S_Example::Update()
{
	return false;
}

#ifdef USE_IMGUI
bool S_Example::RenderUI()
{
	return false;
}
#endif // USE_IMGUI
