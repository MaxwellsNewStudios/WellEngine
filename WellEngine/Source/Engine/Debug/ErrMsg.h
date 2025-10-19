#pragma once

#include "Source/Engine/EngineSettings.h"
#ifndef DEBUG_MESSAGES
#include <iostream>
#endif // DEBUG_MESSAGES
#include <string>

#ifdef BREAK_ON_WARN
#define __do_break_on_warn true
#else
#define __do_break_on_warn false
#endif

#define CUSTOM_WARNING(msg, file, line)								\
	do {															\
		if (!__do_break_on_warn)									\
			break;													\
		char c_msg[1024];											\
		snprintf(c_msg, sizeof(c_msg),								\
			"%s\n\n"												\
			"Source: %s:%d\n\n"										\
			"Do you want to issue a debug break at the source?\n"	\
			"Press 'Cancel' to abort the application.",				\
			msg, file, line);										\
		HWND hwnd = MsgLogger::GetHWnd();							\
		int result = OpenWinMessageBox(hwnd, c_msg, "Warning",		\
			MB_YESNOCANCEL | MB_ICONWARNING);						\
		if (result == IDYES) {										\
			__debugbreak();											\
		} else if (result == IDCANCEL) {							\
			exit(1);												\
		}															\
	} while (0)

#define CUSTOM_ASSERT(expr, msg, file, line)							\
	do {																\
		if (expr)														\
			break;														\
		if (!__do_break_on_warn)										\
			break;														\
		char c_msg[1024];												\
		snprintf(c_msg, sizeof(c_msg),									\
			"Assertion failed: %s\n"									\
			"Problem: %s\n\n"											\
			"Source: %s:%d\n\n"											\
			"Do you want to issue a debug break at the source?\n"		\
			"Press 'Cancel' to abort the application.",					\
			#expr, msg, file, line);									\
		HWND hwnd = MsgLogger::GetHWnd();								\
		int result = OpenWinMessageBox(hwnd, c_msg, "Assertion Failed",	\
			MB_YESNOCANCEL | MB_ICONWARNING);							\
		if (result == IDYES) {											\
			__debugbreak();												\
		} else if (result == IDCANCEL) {								\
			exit(1);													\
		}																\
	} while (0)


#ifdef DEBUG_MESSAGES
#define LogIndentIncr() MsgLogger::IndentIncr()
#define LogIndentDecr() MsgLogger::IndentDecr()

// Used for fatal problems, aborts automatically.
#define ErrMsg(msg)										\
{ 														\
	MsgLogger::ErrorMessage(msg, __FILE__, __LINE__); 	\
	std::abort(); 										\
}
#define ErrMsgF(msg, ...)														\
{ 																				\
	MsgLogger::ErrorMessage(std::format(msg, __VA_ARGS__), __FILE__, __LINE__); \
	std::abort(); 																\
}

// Used for important information that does not require immediate action.
#define DbgMsg(msg) MsgLogger::DebugMessage(msg)
#define DbgStr(msg) MsgLogger::DebugMessage(msg, false)
#define DbgMsgF(msg, ...) MsgLogger::DebugMessage(std::format(msg, __VA_ARGS__))
#define DbgStrF(msg, ...) MsgLogger::DebugMessage(std::format(msg, __VA_ARGS__), false)

// Used for potentially fatal problems, lets the user choose how to respond.
#define Warn(msg)                                       \
{                                                       \
	MsgLogger::WarningMessage(msg, __FILE__, __LINE__); \
	CUSTOM_WARNING(msg, __FILE__, __LINE__);           \
}
#define WarnF(msg, ...)                                                             \
{                                                                                   \
	MsgLogger::WarningMessage(std::format(msg, __VA_ARGS__), __FILE__, __LINE__);   \
	CUSTOM_WARNING(std::format(msg, __VA_ARGS__).c_str(), __FILE__, __LINE__);		\
}

// Used to warn if a condition is not met, lets the user choose how to respond.
#define Assert(expr, msg) { CUSTOM_ASSERT(expr, msg, __FILE__, __LINE__); }
#define AssertF(expr, msg, ...) { CUSTOM_ASSERT(expr, std::format(msg, __VA_ARGS__).c_str(), __FILE__, __LINE__); }
#else
#define LogIndentIncr() { }
#define LogIndentDecr() { }

// (DISABLED) Used for fatal problems, aborts automatically.
#define ErrMsg(msg) { }
#define ErrMsgF(msg, ...) { }

// (DISABLED) Used for important information that does not require immediate action.
#define DbgMsg(msg) { }
#define DbgStr(msg) { }
#define DbgMsgF(msg, ...) { }
#define DbgStrF(msg, ...) { }

// (DISABLED) Used for potentially fatal problems, lets the user choose how to respond.
#define Warn(msg) { }
#define WarnF(msg, ...) { }

// (DISABLED) Used to warn if a condition is not met, lets the user choose how to respond.
#define Assert(expr, msg) { }
#define AssertF(expr, msg, ...) { }
#endif // DEBUG_MESSAGES

int OpenWinMessageBox(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

class MsgLogger
{
private:
	std::string _indentStr = "    ";
	int _indentLevel = 0;
	HWND _hWnd = nullptr;

	[[nodiscard]] inline std::string GetIndent() const
	{
		std::string indent;
		for (int i = 0; i < _indentLevel; i++)
			indent += _indentStr;
		return indent;
	}

public:
	MsgLogger() = default;
	~MsgLogger() = default;

	static MsgLogger &GetInstance()
	{
		static MsgLogger instance;
		return instance;
	}

	static inline void SetHWnd(HWND hWnd) { GetInstance()._hWnd = hWnd; }
	static inline HWND GetHWnd() { return GetInstance()._hWnd; }

	static inline void IndentIncr() { GetInstance()._indentLevel++; }
	static inline void IndentDecr() { if (GetInstance()._indentLevel > 0) GetInstance()._indentLevel--; }

	static void ErrorMessage(const char *msg, const std::string &filePath, int line);
	static void ErrorMessage(const std::string &msg, const std::string &filePath, int line);

	static void WarningMessage(const char *msg, const std::string &filePath, int line);
	static void WarningMessage(const std::string &msg, const std::string &filePath, int line);

	static void DebugMessage(const char *msg, bool br = true);
	static void DebugMessage(const std::string &msg, bool br = true);
};
