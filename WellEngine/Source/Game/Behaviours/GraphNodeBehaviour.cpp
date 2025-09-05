#include "stdafx.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "GraphManager.h"
#include "Debug/DebugDrawer.h"
#include "Behaviours/MeshBehaviour.h"
#include "Scenes/Scene.h"
#include "Game.h"
#include "Behaviours/DebugPlayerBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

GraphNodeBehaviour::~GraphNodeBehaviour()
{
	int connectionSize = static_cast<int>(_connections.size());
	for (int i = 0; i < connectionSize; i++)
	{
		auto &connection = _connections[i];

		if (connection == nullptr)
			continue;

		RemoveConnection(connection);
		connectionSize--;
		i--;
	}

	GetScene()->GetGraphManager()->RemoveNode(this);
}

bool GraphNodeBehaviour::Start()
{
	if (_name.empty())
		_name = "GraphNodeBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();

	scene->GetGraphManager()->AddNode(this);

#ifdef DEBUG_BUILD
	Content *content = scene->GetContent();

	BoundingOrientedBox defaultBox = { { 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 } };
	UINT meshID = content->GetMeshID("Sphere");
	Material mat;
	mat.textureID = mat.ambientID = content->GetTextureID((_cost < 0.1f) ? "Blue" : "Red");

	MeshBehaviour *mesh = new MeshBehaviour(defaultBox, meshID, &mat, false, false);
	if (!mesh->Initialize(GetEntity()))
	{
		ErrMsg("Failed to initialize MeshBehaviour for GraphNodeBehaviour.");
		return false;
	}
	mesh->SetSerialization(false);
	mesh->SetEnabled(false);
#endif

	SetEnabled(false);
	//SetEnabled(true);
	return true;
}

bool GraphNodeBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Cost", _cost, docAlloc);

	if (_connections.size() <= 0)
		return true;

	json::Value connectionsArr(json::kArrayType);
	for (auto &connection : _connections)
	{
		if (connection == nullptr)
			continue;

		connectionsArr.PushBack(connection->GetEntity()->GetID(), docAlloc);
	}
	obj.AddMember("Connections", connectionsArr, docAlloc);

	return true;
}
bool GraphNodeBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Cost"))
		_cost = obj["Cost"].GetFloat();

	_deserializedConnections.clear();
	if (!obj.HasMember("Connections"))
		return true;

	const auto &connectionsArray = obj["Connections"].GetArray();
	_deserializedConnections.reserve(connectionsArray.Size());

	for (const json::Value &connection : connectionsArray)
	{
		_deserializedConnections.emplace_back(connection.GetUint());
	}

	return true;
}
void GraphNodeBehaviour::PostDeserialize()
{
	_connections.reserve(_deserializedConnections.size());
	for (auto &connection : _deserializedConnections)
	{
		Entity *ent = GetScene()->GetSceneHolder()->GetEntityByDeserializedID(connection);
		if (ent == nullptr)
			continue;

		GraphNodeBehaviour *node;
		ent->GetBehaviourByType<GraphNodeBehaviour>(node);
		if (node == nullptr)
			continue;

		AddConnection(node);
	}
	_deserializedConnections.clear();
}

#ifdef USE_IMGUI
bool GraphNodeBehaviour::RenderUI()
{
	UINT thisID = GetEntity()->GetID();

	float newCost = _cost;
	ImGui::Text("Cost:");
	ImGui::SameLine();
	if (ImGui::InputFloat("##NodeCost", &newCost))
		SetCost(newCost);

	ImGui::Dummy({ 0, 8 });
	ImGui::Text("Connections:");
	UINT connectionSize = static_cast<UINT>(_connections.size());
	for (UINT i = 0; i < connectionSize; i++)
	{
		ImGui::PushID(std::format("Node {} Connection {}", thisID, i).c_str());

		auto &connection = _connections[i];
		if (connection == nullptr)
			continue;

		if (ImGui::SmallButton(std::format("[{}] {}", i, connection->GetEntity()->GetName()).c_str()))
			GetScene()->GetDebugPlayer()->Select(connection->GetEntity(), ImGui::GetIO().KeyShift);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.5f, 0.55f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.5f, 0.65f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.5f, 0.75f, 0.8f));
		ImGui::SmallButton("?");
		if (ImGui::IsItemActive())
		{
			DebugDrawer &drawer = DebugDrawer::Instance();

			drawer.DrawLine(
				GetTransform()->GetPosition(World),
				connection->GetTransform()->GetPosition(World),
				1.0f,
				dx::XMFLOAT4(0, 1, 1, 1),
				false
			);
		}
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
		if (ImGui::SmallButton("X"))
		{
			RemoveConnection(connection);
			connectionSize--;
			i--;
		}
		ImGuiUtils::EndButtonStyle();

		ImGui::PopID();
	}

	return true;
}
#endif

void GraphNodeBehaviour::OnEnable()
{
	if (!GetScene()->GetSceneHolder()->IncludeEntityInTree(GetEntity()))
		Warn("Failed to include GraphNodeBehaviour entity in scene tree!");
}
void GraphNodeBehaviour::OnDisable()
{
	if (!GetScene()->GetSceneHolder()->ExcludeEntityFromTree(GetEntity()))
		Warn("Failed to exclude GraphNodeBehaviour entity from scene tree!");
}

void GraphNodeBehaviour::SetCost(float cost)
{
	_cost = cost;
	GraphManager *manager = GetScene()->GetGraphManager();
	if (manager)
		manager->UpdateNode(this);

#ifdef DEBUG_BUILD
	Content *content = GetScene()->GetContent();

	MeshBehaviour *mesh;
	if (GetEntity()->GetBehaviourByType<MeshBehaviour>(mesh))
	{
		Material mat = *mesh->GetMaterial();
		mat.textureID = mat.ambientID = cost < 0.1f ? content->GetTextureID("Blue") : content->GetTextureID("Red");
		
		if (!mesh->SetMaterial(&mat))
			Warn("Failed to set material for GraphNodeBehaviour mesh!");
	}
#endif
}
float GraphNodeBehaviour::GetCost() const
{
	return _cost;
}

const std::vector<GraphNodeBehaviour*> &GraphNodeBehaviour::GetConnections() const
{
	return _connections;
}

void GraphNodeBehaviour::AddConnection(GraphNodeBehaviour *connection, bool secondIteration)
{
	if (connection == nullptr)
	{
		ErrMsg("Failed to add connection, connection is nullptr!");
		return;
	}

	if (std::find(_connections.begin(), _connections.end(), connection) != _connections.end())
		return;

	_connections.emplace_back(connection);

	if (!secondIteration)
		connection->AddConnection(this, true);
}
void GraphNodeBehaviour::RemoveConnection(GraphNodeBehaviour *connection, bool secondIteration)
{
	if (connection == nullptr)
		return;

	auto it = std::find(_connections.begin(), _connections.end(), connection);
	if (it == _connections.end())
		return;

	_connections.erase(it);

	if (!secondIteration)
		connection->RemoveConnection(this, true);
}

bool GraphNodeBehaviour::DrawConnections()
{
	DebugDrawer &drawer = DebugDrawer::Instance();

	for (auto &connection : _connections)
	{
		if (connection == nullptr)
			continue;

		drawer.DrawLine(
			GetTransform()->GetPosition(World), 
			connection->GetTransform()->GetPosition(World),
			0.33f,
			dx::XMFLOAT4(0, 1, 0, 0.1f), 
			false
		);
	}

	return true;
}

#ifdef DEBUG_BUILD
void GraphNodeBehaviour::SetShowNode(bool show)
{
	SetEnabled(show);

	MeshBehaviour *mesh;
	if (GetEntity()->GetBehaviourByType<MeshBehaviour>(mesh))
		mesh->SetEnabled(show);
}
#endif