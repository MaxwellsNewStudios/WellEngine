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
const std::string RegistryDir = SolutionDir + "WellEngine\\Source\\Game\\Behaviours\\";
const std::string BehavioursDir = RegistryDir + "";
const std::string RegistryFile = RegistryDir + "BehaviourRegistry.cpp";
const std::string RegisterAttribute = "[[register_behaviour]]";
const std::string IncludeTag = "%INCLUDE%";
const std::string RegisterTag = "%REGISTER%";
const std::string CategoryTag = "%CATEGORY%";
const std::string RegistryTemplate = "\
// Automatically generated during build by BehaviourRegistration.\n\
// Scans for all behaviour definitions and includes them here for the behaviour factory to use.\n\
// NOTE: DO NOT MODIFY MANUALLY!\n\
\n\
#include \"stdafx.h\"\n\
#include \"BehaviourRegistry.h\"\n\
#include \"Behaviour.h\"\n\
" + IncludeTag + "\n\
#ifdef LEAK_DETECTION\n\
#define new			DEBUG_NEW\n\
#endif\n\
\n\
const std::map<std::string, std::function<Behaviour *(void)>> &BehaviourRegistry::Get()\n\
{\n\
	static const std::map<std::string, std::function<Behaviour *(void)>> behaviourMap = {\n\
" + RegisterTag + "\n\
	};\n\
\n\
	return behaviourMap;\n\
};\n\
\n\
#ifdef DEBUG_BUILD\n\
const std::map<std::string, std::string> &BehaviourRegistry::GetCategories()\n\
{\n\
	static const std::map<std::string, std::string> behaviourCategoryMap = {\n\
" + CategoryTag + "\n\
	};\n\
\n\
	return behaviourCategoryMap;\n\
};\n\
#endif\n";


static std::vector<std::string> ScanHeaderFileForBehaviours(const std::filesystem::path &filePath)
{
	std::ifstream headerReadFile(filePath);
	if (!headerReadFile.is_open())
	{
		std::cerr << "Failed to open header file: " << filePath << "\n";
		return {};
	}

	std::string headerCode((std::istreambuf_iterator<char>(headerReadFile)), std::istreambuf_iterator<char>());
	headerReadFile.close();

	std::vector<std::string> behaviourNames;

	// Scan for [[register_behaviour]] attributes
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
		// Ex: class [[register_behaviour]] B_Name : public Behaviour
		//                                  ^----^

		size_t nameOpeningPos = headerCode.find_first_not_of(" \n\t", offset);
		size_t nameClosingPos = headerCode.find_first_of(" :\n\t", nameOpeningPos);

		std::string name = headerCode.substr(nameOpeningPos, nameClosingPos - nameOpeningPos);
		behaviourNames.emplace_back(std::move(name));

		offset = nameClosingPos;
	}

	return behaviourNames;
}

struct BehaviourInfo
{
	std::vector<std::string> includes;
	std::vector<std::pair<std::string, std::string>> classes; // Name, Category
};
static void RecursiveHeaderSearch(const std::string &recursedPath, std::filesystem::directory_iterator &dirIter, BehaviourInfo &info)
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

			std::vector<std::string> behaviourNames = ScanHeaderFileForBehaviours(path);

			if (behaviourNames.empty())
				continue; // No behaviours found in this file

			// Add include for this header file
			std::string newInclude = recursedPath + name;
			DBG_MSG("Including '" << newInclude << "'\n");
			info.includes.emplace_back(std::move(newInclude));

			// Add each behaviour class found
			for (auto &behaviourName : behaviourNames)
			{
				DBG_MSG("Registering '" << behaviourName << "'\n");

				std::pair<std::string, std::string> behaviourEntry;
				behaviourEntry.first = std::move(behaviourName);

				behaviourEntry.second = recursedPath; // Script path

				// Replace backslashes with forward slashes for category
				if (behaviourEntry.second.find('\\') != std::string::npos)
				{
					for (char &c : behaviourEntry.second)
					{
						if (c == '\\')
							c = '/';
					}
				}
				
				info.classes.emplace_back(std::move(behaviourEntry));
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

// Recursively search the BehavioursDir for behaviour subclass definitions
static BehaviourInfo GatherBehaviours()
{
	std::cout << "Gathering Behaviours\n";

	std::filesystem::directory_iterator dirIter(BehavioursDir);
	BehaviourInfo outBehaviours;

	RecursiveHeaderSearch("", dirIter, outBehaviours);

	return outBehaviours;
}


static std::string GenerateRegistryCode(const BehaviourInfo &behaviourInfo)
{
	std::string output = RegistryTemplate;

	size_t maxClassNameLength = 0;
	size_t maxClassCategoryLength = 0;
	for (const auto &behaviourClass : behaviourInfo.classes)
	{
		maxClassNameLength = std::max(maxClassNameLength, behaviourClass.first.length());
		maxClassCategoryLength = std::max(maxClassCategoryLength, behaviourClass.second.length());
	}

	std::string registerCode = "";
	std::string categoryCode = "";
	for (const auto &behaviourClass : behaviourInfo.classes)
	{
		std::string name = behaviourClass.first;
		std::string category = behaviourClass.second;

		std::string refName = name;
		if (refName.starts_with("B_") || refName.starts_with("b_"))
			refName = refName.substr(2);

		size_t thisClassNameLength = name.length();
		size_t thisClassCategoryLength = category.length();

		size_t namePaddingLength = maxClassNameLength - thisClassNameLength;
		size_t categoryPaddingLength = maxClassCategoryLength - thisClassCategoryLength;

		std::string namePadding(namePaddingLength, ' ');
		std::string categoryPadding(categoryPaddingLength, ' ');

		registerCode += "\t\t{ \"" + refName + "\", " + namePadding + "[]() { return new " + name + "(); } " + namePadding + "},\n";
		categoryCode += "\t\t{ \"" + refName + "\", " + namePadding + "\"" + category + "\" " + categoryPadding + "},\n";
	}

	std::string includeCode = "";
	for (const auto &behaviourInclude : behaviourInfo.includes)
		includeCode += "#include \"Game/Behaviours/" + behaviourInclude + ".h\"\n";

	// Locate category tag
	{
		size_t categoryPos = output.find(CategoryTag);

		if (categoryPos == std::string::npos)
			std::cerr << "Category tag not found in template!\n";

		// Replace tag with generated code
		output.replace(categoryPos, CategoryTag.length(), categoryCode);
	}

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
	const BehaviourInfo behaviours = GatherBehaviours();

	const std::string registryCode = GenerateRegistryCode(behaviours);

	WriteRegistryFile(registryCode);

	std::cout << "Behaviour Registration Done.\n";
}
