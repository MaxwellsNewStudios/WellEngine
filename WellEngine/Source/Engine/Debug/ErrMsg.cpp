#include "stdafx.h"
#include "Debug/ErrMsg.h"
#include "Utils/StringUtils.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

void MsgLogger::ErrorMessage(const char *msg, const std::string &filePath, int line)
{
	MsgLogger &logger = GetInstance();
	std::string indent = logger.GetIndent();

    std::string file = filePath.substr(filePath.find_last_of("/\\") + 1);
    OutputDebugString(StringUtils::NarrowToWide(indent + std::format("ERROR: [{}({})]\n", file, line)).c_str());
    OutputDebugString(StringUtils::NarrowToWide(indent + msg).c_str());
    OutputDebugString(L"\n");

#undef cerr
    std::cerr << indent << msg << std::endl;
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

    std::string file = filePath.substr(filePath.find_last_of("/\\") + 1);
    OutputDebugString(StringUtils::NarrowToWide(indent + std::format("WARNING: [{}({})]\n", file, line)).c_str());
    OutputDebugString(StringUtils::NarrowToWide(indent + msg).c_str());
    OutputDebugString(L"\n");

#undef cerr
    std::cerr << indent << msg << std::endl;
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

#undef cout
    std::cout << indent << msg;
    if (br) std::cout << std::endl;
#define cout COUT_USAGE_WARNING
}
void MsgLogger::DebugMessage(const std::string &msg, bool br)
{
    DebugMessage(msg.c_str(), br);
}
