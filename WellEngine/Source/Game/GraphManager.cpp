#include "stdafx.h"
#include "GraphManager.h"
#include "Behaviours/GraphNodeBehaviour.h"
#include "Collision/Intersections.h"
#include "Debug/DebugDrawer.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

inline static void ReconstructPath(int start, int goal, std::unordered_map<int, int> cameFrom, std::vector<int> &path)
{
	int current = goal;
	if (cameFrom.find(goal) == cameFrom.end())
		return; // No path found

	while (current != start) 
	{
		path.emplace_back(current);
		current = cameFrom[current];
	}

	std::reverse(path.begin(), path.end());
}

inline static float CalculateCost(XMFLOAT3 a, XMFLOAT3 b)
{
	return XMVectorGetX(XMVector3LengthEst(XMVectorSubtract(Load(b), Load(a))));
}

inline static void AStarStep(
	std::vector<Pathfinding::GraphNode> &graph,
	int start, int goal,
	std::unordered_map<int, int> &cameFrom,
	std::unordered_map<int, float> &cost)
{
	std::priority_queue<std::pair<int, float>, std::vector<std::pair<int, float>>, std::greater<std::pair<int, float>>> frontier;
	frontier.emplace(start, 0);

	cameFrom[start] = start;
	cost[start] = 0;

	while (!frontier.empty()) 
	{
		int current = frontier.top().first;
		frontier.pop();

		if (current == goal)
			return;

		for (int next : graph[current].connections)
		{
			float avgCost = 0.5f * (graph[current].point.w + graph[next].point.w);

			float newCost = avgCost + cost[current] + CalculateCost(
				To3(graph[current].point), 
				To3(graph[next].point)
			);

			if (cost.find(next) == cost.end() || newCost < cost[next])
			{
				cost[next] = newCost;
				float priority = newCost + CalculateCost(
					To3(graph[next].point), 
					To3(graph[goal].point)
				);

				frontier.emplace(next, priority);
				cameFrom[next] = current;
			}
		}
	}
}


void GraphManager::AStar(const Pathfinding::PointRelativeGraph &start, const Pathfinding::PointRelativeGraph &end, std::vector<XMFLOAT3> *points) const
{
	// Convert entity graph to indexed format
	std::vector<Pathfinding::GraphNode> graph = {};
	graph.reserve(_bakedNodes.size() + 2);

	// Copy nodes from _bakedNodes
	//graph.assign(_bakedNodes.begin(), _bakedNodes.end());
	for (int i = 0; i < _bakedNodes.size(); i++)
		graph.emplace_back(_bakedNodes[i]);
	
	// Add start node
	{
		int nodeOneIndex = -1;
		int nodeTwoIndex = -1;

		std::vector<int> connections = {};
		connections.reserve(2);

		auto it = std::find(_bakedNodes.begin(), _bakedNodes.end(), start.connectedNodeOne);
		if (it != _bakedNodes.end())
		{
			nodeOneIndex = static_cast<int>(std::distance(_bakedNodes.begin(), it));
			connections.emplace_back(nodeOneIndex);
			graph[nodeOneIndex].connections.emplace_back(static_cast<int>(graph.size()));
		}

		it = std::find(_bakedNodes.begin(), _bakedNodes.end(), start.connectedNodeTwo);
		if (it != _bakedNodes.end())
		{
			nodeTwoIndex = static_cast<int>(std::distance(_bakedNodes.begin(), it));
			connections.emplace_back(nodeTwoIndex);
			graph[nodeTwoIndex].connections.emplace_back(static_cast<int>(graph.size()));
		}

		XMFLOAT4 point = To4(start.point);
		point.w = 0.0f;

		graph.emplace_back(
			point,
			connections
		);

		// Remove all pre-existing paths connecting the two nodes of the start line
		/*auto &nodeOneConnections = graph[nodeOneIndex].connections;
		nodeOneConnections.erase(
			std::remove(nodeOneConnections.begin(), nodeOneConnections.end(), nodeTwoIndex),
			nodeOneConnections.end()
		);

		auto &nodeTwoConnections = graph[nodeTwoIndex].connections;
		nodeTwoConnections.erase(
			std::remove(nodeTwoConnections.begin(), nodeTwoConnections.end(), nodeOneIndex),
			nodeTwoConnections.end()
		);*/
	}

	// Add end node
	{
		int nodeOneIndex = -1;
		int nodeTwoIndex = -1;

		std::vector<int> connections = {};
		connections.reserve(2);

		auto it = std::find(_bakedNodes.begin(), _bakedNodes.end(), end.connectedNodeOne);
		if (it != _bakedNodes.end())
		{
			nodeOneIndex = static_cast<int>(std::distance(_bakedNodes.begin(), it));
			connections.emplace_back(nodeOneIndex);
			graph[nodeOneIndex].connections.emplace_back(static_cast<int>(graph.size()));
		}

		it = std::find(_bakedNodes.begin(), _bakedNodes.end(), end.connectedNodeTwo);
		if (it != _bakedNodes.end())
		{
			nodeTwoIndex = static_cast<int>(std::distance(_bakedNodes.begin(), it));
			connections.emplace_back(nodeTwoIndex);
			graph[nodeTwoIndex].connections.emplace_back(static_cast<int>(graph.size()));
		}

		XMFLOAT4 point = To4(end.point);
		point.w = 0.0f;

		graph.emplace_back(
			point,
			connections
		);

		// Remove all pre-existing paths connecting the two nodes of the end line
		/*auto &nodeOneConnections = graph[nodeOneIndex].connections;
		nodeOneConnections.erase(
			std::remove(nodeOneConnections.begin(), nodeOneConnections.end(), nodeTwoIndex),
			nodeOneConnections.end()
		);

		auto &nodeTwoConnections = graph[nodeTwoIndex].connections;
		nodeTwoConnections.erase(
			std::remove(nodeTwoConnections.begin(), nodeTwoConnections.end(), nodeOneIndex),
			nodeTwoConnections.end()
		);*/
	}

	// Find shortest path between the two points using A*
	int startNode = static_cast<int>(graph.size() - 2);
	int goalNode = static_cast<int>(graph.size() - 1);

	std::unordered_map<int, int> cameFrom = {};
	std::unordered_map<int, float> cost = {};
	AStarStep(graph, startNode, goalNode, cameFrom, cost);

	// Reconstruct path
	std::vector<int> path = {};
	ReconstructPath(startNode, goalNode, cameFrom, path);

	points->emplace_back(start.point);

	for (int i = 0; i < path.size(); i++)
	{
		Pathfinding::GraphNode &node = graph[path[i]];

		points->emplace_back(node.point.x, node.point.y, node.point.z);
	}
}

int GraphManager::GetNodeCount() const
{
	return static_cast<int>(_nodes.size());
}
int GraphManager::GetMineNodeCount() const
{
	return static_cast<int>(_mineNodes.size());
}
int GraphManager::GetBakedNodeCount() const
{
	return static_cast<int>(_bakedNodes.size());
}

void GraphManager::GetNodes(std::vector<GraphNodeBehaviour *> &nodes) const
{
	nodes = _nodes;
}
void GraphManager::GetMineNodes(std::vector<GraphNodeBehaviour *> &nodes) const
{
	nodes = _mineNodes;
}
const std::vector<Pathfinding::GraphNode> &GraphManager::GetBakedNodes() const
{
	return _bakedNodes;
}

void GraphManager::AddNode(GraphNodeBehaviour *node)
{
	if (node == nullptr)
		return;

	if (std::find(_nodes.begin(), _nodes.end(), node) != _nodes.end())
		return;

	_nodes.emplace_back(node);

	if (node->GetCost() < 0.1f)
		_mineNodes.emplace_back(node);
}
void GraphManager::RemoveNode(GraphNodeBehaviour *node)
{
	if (node == nullptr)
		return;

	auto it = std::find(_nodes.begin(), _nodes.end(), node);
	if (it == _nodes.end())
		return;
	_nodes.erase(it);

	it = std::find(_mineNodes.begin(), _mineNodes.end(), node);
	if (it == _mineNodes.end())
		return;
	_mineNodes.erase(it);
}
void GraphManager::UpdateNode(GraphNodeBehaviour *node)
{
	if (node == nullptr)
		return;

	auto it = std::find(_mineNodes.begin(), _mineNodes.end(), node);
	if (it == _mineNodes.end())
	{
		if (node->GetCost() < 0.1f)
			_mineNodes.emplace_back(node);
	}
	else
	{
		if (node->GetCost() >= 0.1f)
			_mineNodes.erase(it);
	}
}

void GraphManager::BakeNodes()
{
	if (_nodes.size() <= 0)
		return;

	SceneHolder *sceneHolder = _nodes[0]->GetScene()->GetSceneHolder();

	// Bake _nodes into _bakedNodes
	_bakedNodes.clear();
	_bakedMineNodes.clear();

	_bakedNodes.reserve(_nodes.size());
	_bakedMineNodes.reserve(_mineNodes.size());

	for (int i = 0; i < _nodes.size(); i++)
	{
		auto node = _nodes[i];
		if (node == nullptr)
			continue;

		auto &nodeConnections = node->GetConnections();

		std::vector<int> connections = {};
		connections.reserve(nodeConnections.size());

		for (int j = 0; j < nodeConnections.size(); j++)
		{
			auto connection = nodeConnections[j];
			if (connection == nullptr)
				continue;

			int connectedIndex = -1;
			auto it = std::find(_nodes.begin(), _nodes.end(), connection);
			if (it != _nodes.end())
				connectedIndex = static_cast<int>(std::distance(_nodes.begin(), it));

			connections.emplace_back(connectedIndex);
		}

		XMFLOAT4 point = To4(node->GetTransform()->GetPosition(World));
		point.w = node->GetCost();
		
		_bakedNodes.emplace_back(point, connections);

		auto it = std::find(_mineNodes.begin(), _mineNodes.end(), node);
		if (it != _mineNodes.end())
			_bakedMineNodes.emplace_back(point, connections);
	}

	// Remove all nodes from the scene for performance
	for (int i = 0; i < _nodes.size(); i++)
	{
		GraphNodeBehaviour *node = _nodes[i];
		if (!sceneHolder->RemoveEntity(node->GetEntity()))
		{
			ErrMsg("Failed to remove node from scene!");
			continue;
		}
	}

	_mineNodes.clear();
	_nodes.clear();
}

void GraphManager::UnbakeNodes(Scene *scene)
{
	std::vector<GraphNodeBehaviour *> nodeBehaviours;
	nodeBehaviours.resize(_bakedNodes.size());

	for (int i = 0; i < _bakedNodes.size(); i++)
	{
		Pathfinding::GraphNode &bakedNode = _bakedNodes[i];

		XMFLOAT4 nodePos = bakedNode.point;

		Entity *nodeEnt = nullptr;
		if (!scene->CreateGraphNodeEntity(&nodeEnt, &(nodeBehaviours[i]), To3(nodePos)))
		{
			ErrMsg("Failed to create temporary graph node entity!");
			return;
		}
	}

	for (int i = 0; i < _bakedNodes.size(); i++)
	{
		Pathfinding::GraphNode &bakedNode = _bakedNodes[i];

		if (nodeBehaviours[i])
		{
			nodeBehaviours[i]->SetEnabled(true);
			nodeBehaviours[i]->SetCost(bakedNode.point.w);

			for (int j = 0; j < bakedNode.connections.size(); j++)
				nodeBehaviours[i]->AddConnection(nodeBehaviours[bakedNode.connections[j]]);
		}
	}
}

void GraphManager::GetClosestPoint(XMFLOAT3 pos, XMFLOAT3 &closestPoint, bool onlyMines)
{
	float closestStartDist = FLT_MAX;
	auto &nodes = onlyMines ? _bakedMineNodes : _bakedNodes;

	XMVECTOR startVec = Load(pos);

	// TODO: Skip duplicates
	for (auto &node : nodes)
	{
		XMFLOAT3 connectionStart = To3(node.point);

		for (auto &connectionIndex : node.connections)
		{
			auto &connection = nodes[connectionIndex];

			XMFLOAT3 connectionEnd = To3(connection.point);

			// Find closest point on line to pos & dest
			XMFLOAT3 closestToStart = Collisions::ClosestPoint({ connectionStart, connectionEnd }, pos);

			XMVECTOR closestToStartVec = Load(closestToStart);

			float lengthToStart = XMVectorGetX(XMVector3LengthEst(XMVectorSubtract(startVec, closestToStartVec)));

			if (lengthToStart < closestStartDist)
			{
				closestStartDist = lengthToStart;
				closestPoint = closestToStart;
			}
		}
	}
}

bool GraphManager::isMinePoint(dx::XMFLOAT3 nodePos)
{
	auto nodePosVec = Load(nodePos);

	for (int i = 0; i < _bakedMineNodes.size(); i++)
	{
		auto pos = Load(_bakedMineNodes[i].point);

		float dist;
		XMStoreFloat(&dist, XMVector3LengthSq(nodePosVec - pos));

		if (dist <= 0.2f)
			return true;
	}

	return false;
}

bool GraphManager::isPoint(dx::XMFLOAT3 nodePos)
{
	auto nodePosVec = Load(nodePos);

	for (int i = 0; i < _bakedNodes.size(); i++)
	{
		auto pos = Load(_bakedNodes[i].point);

		float dist;
		XMStoreFloat(&dist, XMVector3LengthSq(nodePosVec - pos));

		if (dist <= 0.2f)
			return true;
	}

	return false;
}

void GraphManager::GetPath(XMFLOAT3 pos, XMFLOAT3 dest, std::vector<XMFLOAT3> *points) const
{
	ZoneScopedXC(RandomUniqueColor());

	// Find closest point on line to pos
	Pathfinding::GraphNode startNodeOne;
	Pathfinding::GraphNode startNodeTwo;
	Pathfinding::GraphNode destNodeOne;
	Pathfinding::GraphNode destNodeTwo;

	float closestStartDist = FLT_MAX;
	XMFLOAT3 closestStartPoint = { FLT_MAX, FLT_MAX, FLT_MAX };

	float closestDestDist = FLT_MAX;
	XMFLOAT3 closestDestPoint = { FLT_MAX, FLT_MAX, FLT_MAX };

	XMVECTOR startVec = Load(pos);
	XMVECTOR destVec = Load(dest);

	// TODO: Skip duplicates
	for (int i = 0; i < _bakedNodes.size(); i++)
	{
		const Pathfinding::GraphNode *node = &_bakedNodes[i];
		XMFLOAT3 connectionStart = To3(node->point);

		for (auto &connectionIndex : node->connections)
		{
			auto *connection = &_bakedNodes[connectionIndex];
			XMFLOAT3 connectionEnd = To3(connection->point);

			// Find closest point on line to pos & dest
			XMFLOAT3 closestToStart = Collisions::ClosestPoint({ connectionStart, connectionEnd }, pos);
			XMFLOAT3 closestToDest = Collisions::ClosestPoint({ connectionStart, connectionEnd }, dest);

			XMVECTOR closestToStartVec = Load(closestToStart);
			XMVECTOR closestToDestVec = Load(closestToDest);

			float lengthToStart = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(startVec, closestToStartVec)));
			float lengthToDest = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(destVec, closestToDestVec)));

			if (lengthToStart < closestStartDist)
			{
				closestStartDist = lengthToStart;
				closestStartPoint = closestToStart;
				startNodeOne = *node;
				startNodeTwo = *connection;
			}

			if (lengthToDest < closestDestDist)
			{
				closestDestDist = lengthToDest;
				closestDestPoint = closestToDest;
				destNodeOne = *node;
				destNodeTwo = *connection;
			}
		}
	}

	if ((startNodeOne == destNodeOne || startNodeOne == destNodeTwo) &&
		(startNodeTwo == destNodeOne || startNodeTwo == destNodeTwo))
	{
		points->emplace_back(pos);
		points->emplace_back(dest);
		return;
	}

	Pathfinding::PointRelativeGraph start{
		pos, closestStartPoint,
		startNodeOne, startNodeTwo
	};

	Pathfinding::PointRelativeGraph end{
		dest, closestDestPoint,
		destNodeOne, destNodeTwo
	};

	// Find shortest path between the two points using A*
	AStar(start, end, points);
}

#ifdef USE_IMGUI
bool GraphManager::RenderUI(XMFLOAT3 posA, XMFLOAT3 posB)
{
	DebugDrawer &drawer = DebugDrawer::Instance();

	static bool showNodes = false;
	if (ImGui::Checkbox("Show Nodes", &showNodes))
	{
		for (auto &node : _nodes)
		{
			if (node == nullptr)
				continue;
			node->SetShowNode(showNodes);
		}
	}
	if (showNodes)
	{
		static bool overlayNodes = true;
		ImGui::Checkbox("Overlay##OverlayNodes", &overlayNodes);

		for (int i = 0; i < _bakedNodes.size(); i++)
		{
			Pathfinding::GraphNode *node = &(_bakedNodes[i]);

			dx::XMFLOAT4 p = node->point;
			dx::XMFLOAT3 p1 = To3(p);
			dx::XMFLOAT3 p2 = p1;

			p1.y -= 0.15f;
			p2.y += 0.15f;

			drawer.DrawLine(
				p1, 
				p2,
				0.5f,
				node->point.w < 0.1f ? dx::XMFLOAT4(0, 0, 1, 1.0f) : dx::XMFLOAT4(1, 0, 0, 1.0f), 
				!overlayNodes
			);
		}
	}

	static bool showConnections = false;
	ImGui::Checkbox("Show Connections", &showConnections);
	if (showConnections)
	{
		static bool overlayConnections = true;
		ImGui::Checkbox("Overlay##OverlayConnections", &overlayConnections);

		for (auto &node : _nodes)
		{
			if (node == nullptr)
				continue;

			if (!node->DrawConnections())
			{
				ErrMsg("Failed to draw connections for GraphNodeBehaviour.");
				return false;
			}
		}

		for (int i = 0; i < _bakedNodes.size(); i++)
		{
			Pathfinding::GraphNode *node = &(_bakedNodes[i]);

			for (int connectionIndex : node->connections)
			{
				auto &connectionNode = _bakedNodes[connectionIndex];

				dx::XMFLOAT4 p1 = node->point;
				dx::XMFLOAT4 p2 = connectionNode.point;

				drawer.DrawLine(
					To3(p1), 
					To3(p2),
					0.33f,
					dx::XMFLOAT4(0, 1, 0, 0.1f), 
					!overlayConnections
				);
			}

		}
	}

	static bool showPath = false;
	ImGui::Checkbox("Show Path", &showPath);
	if (showPath)
	{
		static bool overlayPath = false;
		ImGui::Checkbox("Overlay", &overlayPath);

		std::vector<XMFLOAT3> points = {};
		GetPath(posA, posB, &points);

		DebugDrawer &drawer = DebugDrawer::Instance();
		drawer.DrawLineStrip(points.data(), static_cast<UINT>(points.size()), 0.25f, { 1,1,0,1 }, !overlayPath);
	}

	return true;
}
#endif

void GraphManager::CompleteDeserialization()
{
	for (auto &node : _nodes)
	{
		if (node == nullptr)
			continue;

		UpdateNode(node);
	}

#ifndef EDIT_MODE
	BakeNodes();
#endif
}
