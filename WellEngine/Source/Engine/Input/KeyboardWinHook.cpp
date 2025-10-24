#include "stdafx.h"
#include "KeyboardWinHook.h"
#include <windows.h>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KeyboardWinHook &keyboardHook = KeyboardWinHook::Instance();
	bool *keys = keyboardHook.GetKeys();
	HHOOK &hook = keyboardHook.GetHook();

    if (nCode == HC_ACTION) 
    {
        KBDLLHOOKSTRUCT *pKeyboard = (KBDLLHOOKSTRUCT *)lParam;
        keys[pKeyboard->vkCode] = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    }

    // Pass the event to the next hook in the chain
    return CallNextHookEx(hook, nCode, wParam, lParam);
}

KeyboardWinHook::KeyboardWinHook()
{
	// Install the low-level keyboard hook
    _hHook = SetWindowsHookEx(WH_KEYBOARD_LL,
		(HOOKPROC)LowLevelKeyboardProc,
		GetModuleHandle(NULL),
		0
    );

    if (!_hHook)
    {
		ErrMsg("Failed to install keyboard hook!");
        return;
    }
}

KeyboardWinHook::~KeyboardWinHook()
{
    UnhookWindowsHookEx(_hHook);
}

void KeyboardWinHook::Update()
{
    // Process all pending messages to ensure the hook works
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
