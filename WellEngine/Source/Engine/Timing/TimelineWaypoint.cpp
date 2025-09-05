#include "stdafx.h"
#include "TimelineWaypoint.h"
#include <sstream>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

const dx::XMFLOAT3A& TimelineWaypoint::GetPosition() const
{
	return _worldPosition;
}

const dx::XMFLOAT4A& TimelineWaypoint::GetRotation() const
{
	return _worldRotation;
}

const dx::XMFLOAT3A& TimelineWaypoint::GetScale() const
{
	return _worldScale;
}

const float& TimelineWaypoint::GetTime() const
{
	return _time;
}

void TimelineWaypoint::SetTime(const float time)
{
	_time = time;
}

bool TimelineWaypoint::Serialize(std::string *code) const
{
	// TODO: Add error handling

	*code += "(" + std::to_string(_worldPosition.x) + "," + std::to_string(_worldPosition.y) + "," + std::to_string(_worldPosition.z) + ")"; // Position
	*code += "/"; // Separator
	*code += "(" + std::to_string(_worldRotation.x) + "," + std::to_string(_worldRotation.y) + "," + std::to_string(_worldRotation.z) + "," + std::to_string(_worldRotation.w) + ")"; // Rotation
	*code += "/"; // Separator
	*code += "(" + std::to_string(_worldScale.x) + "," + std::to_string(_worldScale.y) + "," + std::to_string(_worldScale.z) + ")"; // Scale
	*code += "/"; // Separator
	*code += "(" + std::to_string(_time) + ")"; // Time

	return true;
}

bool TimelineWaypoint::Deserialize(const std::string &code)
{
	// Split the components by '/'
	std::vector<std::string> components;
	std::stringstream ss(code);
	std::string item;
	while (std::getline(ss, item, '/'))
	{
		components.emplace_back(item.substr(1, item.size() - 2)); // Remove parentheses
	}

	std::string pos = components[0];
	std::string rot = components[1];
	std::string scale = components[2];
	std::string time = components[3];

	// Parse position
	ss = std::stringstream(pos);
	std::vector<float> posComponents;
	while (std::getline(ss, item, ','))
	{
		posComponents.emplace_back(std::stof(item));
	}
	_worldPosition = dx::XMFLOAT3A(posComponents[0], posComponents[1], posComponents[2]);

	// Parse rotation
	ss = std::stringstream(rot);
	std::vector<float> rotComponents;
	while (std::getline(ss, item, ','))
	{
		rotComponents.emplace_back(std::stof(item));
	}
	_worldRotation = dx::XMFLOAT4A(rotComponents[0], rotComponents[1], rotComponents[2], rotComponents[3]);

	// Parse scale
	ss = std::stringstream(scale);
	std::vector<float> scaleComponents;
	while (std::getline(ss, item, ','))
	{
		scaleComponents.emplace_back(std::stof(item));
	}
	_worldScale = dx::XMFLOAT3A(scaleComponents[0], scaleComponents[1], scaleComponents[2]);

	// Parse time
	_time = std::stof(time);

	return true;
}

#ifdef USE_IMGUI
bool TimelineWaypoint::RenderUI()
{
	if (ImGui::BeginTable("table_nested", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
	{
		// Table setup
		{
			ImGui::TableSetupColumn("##type", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
			ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending);
		}

		// Position
		{
			ImGui::TableHeadersRow();
			ImGui::TableNextColumn();
			ImGui::Text("Position");
			ImGui::TableNextColumn();
			ImGui::DragFloat("##posx", &_worldPosition.x, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##posy", &_worldPosition.y, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##posz", &_worldPosition.z, 0.005f);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("--");
			ImGui::TableNextRow();
		}

		// Rotation
		{
			ImGui::TableNextColumn();
			ImGui::Text("Rotation");
			ImGui::TableNextColumn();
			ImGui::DragFloat("##rotx", &_worldRotation.x, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##roty", &_worldRotation.y, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##rotz", &_worldRotation.z, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##rotw", &_worldRotation.w, 0.005f);
			ImGui::TableNextRow();
		}

		// Scale
		{
			ImGui::TableNextColumn();
			ImGui::Text("Scale");
			ImGui::TableNextColumn();
			ImGui::DragFloat("##scalex", &_worldScale.x, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##scaley", &_worldScale.z, 0.005f);
			ImGui::TableNextColumn();
			ImGui::DragFloat("##scalez", &_worldScale.z, 0.005f);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("--");
		}

		ImGui::EndTable();
	}
	return true;
}
#endif
