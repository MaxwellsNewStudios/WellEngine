#include "stdafx.h"
#include "DirectoryManager.h"

namespace fs = std::filesystem;


fs::file_time_type DirectoryManager::GetMostRecentFileDate(const std::string &directoryPath)
{
	fs::file_time_type mostRecentDate = fs::file_time_type::min();
	for (const auto &entry : fs::recursive_directory_iterator(directoryPath))
	{
		if (fs::is_regular_file(entry.path()))
		{
			auto lastWriteTime = fs::last_write_time(entry.path());
			if (lastWriteTime > mostRecentDate)
			{
				mostRecentDate = lastWriteTime;
			}
		}
	}
	return mostRecentDate;
}
