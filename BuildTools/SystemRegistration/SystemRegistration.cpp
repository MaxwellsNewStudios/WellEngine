#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

#ifdef _DEBUG
#define DBG_MSG(msg) std::cout << msg
#else
#define DBG_MSG(msg)
#endif


const std::string SolutionDir = TO_SOLUTION_PATH;
const std::string RegistryDir = SolutionDir + "WellEngine\\Source\\Game\\Systems\\";
const std::string SystemsDir = RegistryDir + "";
const std::string RegistryFile = RegistryDir + "SystemRegistry.cpp";
const std::string RegisterAttribute = "[[register_system]]";
const std::string IncludeTag = "%INCLUDE%";
const std::string RegisterTag = "%REGISTER%";
const std::string RegistryTemplate = "\
// Automatically generated during build by SystemRegistration.\n\
// Scans for all system definitions and includes them here for the System Manager to use.\n\
// NOTE: DO NOT MODIFY MANUALLY!\n\
\n\
#include \"stdafx.h\"\n\
#include \"SystemRegistry.h\"\n\
#include \"Game/Game.h\"\n\
#include \"System.h\"\n\
" + IncludeTag + "\n\
#ifdef LEAK_DETECTION\n\
#define new			DEBUG_NEW\n\
#endif\n\
\n\
std::vector<System *> WellEngine::SystemRegistry::GetSystems(Game *game)\n\
{\n\
	std::vector<System *> systemList = {\n\
" + RegisterTag + "\
	};\n\
\n\
	return systemList;\n\
};\n";


static std::vector<std::string> ScanHeaderFileForSystems(const std::filesystem::path &filePath)
{
	std::ifstream headerReadFile(filePath);
	if (!headerReadFile.is_open())
	{
		std::cerr << "Failed to open header file: " << filePath << "\n";
		return {};
	}

	std::string headerCode((std::istreambuf_iterator<char>(headerReadFile)), std::istreambuf_iterator<char>());
	headerReadFile.close();

	std::vector<std::string> systemNames;

	// Scan for [[register_system]] attributes
	size_t offset = 0, pos;
	while (true)
	{
		pos = headerCode.find(RegisterAttribute, offset);

		if (pos == std::string::npos)
			break; // No more attributes found

		offset = pos + RegisterAttribute.length();

		// Ensure the attribute is not in a comment
		{
			size_t lineStart = headerCode.rfind('\n', pos);
			size_t commentPos = headerCode.find("//", lineStart == std::string::npos ? 0 : lineStart);
			if (commentPos != std::string::npos && commentPos < pos)
			{
				DBG_MSG("Skipping commented-out attribute in file: '" << filePath << "', Pos: " << pos << ".\n");
				continue; // Attribute is in a comment, skip it
			}

			size_t blockCommentStart = headerCode.rfind("/*", pos);
			if (blockCommentStart != std::string::npos)
			{
				size_t blockCommentEnd = headerCode.rfind("*/", pos);
				if (blockCommentEnd == std::string::npos || blockCommentEnd < blockCommentStart)
				{
					DBG_MSG("Skipping commented-out attribute in file: '" << filePath << "', Pos: " << pos << ".\n");
					continue; // Attribute is in a block comment, skip it
				}
			}
		}

		// Extract the class name that follows the attribute
		// Ex: class [[register_system]] S_Name : public System
		//                                  ^----^

		size_t nameOpeningPos = headerCode.find_first_not_of(" \n\t", offset);
		size_t nameClosingPos = headerCode.find_first_of(" :\n\t", nameOpeningPos);

		std::string name = headerCode.substr(nameOpeningPos, nameClosingPos - nameOpeningPos);
		systemNames.emplace_back(std::move(name));

		offset = nameClosingPos;
	}

	return systemNames;
}

struct SystemInfo
{
	std::vector<std::string> includes;
	std::vector<std::string> classes;
};
static void RecursiveHeaderSearch(const std::string &recursedPath, std::filesystem::directory_iterator &dirIter, SystemInfo &info)
{
	for (const std::filesystem::directory_entry &entry : dirIter)
	{
		const auto &path = entry.path();

		if (entry.is_regular_file())        
		{
			const std::string filename = path.filename().string();
			size_t dotPos = filename.find_last_of('.');

			const std::string name = filename.substr(0, dotPos);
			const std::string ext = filename.c_str() + dotPos + 1;

			if (ext != "h")
				continue; // Skip non-header files

			std::vector<std::string> systemNames = ScanHeaderFileForSystems(path);

			if (systemNames.empty())
				continue; // No systems found in this file

			// Add include for this header file
			std::string newInclude = recursedPath + name;
			DBG_MSG("Including '" << newInclude << "'\n");
			info.includes.emplace_back(std::move(newInclude));

			// Add each system class found
			for (auto &systemName : systemNames)
			{
				DBG_MSG("Registering '" << systemName << "'\n");

				info.classes.emplace_back(std::move(systemName));
			}
		}
		else if (entry.is_directory())
		{
			// Recurse into subdirectory
			std::filesystem::directory_iterator subDirIter(path);

			std::string subDirName = path.filename().string();
			if (!subDirName.ends_with('/'))
				subDirName += '/';

			RecursiveHeaderSearch(recursedPath + subDirName, subDirIter, info);
		}
		else
		{
			std::cerr << "Skipping non-file, non-directory entry: " << path << "\n";
		}
	}
}

// Recursively search the SystemsDir for system subclass definitions
static SystemInfo GatherSystems()
{
	std::cout << "Gathering Systems\n";

	std::filesystem::directory_iterator dirIter(SystemsDir);
	SystemInfo outSystems;

	RecursiveHeaderSearch("", dirIter, outSystems);

	return outSystems;
}


static std::string GenerateRegistryCode(const SystemInfo &systemInfo)
{
	std::string output = RegistryTemplate;

	std::string registerCode = "";
	for (const auto &name : systemInfo.classes)
	{
		registerCode += "\t\tnew " + name + "(game),\n";
	}

	std::string includeCode = "";
	for (const auto &systemInclude : systemInfo.includes)
		includeCode += "#include \"Game/Systems/" + systemInclude + ".h\"\n";

	// Locate register tag
	{
		size_t registerPos = output.find(RegisterTag);

		if (registerPos == std::string::npos)
			std::cerr << "Register tag not found in template!\n";

		// Replace tag with generated code
		output.replace(registerPos, RegisterTag.length(), registerCode);
	}

	// Locate include tag
	{
		size_t includePos = output.find(IncludeTag);

		if (includePos == std::string::npos)
			std::cerr << "Include tag not found in template!\n";

		// Replace tag with generated code
		output.replace(includePos, IncludeTag.length(), includeCode);
	}

	return output;
}

static void WriteRegistryFile(const std::string &code)
{
	std::cout << "Writing Registry File\n";

	std::ofstream registryWriteFile(RegistryFile);
	if (!registryWriteFile.is_open())
		std::cerr << "Failed to open registry file for writing!\n";

	registryWriteFile << code;

	registryWriteFile.close();
}


int main()
{
	const SystemInfo systems = GatherSystems();

	const std::string registryCode = GenerateRegistryCode(systems);

	WriteRegistryFile(registryCode);

	std::cout << "System Registration Done.\n";
}
