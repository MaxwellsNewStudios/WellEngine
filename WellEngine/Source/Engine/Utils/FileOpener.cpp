#include "stdafx.h"
#include "FileOpener.h"

bool WellEngine::OpenFile(const std::string &path)
{
	std::string filePath = path;

	// Replace all / with \ for Windows
	std::replace(filePath.begin(), filePath.end(), '/', '\\');

	DbgMsgF("Opening '{}'", filePath);

	// Open script with default program
	SHELLEXECUTEINFOA sei = { 0 };
	sei.cbSize = sizeof(SHELLEXECUTEINFOA);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_WAITFORINPUTIDLE;
	sei.hwnd = nullptr;
	sei.lpVerb = "open";
	sei.lpFile = filePath.c_str();
	sei.lpParameters = nullptr;
	sei.lpDirectory = nullptr;
	sei.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteExA(&sei))
	{
		DbgMsgF("Failed to open file '{}' with error code {}", filePath, GetLastError());
		return false;
	}
	else
	{
		if (!sei.hProcess)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			ShellExecuteExA(&sei);
		}

		struct WINDOWPROCESSINFO {
			DWORD pid;
			HWND hwnd;
		};

		// Get the window handle of the opened process and set it to foreground
		WINDOWPROCESSINFO info{};
		info.pid = GetProcessId(sei.hProcess);
		info.hwnd = 0;

		AllowSetForegroundWindow(info.pid);

		// Sleep for a short time to allow the process to open the file and create a window
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		EnumWindows(
			[](HWND hwnd, LPARAM lParam) -> BOOL {
				WINDOWPROCESSINFO *infoPtr = (WINDOWPROCESSINFO *)lParam;
				DWORD check = 0;
				BOOL br = TRUE;
				GetWindowThreadProcessId(hwnd, &check);

				if (check == infoPtr->pid)
				{
					infoPtr->hwnd = hwnd;
					br = FALSE;
				}

				return br;
			},
			(LPARAM)&info
		);

		if (info.hwnd != 0)
		{
			SetForegroundWindow(info.hwnd);
			SetActiveWindow(info.hwnd);
		}

		CloseHandle(sei.hProcess);
	}

	return true;
}