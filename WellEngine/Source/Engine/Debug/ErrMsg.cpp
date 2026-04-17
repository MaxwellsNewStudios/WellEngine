#include "stdafx.h"
#include "ErrMsg.h"
#include "Source/Engine/Utils/StringUtils.h"
#include <windows.h>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

int OpenWinMessageBox(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    return MessageBoxA(hWnd, lpText, lpCaption, uType);
}

void MsgLogger::ErrorMessage(const char *msg, const std::string &filePath, int line)
{
	MsgLogger &logger = GetInstance();
	std::string indent = logger.GetIndent();
    std::string msgStr = msg;

    // Replace every '\n' with "\n" + indent
    size_t pos = 0;
    while ((pos = msgStr.find('\n', pos)) != std::string::npos)
    {
        msgStr.insert(pos + 1, indent);
        pos += indent.length() + 1; // Move past the inserted indent and the newline
    }

    std::string file = filePath.substr(filePath.find_last_of("/\\") + 1);
    OutputDebugString(StringUtils::NarrowToWide(indent + std::format("ERROR: [{}({})]\n", file, line)).c_str());
    OutputDebugString(StringUtils::NarrowToWide(indent + msgStr).c_str());
    OutputDebugString(L"\n");

#undef cerr
    std::cerr << indent << msgStr << std::endl;
#define cerr CERR_USAGE_WARNING
}
void MsgLogger::ErrorMessage(const std::string &msg, const std::string &filePath, int line)
{
    ErrorMessage(msg.c_str(), filePath, line);
}

void MsgLogger::WarningMessage(const char *msg, const std::string &filePath, int line)
{
    MsgLogger &logger = GetInstance();
    std::string indent = logger.GetIndent();
    std::string msgStr = msg;

    // Replace every '\n' with "\n" + indent
    size_t pos = 0;
    while ((pos = msgStr.find('\n', pos)) != std::string::npos)
    {
        msgStr.insert(pos + 1, indent);
        pos += indent.length() + 1; // Move past the inserted indent and the newline
    }

    std::string file = filePath.substr(filePath.find_last_of("/\\") + 1);
    OutputDebugString(StringUtils::NarrowToWide(indent + std::format("WARNING: [{}({})]\n", file, line)).c_str());
    OutputDebugString(StringUtils::NarrowToWide(indent + msgStr).c_str());
    OutputDebugString(L"\n");

#undef cerr
    std::cerr << indent << msgStr << std::endl;
#define cerr CERR_USAGE_WARNING
}
void MsgLogger::WarningMessage(const std::string &msg, const std::string &filePath, int line)
{
    WarningMessage(msg.c_str(), filePath, line);
}

void MsgLogger::DebugMessage(const char *msg, bool br)
{
    MsgLogger &logger = GetInstance();
    std::string indent = logger.GetIndent();
    std::string msgStr = msg;

	// Replace every '\n' with "\n" + indent
    size_t pos = 0;
    while ((pos = msgStr.find('\n', pos)) != std::string::npos)
    {
        msgStr.insert(pos + 1, indent);
        pos += indent.length() + 1; // Move past the inserted indent and the newline
    }

#undef cout
    std::cout << indent << msgStr;
    if (br) std::cout << std::endl;
#define cout COUT_USAGE_WARNING
}
void MsgLogger::DebugMessage(const std::string &msg, bool br)
{
    DebugMessage(msg.c_str(), br);
}
