#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <format>
#include <filesystem>

const std::string SolutionDir = SOLUTION_DIR;
const std::string RegistryDir = SolutionDir + "WellEngine\\Source\\Game\\";
const std::string BehavioursDir = RegistryDir + "Behaviours\\";
const std::string RegistryFile = RegistryDir + "BehaviourRegistry.cpp";
const std::string RegisterAttribute = "[[register_behaviour]]";
const std::string IncludeTag = "%INCLUDE%";
const std::string RegisterTag = "%REGISTER%";
const std::string RegistryTemplate = "\
// Automatically generated during build by BehaviourRegistration.\n\
// Scans for all behaviour definitions and includes them here for the behaviour factory to use.\n\
\n\
#include \"stdafx.h\"\n\
#include \"BehaviourRegistry.h\"\n\
#include \"Behaviour.h\"\n\
" + IncludeTag + "\n\
\n\
#ifdef LEAK_DETECTION\n\
#define new			DEBUG_NEW\n\
#endif\n\
#pragma endregion\n\
\n\
const std::map<std::string_view, std::function<Behaviour *(void)>> &BehaviourRegistry::Get()\n\
{\n\
    static const std::map<std::string_view, std::function<Behaviour *(void)>> behaviourMap = {\n\
" + RegisterTag + "\n\
    };\n\
\n\
    return behaviourMap;\n\
};\n";


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

		// If found, extract the class name that follows
        // Ex: class [[register_behaviour]] ExampleBehaviour : public Behaviour
		//                                  ^--------------^

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
    std::vector<std::string> classes;
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
#ifdef _DEBUG
			std::cout << "Including '" << newInclude << "'\n";
#endif
            info.includes.emplace_back(std::move(newInclude));

            // Add each behaviour class found
            for (auto &behaviourName : behaviourNames)
            {
#ifdef _DEBUG
                std::cout << "Registering '" << behaviourName << "'\n";
#endif
                info.classes.emplace_back(std::move(behaviourName));
            }
        }
        else if (entry.is_directory())
        {
            // Recurse into subdirectory
            std::filesystem::directory_iterator subDirIter(path);

            std::string subDirName = path.filename().string();
            if (!subDirName.ends_with('\\'))
                subDirName += '\\';

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
    for (const auto &behaviourClass : behaviourInfo.classes)
		maxClassNameLength = std::max(maxClassNameLength, behaviourClass.length());

    std::string registerCode = "";
    for (const auto &behaviourClass : behaviourInfo.classes)
    {
		size_t thisClassNameLength = behaviourClass.length();
		size_t paddingLength = maxClassNameLength - thisClassNameLength;
		std::string padding(paddingLength, ' ');

        registerCode += "\t\t{ \"" + behaviourClass + "\", " + padding + "[]() { return new " + behaviourClass + "(); } " + padding + "},\n";
    }

	std::string includeCode = "";
    for (const auto &behaviourInclude : behaviourInfo.includes)
        includeCode += "#include \"Behaviours/" + behaviourInclude + ".h\"\n";

    // Locate register tag
    size_t registerPos = output.find(RegisterTag);

    if (registerPos == std::string::npos)
        std::cerr << "Register tag not found in template!\n";

	// Replace tag with generated code
    output.replace(registerPos, RegisterTag.length(), registerCode);

	// Locate include tag
	size_t includePos = output.find(IncludeTag);

	if (includePos == std::string::npos)
		std::cerr << "Include tag not found in template!\n";

	// Replace tag with generated code
	output.replace(includePos, IncludeTag.length(), includeCode);

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
