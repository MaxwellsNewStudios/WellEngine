#pragma once
#include <vector>
#include <DirectXMath.h>
#include <DirectXCollision.h>

class GraphNodeBehaviour;

namespace Pathfinding
{
	struct GraphNode
	{
		dx::XMFLOAT4 point{};
		std::vector<int> connections;

		bool operator==(const GraphNode &other) const
		{
			return point.x == other.point.x 
				&& point.y == other.point.y 
				&& point.z == other.point.z;
		}
	};

	struct PointRelativeGraph
	{
		dx::XMFLOAT3 point, projectedPoint;
		GraphNode connectedNodeOne, connectedNodeTwo;
	};
}

class GraphManager
{
private:
	std::vector<GraphNodeBehaviour *> _nodes = {};
	std::vector<GraphNodeBehaviour *> _mineNodes = {};

	std::vector<Pathfinding::GraphNode> _bakedNodes = {};
	std::vector<Pathfinding::GraphNode> _bakedMineNodes = {};

public:
	GraphManager() = default;
	~GraphManager() = default;

	void AStar(const Pathfinding::PointRelativeGraph &start, const Pathfinding::PointRelativeGraph &end, std::vector<dx::XMFLOAT3> *points) const;

	int GetNodeCount() const;
	int GetMineNodeCount() const;
	int GetBakedNodeCount() const;
	void GetNodes(std::vector<GraphNodeBehaviour *> &nodes) const;
	void GetMineNodes(std::vector<GraphNodeBehaviour *> &nodes) const;
	[[nodiscard]] const std::vector<Pathfinding::GraphNode> &GetBakedNodes() const;

	void AddNode(GraphNodeBehaviour *node);
	void RemoveNode(GraphNodeBehaviour *node);
	void UpdateNode(GraphNodeBehaviour *node);

	void BakeNodes();
	void UnbakeNodes(Scene *scene);

	void GetClosestPoint(dx::XMFLOAT3 pos, dx::XMFLOAT3 &closestPoint, bool onlyMines = false);
	[[nodiscard]] bool isMinePoint(dx::XMFLOAT3 nodePos);
	[[nodiscard]] bool isPoint(dx::XMFLOAT3 nodePos);

	void GetPath(dx::XMFLOAT3 pos, dx::XMFLOAT3 dest, std::vector<dx::XMFLOAT3> *points) const;

	[[nodiscard]] bool RenderUI(dx::XMFLOAT3 posA, dx::XMFLOAT3 posB);

	void CompleteDeserialization();
};