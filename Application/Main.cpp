#include "stdafx.h"
#include "AppMain.h"

#ifdef _DEPLOY
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
	return Engine::AppMain();
}
