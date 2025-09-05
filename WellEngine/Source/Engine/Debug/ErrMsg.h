#pragma once

#include "EngineSettings.h"
#ifndef DEBUG_MESSAGES
#include <iostream>
#endif // DEBUG_MESSAGES
#include <string>
#include <format>
#include <windows.h>
#include <intrin.h>


#define CUSTOM_ASSERT(expr, file, line) \
    do { \
        if (!(expr)) { \
            if (IsDebuggerPresent()) { \
                __debugbreak(); \
            } else { \
                char msg[1024]; \
                snprintf(msg, sizeof(msg), \
                    "Assertion failed: %s\n\nFile: %s\nLine: %d\n\n" \
                    "Press Retry to debug, Ignore to continue, Abort to exit", \
                    #expr, file, line); \
                int result = MessageBoxA(NULL, msg, "Assertion Failed", \
                    MB_ABORTRETRYIGNORE | MB_ICONERROR); \
                if (result == IDRETRY) { \
                    __debugbreak(); \
                } else if (result == IDABORT) { \
                    exit(1); \
                } \
            } \
        } \
    } while (0)

#define CUSTOM_ASSERT_MSG(expr, file, line, msg) \
    do { \
        if (!(expr)) { \
            WarningMessage(msg, file, line); \
            if (IsDebuggerPresent()) { \
                __debugbreak(); \
            } else { \
                char c_msg[1024]; \
                snprintf(c_msg, sizeof(c_msg), \
                    "Assertion failed: %s\n\nFile: %s\nLine: %d\n\n" \
                    "Press Retry to debug, Ignore to continue, Abort to exit", \
                    #expr, file, line); \
                int result = MessageBoxA(NULL, c_msg, "Assertion Failed", \
                    MB_ABORTRETRYIGNORE | MB_ICONERROR); \
                if (result == IDRETRY) { \
                    __debugbreak(); \
                } else if (result == IDABORT) { \
                    exit(1); \
                } \
            } \
        } \
    } while (0)


#ifdef DEBUG_MESSAGES
#define LogIndentIncr() MsgLogger::IndentIncr()
#define LogIndentDecr() MsgLogger::IndentDecr()

// Used for fatal problems, aborts automatically.
#define ErrMsg(msg) { MsgLogger::ErrorMessage(msg, __FILE__, __LINE__); std::abort(); }
#define ErrMsgF(msg, ...) { MsgLogger::ErrorMessage(std::format(msg, __VA_ARGS__), __FILE__, __LINE__); std::abort(); }

// Used for important information that does not require immediate action.
#define DbgMsg(msg) MsgLogger::DebugMessage(msg)
#define DbgStr(msg) MsgLogger::DebugMessage(msg, false)
#define DbgMsgF(msg, ...) MsgLogger::DebugMessage(std::format(msg, __VA_ARGS__))
#define DbgStrF(msg, ...) MsgLogger::DebugMessage(std::format(msg, __VA_ARGS__), false)

// Used for potentially fatal problems, lets the user choose how to react.
#define Warn(msg) { MsgLogger::WarningMessage(msg, __FILE__, __LINE__); CUSTOM_ASSERT(false, __FILE__, __LINE__); }
#define WarnF(msg, ...) { MsgLogger::WarningMessage(std::format(msg, __VA_ARGS__), __FILE__, __LINE__); CUSTOM_ASSERT(false, __FILE__, __LINE__); }

// Used to warn if a condition is not met, lets the user choose how to react.
#define Assert(expr, msg) { CUSTOM_ASSERT(expr, __FILE__, __LINE__); }
#define AssertF(expr, msg, ...) { CUSTOM_ASSERT_MSG(expr, __FILE__, __LINE__, std::format(msg, __VA_ARGS__)); }
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

// (DISABLED) Used for potentially fatal problems, lets the user choose how to react.
#define Warn(msg) { }
#define WarnF(msg, ...) { }

// (DISABLED) Used to warn if a condition is not met, lets the user choose how to react.
#define Assert(expr, msg) { }
#define AssertF(expr, msg, ...) { }
#endif // DEBUG_MESSAGES

class MsgLogger
{
private:
	std::string _indentStr = "    ";
    int _indentLevel = 0;

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

    static inline void IndentIncr() { GetInstance()._indentLevel++; }
    static inline void IndentDecr() { if (GetInstance()._indentLevel > 0) GetInstance()._indentLevel--; }

    static void ErrorMessage(const char *msg, const std::string &filePath, int line);
    static void ErrorMessage(const std::string &msg, const std::string &filePath, int line);

    static void WarningMessage(const char *msg, const std::string &filePath, int line);
    static void WarningMessage(const std::string &msg, const std::string &filePath, int line);

    static void DebugMessage(const char *msg, bool br = true);
    static void DebugMessage(const std::string &msg, bool br = true);
};
