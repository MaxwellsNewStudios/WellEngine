#pragma once

#include <vector>
#include <DirectXCollision.h>
#include "Collision/ColliderShapes.h"
#include "Collision/Raycast.h"

// Forward declaration
struct MeshData;

class MeshCollider
{
private:
	struct Node
	{
		static constexpr UINT MAX_TRIS = 8;
		static constexpr UINT CHILD_COUNT = 8;

		const std::vector<Shape::Tri> *triBufferPtr = nullptr;
		std::vector<UINT> triIndices;

		std::unique_ptr<Node> children[CHILD_COUNT];
		bool isLeaf = true, isEmpty = true;

		dx::BoundingBox bounds, compactBounds;


		void Bake(const std::vector<UINT> *allTriIndices, const UINT depth);

		bool RaycastNode(const Shape::Ray &ray, Shape::RayHit &hit) const;

#ifdef DEBUG_BUILD
		void VisualizeTreeDepth(const dx::XMFLOAT4X4 &worldMatrix, UINT depthLeft, bool compact, const dx::XMFLOAT4 &color, bool drawTris, bool overlay, bool recursive) const;
#endif
	private:
		void CalculateCompactBounds();
	};

	std::unique_ptr<Node> _root;
	std::vector<Shape::Tri> triBuffer;

public:
	static constexpr UINT MAX_DEPTH = 6;

	MeshCollider() = default;
	~MeshCollider() = default;
	MeshCollider(const MeshCollider &other) = default;
	MeshCollider &operator=(const MeshCollider &other) = default;
	MeshCollider(MeshCollider &&other) = default;
	MeshCollider &operator=(MeshCollider &&other) = default;


	[[nodiscard]] bool Initialize(const MeshData &mesh, UINT submeshToUse);

	bool RaycastMesh(const Shape::Ray &ray, Shape::RayHit &hit) const;

#ifdef DEBUG_BUILD
	void VisualizeTreeDepth(const dx::XMFLOAT4X4 &worldMatrix, UINT depth, bool compact, const dx::XMFLOAT4 &color, bool drawTris = false, bool overlay = false, bool recursive = false) const;
#endif

	TESTABLE()
};