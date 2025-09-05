#include "stdafx.h"
#include "InputBindings.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace InputBindings;


void BindingCollection::SaveBindings(const std::string &path)
{
	/*
	// Create JSON document
	json::Document doc;
	json::Document::AllocatorType &docAlloc = doc.GetAllocator();

	json::Value bindingsObj(json::kObjectType);
	{
		settingsObj.AddMember("Transform Snap", data.transformSnap, docAlloc);
		settingsObj.AddMember("UI Layout", SerializerUtils::SerializeString(data.layoutName, docAlloc), docAlloc);
		// Save all settings here...
	}
	doc.SetObject().AddMember("Settings", settingsObj, docAlloc);

	// Write doc to file
	std::ofstream file(path, std::ios::out);
	if (!file)
		ErrMsg("Could not save bindings file!");

	json::StringBuffer buffer;
	json::PrettyWriter<json::StringBuffer> writer(buffer);
	doc.Accept(writer);

	file << buffer.GetString();
	file.close();
	*/
}


static void LoadBindingSteps(json::Value &stepsArr, std::vector<std::unique_ptr<InputBindingStep>> &stepsVec, int &count)
{
	count = 0;

	size_t stepCount = stepsArr.Size();
	for (size_t i = 0; i < stepCount; i++)
	{
		// Resize the steps vector if we need more space
		if (i >= stepsVec.size())
			stepsVec.resize(stepsVec.size() + 4);

		json::Value &bindingStepObj = stepsArr[i];

		std::string bindingStepTypeName = bindingStepObj["Type"].GetString();
		InputBindingStepType bindingStepType = InputBindingStepType::None;

		for (const auto &pair : InputBindingStepNames)
		{
			if (pair.first == bindingStepTypeName)
			{
				bindingStepType = pair.second;
				break;
			}
		}

		std::string keyCodeName = bindingStepObj["Key"].GetString();
		KeyCode keyCode = KeyCode::None;

		for (const auto &pair : KeyCodeNames)
		{
			if (pair.first == keyCodeName)
			{
				keyCode = pair.second;
				break;
			}
		}

		float timeToDeactivate = bindingStepObj["Deactivation Time"].GetFloat();
		bool ignoreAbsorb = bindingStepObj["Ignore Absorb"].GetBool();

		switch (bindingStepType)
		{
		case InputBindings::InputBindingStepType::Simple:
		{
			std::string keyStateName = bindingStepObj["State"].GetString();
			KeyState keyState = KeyState::None;

			for (const auto &pair : KeyStateNames)
			{
				if (pair.first == keyStateName)
				{
					keyState = pair.second;
					break;
				}
			}

			stepsVec[count++] = std::move(std::make_unique<InputBindingStepSimple>(keyCode, keyState, timeToDeactivate, ignoreAbsorb));
			break;
		}

		case InputBindings::InputBindingStepType::Sustained:
		{
			float timeToActivate = bindingStepObj["Activation Time"].GetFloat();

			stepsVec[count++] = std::move(std::make_unique<InputBindingStepSustained>(keyCode, timeToActivate, timeToDeactivate, ignoreAbsorb));
			break;
		}

		default:
			break;
		}
	}
}

void BindingCollection::LoadBindings(const std::string &path)
{
	json::Document doc;
	{
		// Check if a bindings file exists. If so, use it.
		std::ifstream dataFile(path);

		if (!dataFile.is_open())
			return; // No data file exists, nothing to load.

		std::string fileContents;
		dataFile.seekg(0, std::ios::beg);
		fileContents.assign((std::istreambuf_iterator<char>(dataFile)), std::istreambuf_iterator<char>());
		dataFile.close();

		doc.Parse(fileContents.c_str());

		if (doc.HasParseError())
			ErrMsgF("Failed to parse JSON file: {}", (UINT)doc.GetParseError());
	}

	json::Value &bindings = doc["Bindings"];
	{
		if (!bindings.IsArray())
			return; // Invalid format

		size_t bindingCount = bindings.Size();
		if (bindingCount <= 0)
			return; // No bindings found

		for (size_t i = 0; i < bindingCount; i++)
		{
			json::Value &bindingObj = bindings[i];
			if (!bindingObj.IsObject())
				continue;

			std::string actionName = bindingObj["Action"].GetString();
			InputAction action = InputAction::None;

			for (const auto &pair : InputActionNames)
			{
				if (pair.first == actionName)
				{
					action = pair.second;
					break;
				}
			}

			std::string bindingTypeName = bindingObj["Type"].GetString();
			InputBindingType bindingType = InputBindingType::None;

			for (const auto &pair : InputBindingNames)
			{
				if (pair.first == bindingTypeName)
				{
					bindingType = pair.second;
					break;
				}
			}

			json::Value &stepsArr = bindingObj["Steps"];
			if (!stepsArr.IsArray())
				continue; // Invalid format

			std::vector<std::unique_ptr<InputBindingStep>> steps;
			int stepsCount = 0;
			LoadBindingSteps(stepsArr, steps, stepsCount);

			bool resetOnTrigger = bindingObj["Reset On Trigger"].GetBool();

			std::unique_ptr<InputBinding> binding = nullptr;
			switch (bindingType)
			{
			case InputBindings::InputBindingType::Single:
			{
				binding = std::make_unique<InputBindingSingle>(steps.data(), stepsCount, resetOnTrigger);
				break;
			}

			case InputBindings::InputBindingType::Exclusive:
			{
				json::Value &disallowedStepsArr = bindingObj["Disallowed Steps"];
				if (!disallowedStepsArr.IsArray())
					continue; // Invalid format

				std::vector<std::unique_ptr<InputBindingStep>> disallowedSteps;
				int disallowedStepsCount = 0;
				LoadBindingSteps(disallowedStepsArr, disallowedSteps, disallowedStepsCount);

				binding = std::make_unique<InputBindingExclusive>(steps.data(), stepsCount, disallowedSteps.data(), disallowedStepsCount, resetOnTrigger);
				break;
			}

			case InputBindings::InputBindingType::Combo:
			{
				binding = std::make_unique<InputBindingCombo>(steps.data(), stepsCount, resetOnTrigger);
				break;
			}

			case InputBindings::InputBindingType::Sequential:
			{
				float timeToRestart = bindingObj["Time To Restart"].GetFloat();

				binding = std::make_unique<InputBindingSequential>(steps.data(), stepsCount, timeToRestart, resetOnTrigger);
				break;
			}

			default:
				break;
			}

			SetBinding(action, std::move(binding));
		}
	}
}

