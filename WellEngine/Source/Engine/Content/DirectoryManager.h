#pragma once

#include <string>
#include <filesystem>

namespace WellEngine::DirectoryManager
{
	// Find the date of the most recently modified file in the specified directory.
	std::filesystem::file_time_type GetMostRecentFileDate(const std::string &directoryPath);
}

