#include "stdafx.h"
#include "TimelineManager.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

SequenceStatus TimelineManager::RunSequence(const std::string &sequenceName, Transform* transform, bool loop, bool lerpToStart)
{
	auto it = _sequences.find(sequenceName);
	if (it == _sequences.end())
	{
		Warn("Sequence not found: " + sequenceName);
		return SequenceStatus::INVALID_SEQUENCE;
	}

	if (it->second.GetStatus() == SequenceStatus::RUNNING)
		return SequenceStatus::ALREADY_RUNNING;

	_runningSequences.emplace_back(sequenceName);
	DbgMsg("Running sequence " + sequenceName);
	return it->second.Run(transform, loop, lerpToStart);
}

SequenceStatus TimelineManager::StopSequence(const std::string &sequenceName)
{
	auto it = _sequences.find(sequenceName);
	if (it == _sequences.end())
	{
		Warn("Sequence not found: " + sequenceName);
		return SequenceStatus::INVALID_SEQUENCE;
	}

	if (it->second.GetStatus() == SequenceStatus::NOT_RUNNING)
		return SequenceStatus::NOT_RUNNING;

	it->second.Stop();
	_runningSequences.erase(std::remove(_runningSequences.begin(), _runningSequences.end(), sequenceName), _runningSequences.end());
	return SequenceStatus();
}

bool TimelineManager::Update(TimeUtils &time)
{
	for (auto it = _runningSequences.begin(); it != _runningSequences.end();)
	{
		if (_sequences[*it].Update(time) == SequenceStatus::FINISHED)
			it = _runningSequences.erase(it);
		else
			++it;
	}

	return true;
}

bool TimelineManager::AddSequence(const std::string &sequenceName, TimelineSequence sequence)
{
	if (!sequence.Validate())
	{
		Warn("Sequence is invalid: " + sequence.GetError());
		return true;
	}

	auto it = _sequences.find(sequenceName);
	if (it != _sequences.end())
	{
		Warn("Sequence already exists: " + sequenceName);
		return true;
	}

	_sequences.try_emplace(sequenceName, sequence);
	return true;
}

bool TimelineManager::RemoveSequence(const std::string &sequenceName)
{
	auto it = _sequences.find(sequenceName);
	if (it == _sequences.end())
	{
		Warn("Sequence not found: " + sequenceName);
		return true;
	}
	
	if (it->second.GetStatus() == SequenceStatus::RUNNING)
	{
		Warn("Cannot remove running sequence: " + sequenceName);
		return true;
	}

	_sequences.erase(sequenceName);
	return true;
}

bool TimelineManager::Serialize(std::string *code)
{
	// Serialize each sequence
	for (auto& sequence : _sequences)
	{
		*code += "(";
		// Start with the sequence name
		*code += sequence.first;
		*code += ",";
		*code += std::to_string(sequence.second.GetType());
		*code += ")";

		// Serialize the sequence
		if (!sequence.second.Serialize(code))
			Warn("Failed to serialize sequence: " + sequence.first);

		*code += "\n";
	}

	std::ofstream file(PATH_FILE_EXT(ASSET_PATH_SAVES, "Sequences", "txt"), std::ios::out);
	if (!file)
	{
		Warn("Could not open sequences file.");
		return true;
	}

	file << *code;
	file.close();

	return true;
}
bool TimelineManager::Deserialize()
{
	_sequences.clear();

	// Read file
	std::ifstream file(PATH_FILE_EXT(ASSET_PATH_SAVES, "Sequences", "txt"), std::ios::in);
	if (!file)
		return true;

	// Each line is a sequence
	std::string line;
	while (std::getline(file, line))
	{
		// Extract sequence details
		std::string sequenceDetails = line.substr(0, line.find_first_of('<'));
		std::string sequenceName = sequenceDetails.substr(1, sequenceDetails.find_first_of(',') - 1);
		int sequenceType = std::stoi(sequenceDetails.substr(sequenceDetails.find_first_of(',') + 1, sequenceDetails.size() - 1));

		// Extract sequence data
		TimelineSequence sequence = TimelineSequence();
		sequence.SetType(sequenceType);
		if (!sequence.Deserialize(line))
			Warn("Failed to deserialize sequence: " + sequenceName);
		
		// Insert sequence into map
		_sequences.try_emplace(sequenceName, sequence);
	}

	file.close();

	return true;
}

#ifdef USE_IMGUI
bool TimelineManager::RenderUI(Transform *transform)
{
	ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 260));
	// Left child
	{
		bool disabled = selectedSequenceIndex == -1 || 
			(GetSequence(selectedSequence).GetStatus() == SequenceStatus::RUNNING && 
				GetSequence(selectedSequence).GetStatus() == SequenceStatus::LERP_TO_START);

		// Run sequence button, loop checkbox, lerp to start checkbox
		{
			static bool loop = false;
			static bool lerpToStart = true;

			if (disabled)
				ImGui::BeginDisabled();

			if (ImGui::Button("Run sequence"))
				RunSequence(selectedSequence, transform, loop, lerpToStart);

			if (disabled)
				ImGui::EndDisabled();

			ImGui::SameLine();

			// Disable loop and lerp to start for lerp to point sequences
			bool disableCheckboxes = disabled || GetSequence(selectedSequence).GetType() == SequenceType::LERP_TO_POINT;
			if (disableCheckboxes)
			{
				loop = false;
				lerpToStart = false;
				ImGui::BeginDisabled();
			}

			ImGui::Checkbox("Loop", &loop);

			ImGui::SameLine();

			ImGui::Checkbox("Lerp to start", &lerpToStart);

			if (disableCheckboxes)
				ImGui::EndDisabled();

		}

		// Stop sequence button
		{
			if (disabled)
				ImGui::BeginDisabled();

			if (ImGui::Button("Stop sequence"))
			{
				StopSequence(selectedSequence);
			}

			if (disabled)
				ImGui::EndDisabled();
		}

		// Remove seqence button
		{
			if (disabled)
				ImGui::BeginDisabled();

			if (ImGui::Button("Remove sequence"))
			{
				if(RemoveSequence(selectedSequence))
				{
					selectedSequence.clear();
					selectedSequenceIndex = -1;
				}
			}

			if (disabled)
				ImGui::EndDisabled();
		}

		// Create sequence button
		{
			if (ImGui::Button("Create new sequence") && !creatingSequence)
			{
				creatingSequence = true;
			}
		}

	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("ChildR", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
	// Right child
	{
		if (ImGui::BeginTable("Sequences", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Sequences", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			int n = 0;
			for (auto& sequence : _sequences)
			{
				ImGui::TableNextColumn();

				std::string sequenceStatus = "";
				switch (sequence.second.GetStatus())
				{
				case SequenceStatus::RUNNING:
					sequenceStatus = "RUNNING";
					break;
				case SequenceStatus::NOT_RUNNING:
					sequenceStatus = "NOT RUNNING";
					break;
				case SequenceStatus::STOPPED:
					sequenceStatus = "STOPPED";
					break;
				case SequenceStatus::FINISHED:
					sequenceStatus = "FINISHED";
					break;
				case SequenceStatus::INVALID_SEQUENCE:
					sequenceStatus = "INVALID";
					break;
				case SequenceStatus::SEQUENCE_ERROR:
					sequenceStatus = "ERROR";
					break;
				case SequenceStatus::ALREADY_RUNNING:
					sequenceStatus = "ALREADY RUNNING";
					break;
				case SequenceStatus::LERP_TO_START:
					sequenceStatus = "LERP TO START";
					break;
				}


				if (ImGui::Selectable(sequence.first.c_str(), selectedSequenceIndex == n, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
				{
					selectedSequenceIndex = n;
					selectedSequence = sequence.first;
				}

				/*if (ImGui::TableSetColumnIndex(0))
					ImGui::Text(sequence.first.c_str());*/

				if (ImGui::TableSetColumnIndex(1))
					ImGui::Text(sequenceStatus.c_str());

				if (ImGui::TableSetColumnIndex(2))
					ImGui::Text(sequence.second.GetType() == SequenceType::SEQUENCE ? "Sequence" : "Lerp to point");

				n++;
			}
			ImGui::EndTable();
		}
		

	}
	ImGui::EndChild();


	// Create sequence window
	if (creatingSequence)
	{
		ImGui::Begin("Creating new sequence");
		
		// Left child
		ImGui::BeginChild("NewSequenceL", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 260));
		{
			// Sequence type dropdown
			{
				static int sequenceType = 0;
				ImGui::Text("Sequence type");
				ImGui::Combo("##SequenceType", &sequenceType, "Sequence\0Lerp to point\0");
				newSequence.SetType(sequenceType);
			}

			// Add waypoint button
			{
				if (ImGui::Button("Add waypoint"))
				{
					float newTime = 0.0f;
					if (!newSequence.GetWaypoints().empty())
						newTime = newSequence.GetWaypoints().back().GetTime() + 1.0f;

					TimelineWaypoint newWaypoint = { transform->GetPosition(), transform->GetRotation(), transform->GetScale(), newTime };
					newSequence.AddWaypoint(newWaypoint);
					DbgMsg("Waypoint added.");
				}
			}

			// Remove waypoint button
			{
				std::vector<TimelineWaypoint> waypoints = newSequence.GetWaypoints();
				if (waypoints.empty())
					ImGui::BeginDisabled();

				if (ImGui::Button("Remove waypoint"))
				{
					if (!waypoints.empty())
					{
						if (newSequence.RemoveLastWaypoint())
							DbgMsg("Waypoint removed.");
					}
				}

				if (waypoints.empty())
					ImGui::EndDisabled();
			}


			// Finish sequence button
			{
				static std::string error = "";
				// Create a new sequence and add all waypoints to it
				if (ImGui::Button("Finish"))
				{
					if (!newSequence.Validate())
					{
						error = newSequence.GetError();
						Warn("Sequence is invalid: " + error);
						ImGui::OpenPopup("Error");
					}
					else
					{
						ImGui::OpenPopup("Finish sequence");
					}
				}

				if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Sequence is invalid: %s", error.c_str());

					if (ImGui::Button("OK"))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				if (ImGui::BeginPopupModal("Finish sequence", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					static char buf[32] = "";
					bool enableFinish = true;
					// Sequence name input
					{
						ImGui::Text("Sequence name");
						ImGui::InputText("##", buf, 32); // ## removes the label
						if (buf[0] == '\0')
						{
							ImGui::TextColored(ImVec4(1, 0, 0, 1), "Name cannot be empty!");
							enableFinish = false;
						}

						if (_sequences.find(buf) != _sequences.end())
						{
							ImGui::TextColored(ImVec4(1, 0, 0, 1), "Name already exists!");
							enableFinish = false;
						}
					}

					// Finish button
					{
						if (!enableFinish)
							ImGui::BeginDisabled();

						if (ImGui::Button("Finish"))
						{
							// If adding the sequence was successful, close the popup
							if (AddSequence(buf, newSequence))
							{
								creatingSequence = false;
								buf[0] = '\0'; // Reset the input field
								ImGui::CloseCurrentPopup();
								newSequence = TimelineSequence();
							}
						}

						if (!enableFinish)
							ImGui::EndDisabled();
					}

					ImGui::SameLine();

					// Cancel button
					{
						if (ImGui::Button("Cancel (go back to editing)"))
						{
							buf[0] = '\0';
							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::EndPopup();
				}
			}

			// Cancel sequence button
			{
				if (ImGui::Button("Cancel"))
				{
					ImGui::OpenPopup("Cancel creation");
				}

				if (ImGui::BeginPopupModal("Cancel creation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Are you sure you want to cancel creating this sequence? All changes will be deleted!\nThis cannot be undone.");
					ImGui::Separator();

					// Yes button
					{
						if (ImGui::Button("Yes"))
						{
							creatingSequence = false;
							newSequence = TimelineSequence();
							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::SameLine();

					// No button
					{
						if (ImGui::Button("No"))
						{
							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::EndPopup();
				}
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// Right child
		ImGui::BeginChild("NewSequenceR", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY);
		{
			if (ImGui::BeginTable("Waypoints", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_NoHide);
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
				ImGui::TableHeadersRow();
				int n = 0;
				for (auto& waypoint : newSequence.GetWaypoints())
				{
					ImGui::TableNextColumn();
					std::string buttonLabel = std::to_string(n);
					bool open = ImGui::TreeNodeEx(buttonLabel.c_str());

					ImGui::TableNextColumn();
					float t = waypoint.GetTime();
					std::string inputLabel = "##" + std::to_string(n);
					const float pointOne = 0.1f;
					ImGui::InputScalar(inputLabel.c_str(), ImGuiDataType_Float, &t, &pointOne);
					if (t != waypoint.GetTime())
					{
						t = round(t * 1000) / 1000;
						waypoint.SetTime(t);
					}

					if (open)
					{
						ImGui::TableNextColumn();
							
						waypoint.RenderUI();
						ImGui::TableNextColumn();
						ImGui::TableNextColumn();


						ImGui::TreePop();
					}

					n++;
				}
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();

		ImGui::End();
	}
	
	return true;
}
#endif

std::unordered_map<std::string, TimelineSequence> TimelineManager::GetSequences() const
{
	return _sequences;
}

TimelineSequence TimelineManager::GetSequence(const std::string &sequenceName) const
{
	auto it = _sequences.find(sequenceName);
	if (it == _sequences.end())
	{
		Warn("Sequence not found: " + sequenceName);
		return TimelineSequence();
	}
	return it->second;
}
