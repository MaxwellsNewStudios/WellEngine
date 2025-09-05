#include "stdafx.h"
#include <comdef.h>
#include "StringUtils.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

std::string StringUtils::HResultToString(HRESULT hr)
{
    _com_error error(hr);
    const TCHAR *message = error.ErrorMessage();
    std::string strMessage = StringUtils::WideToNarrow(message);
    return std::string(message ? strMessage : "Unknown HRESULT");
}

std::wstring StringUtils::NarrowToWide(const std::string &narrow)
{
    if (narrow.empty())
        return { };

    const size_t reqLength = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.length(), nullptr, 0);

    std::wstring ret(reqLength, L'\0');

    MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.length(), &ret[0], (int)ret.length());
    return ret;
}

std::string StringUtils::WideToNarrow(const std::wstring &wide)
{
    if (wide.empty())
        return { };

    const size_t reqLength = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), nullptr, 0, nullptr, nullptr);

    std::string ret(reqLength, '\0');

    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &ret[0], (int)ret.length(), nullptr, nullptr);
	return ret;
}

std::string StringUtils::Trim(const std::string &str, const std::string &trimmed)
{
    const auto strBegin = str.find_first_not_of(trimmed);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(trimmed);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}
