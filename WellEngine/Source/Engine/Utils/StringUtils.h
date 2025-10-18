#pragma once
#include <string>
#include <initializer_list>

namespace StringUtils
{
	std::string HResultToString(HRESULT hr);


	std::wstring NarrowToWide(const std::string &narrow);
	std::wstring NarrowToWide(std::string_view narrow);

	std::string WideToNarrow(const std::wstring &wide);
	std::string WideToNarrow(std::wstring_view wide);

	std::string Trim(const std::string &str, const std::string &trimmed = " \t");

	template<typename... Args>
	inline std::string PathChain(Args... args) 
	{
		std::string result;
		bool first = true;
		(std::initializer_list<int>{ (result += (first ? "" : "\\") + args, first = false, 0)... });
		return result;
	}

	inline int SplitVersionString(const char *version, int *parts, int maxSections)
	{
		char buffer[16]{};
		int section = 0;
		int charIndex = 0;

		for (int i = 0; section < maxSections; i++)
		{
			if (charIndex >= 15)
			{
				while (version[i] != '.' && version[i] != '\0')
					i++;
			}

			if (version[i] == '.' || version[i] == '\0')
			{
				buffer[charIndex] = '\0';
				parts[section++] = atoi(buffer);
				charIndex = 0;

				if (version[i] == '\0')
					break;
				continue;
			}

			if (!isdigit(version[i]))
				continue;

			buffer[charIndex++] = version[i];
		}

		return section;
	}
}
