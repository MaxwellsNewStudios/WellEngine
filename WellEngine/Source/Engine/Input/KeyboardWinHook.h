#pragma once

#include <windows.h>

namespace WellEngine
{
	class KeyboardWinHook
	{
		HHOOK _hHook = NULL;
		bool _keys[256] = { };

	public:
		KeyboardWinHook();
		~KeyboardWinHook();

		static KeyboardWinHook &Instance()
		{
			static KeyboardWinHook instance;
			return instance;
		}

		void Update();

		HHOOK &GetHook() noexcept
		{
			return _hHook;
		}

		bool *GetKeys() noexcept
		{
			return _keys;
		}

		bool GetKeyState(int key) const noexcept
		{
			return _keys[key];
		}
	};
}
