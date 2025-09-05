#include "stdafx.h"
#include "MeshCollider.h"
#include "Content/ContentLoader.h"
#include "Debug/DebugDrawer.h"
#include "D3D/MeshD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

constexpr float COMPACT_BOUNDS_EPSILON = 0.001f;

//#define DEBUG_DRAW_RAYCAST
constexpr dx::XMFLOAT4 rayColor = { 0.0f, 0.0f, 1.0f, 0.75f };
constexpr dx::XMFLOAT4 triMissedColor = { 1.0f, 0.0f, 0.0f, 0.075f };
constexpr dx::XMFLOAT4 triHitColor = { 1.0f, 1.0f, 0.0f, 0.15f };
constexpr dx::XMFLOAT4 triClosestColor = { 0.0f, 1.0f, 0.0f, 0.6f };

void MeshCollider::Node::CalculateCompactBounds()
{
	if (isEmpty)
	{
		compactBounds = bounds;
		return;
	}

	dx::XMFLOAT3 
		minPos(FLT_MAX, FLT_MAX, FLT_MAX), 
		maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	if (isLeaf)
	{
		for (UINT index : triIndices)
		{
			const Shape::Tri &tri = (*triBufferPtr)[index];
			dx::XMFLOAT3 verts[3] = { tri.v0, tri.v1, tri.v2 };

			for (const dx::XMFLOAT3 &vert : verts)
			{
				minPos.x = min(minPos.x, vert.x);
				minPos.y = min(minPos.y, vert.y);
				minPos.z = min(minPos.z, vert.z);

				maxPos.x = max(maxPos.x, vert.x);
				maxPos.y = max(maxPos.y, vert.y);
				maxPos.z = max(maxPos.z, vert.z);
			}
		}
	}
	else
	{
		for (int i = 0; i < CHILD_COUNT; i++)
		{
			if (!children[i])
				continue;

			if (children[i]->isEmpty)
				continue;

			dx::BoundingBox &childCompactBounds = children[i]->compactBounds;

			dx::XMFLOAT3 childMin = {
				childCompactBounds.Center.x - childCompactBounds.Extents.x, 
				childCompactBounds.Center.y - childCompactBounds.Extents.y, 
				childCompactBounds.Center.z - childCompactBounds.Extents.z
			};

			dx::XMFLOAT3 childMax = {
				childCompactBounds.Center.x + childCompactBounds.Extents.x, 
				childCompactBounds.Center.y + childCompactBounds.Extents.y, 
				childCompactBounds.Center.z + childCompactBounds.Extents.z
			};

			minPos.x = min(minPos.x, childMin.x);
			minPos.y = min(minPos.y, childMin.y);
			minPos.z = min(minPos.z, childMin.z);

			maxPos.x = max(maxPos.x, childMax.x);
			maxPos.y = max(maxPos.y, childMax.y);
			maxPos.z = max(maxPos.z, childMax.z);
		}
	}

	dx::XMFLOAT3 boundsMin = {
		bounds.Center.x - bounds.Extents.x,
		bounds.Center.y - bounds.Extents.y,
		bounds.Center.z - bounds.Extents.z
	};

	dx::XMFLOAT3 boundsMax = {
		bounds.Center.x + bounds.Extents.x,
		bounds.Center.y + bounds.Extents.y,
		bounds.Center.z + bounds.Extents.z
	};

	minPos.x = max(minPos.x, boundsMin.x);
	minPos.y = max(minPos.y, boundsMin.y);
	minPos.z = max(minPos.z, boundsMin.z);

	maxPos.x = min(maxPos.x, boundsMax.x);
	maxPos.y = min(maxPos.y, boundsMax.y);
	maxPos.z = min(maxPos.z, boundsMax.z);

	dx::BoundingBox::CreateFromPoints(
		compactBounds,
		{ minPos.x, minPos.y, minPos.z, 0 },
		{ maxPos.x, maxPos.y, maxPos.z, 0 }
	);
}

void MeshCollider::Node::Bake(const std::vector<UINT> *allTriIndices, const UINT depth)
{
	if (!triBufferPtr)
		return;

	if (!allTriIndices)
		return;

	ZoneScopedXC(RandomUniqueColor());

	dx::BoundingBox errorMarginBounds = bounds;
	errorMarginBounds.Extents.x += COMPACT_BOUNDS_EPSILON * 0.5f;
	errorMarginBounds.Extents.y += COMPACT_BOUNDS_EPSILON * 0.5f;
	errorMarginBounds.Extents.z += COMPACT_BOUNDS_EPSILON * 0.5f;

	// Get all triangles that collide with this node
	int length = allTriIndices->size();
	for (int i = 0; i < length; i++)
	{
		UINT triIndex = (*allTriIndices)[i];
		const Shape::Tri &tri = (*triBufferPtr)[triIndex];

		if (GameCollision::BoxTriIntersect(errorMarginBounds, tri))
			triIndices.push_back(triIndex);
	}

	isEmpty = true;

	if (triIndices.size() <= 0)
	{
		isLeaf = true;
		return;
	}

	// If we can't go deeper, force this node to be a leaf
	if (depth >= MAX_DEPTH)
	{
		isLeaf = true;
		isEmpty = false;
		CalculateCompactBounds();
		return;
	}

	if (triIndices.size() <= MAX_TRIS)
	{
		isLeaf = true;
		isEmpty = false;
		CalculateCompactBounds();
		return;
	}

	// Split 
	const dx::XMFLOAT3
		center = bounds.Center,
		extents = bounds.Extents,
		min = { center.x - extents.x, center.y - extents.y, center.z - extents.z },
		max = { center.x + extents.x, center.y + extents.y, center.z + extents.z };

	children[0] = std::make_unique<Node>();
	children[1] = std::make_unique<Node>();
	children[2] = std::make_unique<Node>();
	children[3] = std::make_unique<Node>();
	children[4] = std::make_unique<Node>();
	children[5] = std::make_unique<Node>();
	children[6] = std::make_unique<Node>();
	children[7] = std::make_unique<Node>();

	dx::BoundingBox::CreateFromPoints(children[0]->bounds, { min.x,	   min.y,	 min.z,    0 }, { center.x, center.y, center.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[1]->bounds, { center.x, min.y,	 min.z,	   0 }, { max.x,	center.y, center.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[2]->bounds, { min.x,	   min.y,	 center.z, 0 }, { center.x, center.y, max.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[3]->bounds, { center.x, min.y,	 center.z, 0 }, { max.x,	center.y, max.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[4]->bounds, { min.x,	   center.y, min.z,	   0 }, { center.x, max.y,	  center.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[5]->bounds, { center.x, center.y, min.z,	   0 }, { max.x,	max.y,	  center.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[6]->bounds, { min.x,	   center.y, center.z, 0 }, { center.x, max.y,	  max.z,	0 });
	dx::BoundingBox::CreateFromPoints(children[7]->bounds, { center.x, center.y, center.z, 0 }, { max.x,	max.y,	  max.z,	0 });
	
	// Distribute triangles to children
	for (int i = 0; i < 8; i++)
	{
		children[i]->triBufferPtr = triBufferPtr;
		children[i]->Bake(&triIndices, depth + 1);

		if (!children[i]->isEmpty)
			isEmpty = false;
	}

	// Clear triIndices, since we have distributed them to children
	triIndices.clear();
	isLeaf = false;

	CalculateCompactBounds();
	return;
}

bool MeshCollider::Node::RaycastNode(const Shape::Ray &ray, Shape::RayHit &hit) const
{
	if (isEmpty)
		return false;

	ZoneScopedXC(RandomUniqueColor());

	DebugDrawer *dbgDrawer = nullptr;
#ifdef DEBUG_DRAW_RAYCAST
	dbgDrawer = &DebugDrawer::Instance();
#endif

	if (isLeaf)
	{
		bool hasHit = false;
		const Shape::Tri *currClosest = nullptr;

		// Check all items in leaf for intersection & return result.
		for (UINT index : triIndices)
		{
			const Shape::Tri &tri = (*triBufferPtr)[index];

			Shape::RayHit newHit;
			if (Raycast(ray, tri, newHit))
			{
				if (newHit.length >= hit.length)
				{
					if (dbgDrawer)
						dbgDrawer->DrawTri(tri, triHitColor, false, true);
					continue;
				}

				hasHit = true;
				hit = newHit;

				if (dbgDrawer && currClosest)
					dbgDrawer->DrawTri(*currClosest, triHitColor, false, true);
				currClosest = &tri;
			}
			else
			{
				if (dbgDrawer)
					dbgDrawer->DrawTri(tri, triMissedColor, false, true);
			}
		}

		if (dbgDrawer && currClosest)
			dbgDrawer->DrawTri(*currClosest, triClosestColor, false, true);

		return hasHit;
	}

	struct ChildHit { int index; float length; };
	std::vector<ChildHit> childHits = {
		{ 0, FLT_MAX }, { 1, FLT_MAX }, { 2, FLT_MAX }, { 3, FLT_MAX },
		{ 4, FLT_MAX }, { 5, FLT_MAX }, { 6, FLT_MAX }, { 7, FLT_MAX }
	};

	int childHitCount = CHILD_COUNT;
	for (int i = 0; i < childHitCount; i++)
	{
		const Node *child = children[childHits[i].index].get();
		if (Raycast(ray.origin, ray.direction, child->compactBounds, childHits[i].length))
		{
			if (childHits[i].length <= hit.length)
				continue;
		}

		// Remove child node from hits.
		childHits.erase(childHits.begin() + i);
		childHitCount--;
		i--;
	}

	// Insertion sort by length.
	for (int i = 1; i < childHitCount; i++)
	{
		int j = i;
		while (childHits[j].length < childHits[j - 1].length)
		{
			std::swap(childHits[j], childHits[j - 1]);
			if (--j <= 0)
				break;
		}
	}

	// Check children in order of closest to furthest.
	bool hasHit = false;

	Shape::RayHit newHit;
	newHit.length = hit.length;

	for (int i = 0; i < childHitCount; i++)
	{
		if (children[childHits[i].index]->RaycastNode(ray, newHit))
		{
			if (newHit.length >= hit.length)
				continue;
			
			hasHit = true;
			hit = newHit;
		}
	}

	return hasHit;
}


[[nodiscard]] bool MeshCollider::Initialize(const MeshData &mesh, UINT submeshToUse)
{
	ZoneScopedC(RandomUniqueColor());

	if (mesh.subMeshInfo.size() <= submeshToUse)
	{
		Warn("Invalid submesh index!");
		return false;
	}

	_root = std::make_unique<Node>();

	dx::XMFLOAT3 corners[8];
	mesh.boundingBox.GetCorners(corners);
	dx::BoundingBox::CreateFromPoints(_root->bounds, 8, corners, sizeof(dx::XMFLOAT3));

#if (MESH_COLLISION_DETAIL_REDUCTION == 3)
	return true;
#endif

	const MeshData::SubMeshInfo &submesh = mesh.subMeshInfo[submeshToUse];

	triBuffer.clear();
	triBuffer.reserve(submesh.nrOfIndicesInSubMesh / 3);

	for (UINT i = 0; i < submesh.nrOfIndicesInSubMesh; i += 3)
	{
		const UINT index = submesh.startIndexValue + i;

		const UINT index0 = mesh.indexInfo.indexData[index];
		const UINT index1 = mesh.indexInfo.indexData[index + 1];
		const UINT index2 = mesh.indexInfo.indexData[index + 2];

		if (index0 >= mesh.vertexInfo.nrOfVerticesInBuffer ||
			index1 >= mesh.vertexInfo.nrOfVerticesInBuffer ||
			index2 >= mesh.vertexInfo.nrOfVerticesInBuffer)
			continue; // Invalid index, skip this triangle.

		const ContentData::FormattedVertex *vertices = reinterpret_cast<ContentData::FormattedVertex *>(mesh.vertexInfo.vertexData);

		const ContentData::FormattedVertex &v0 = vertices[index0];
		const ContentData::FormattedVertex &v1 = vertices[index1];
		const ContentData::FormattedVertex &v2 = vertices[index2];

		triBuffer.emplace_back(
			dx::XMFLOAT3(v0.px, v0.py, v0.pz),
			dx::XMFLOAT3(v1.px, v1.py, v1.pz),
			dx::XMFLOAT3(v2.px, v2.py, v2.pz)
		);

		/*DebugDraw::Tri tri(
			DebugDraw::Vertex({ v0.px, v0.py, v0.pz }, { v0.nx, v0.ny, v0.nz, 1.0f }),
			DebugDraw::Vertex({ v1.px, v1.py, v1.pz }, { v1.nx, v1.ny, v1.nz, 1.0f }),
			DebugDraw::Vertex({ v2.px, v2.py, v2.pz }, { v2.nx, v2.ny, v2.nz, 1.0f })
		);

		DebugDrawer::Instance().DrawTri(tri, false);*/
	}

	std::vector<UINT> allTriIndices;
	allTriIndices.reserve(triBuffer.size());
	for (UINT i = 0; i < triBuffer.size(); i++)
		allTriIndices.push_back(i);

	_root.get()->triBufferPtr = &triBuffer;
	_root->Bake(&allTriIndices, 0);

	return true;
}

bool MeshCollider::RaycastMesh(const Shape::Ray &ray, Shape::RayHit &hit) const
{
	ZoneScopedC(RandomUniqueColor());

	if (_root == nullptr)
		return false;

	hit.length = ray.length > 0.0f ? ray.length : FLT_MAX;

	float len = hit.length; // In case Intersects() uses the initial dist value as a maximum. Docs don't specify.
	if (!Raycast(ray.origin, ray.direction, _root->bounds, len))
		return false;

	if (len > hit.length)	
		return false;

#if (MESH_COLLISION_DETAIL_REDUCTION == 3)
	if (Raycast(ray.origin, ray.direction, _root->bounds, hit.length))
	{
		using namespace DirectX;

		XMVECTOR rayOrigin = Load(ray.origin);
		XMVECTOR rayDir = Load(ray.origin);

		Store(hit.point, rayOrigin + (rayDir * hit.length));
		return true;
	}
	return false;
#endif

	bool hasHit = _root->RaycastNode(ray, hit);
#ifdef DEBUG_DRAW_RAYCAST
	DebugDrawer &dbgDrawer = DebugDrawer::Instance();
	if (hasHit)
	{
		dbgDrawer.DrawLine(ray.origin, hit.point, 0.001f, rayColor, true);
	}
	else
	{
		dbgDrawer.DrawRay(ray.origin, ray.direction, 0.001f, rayColor, true);
	}
#endif
	return hasHit;
}


#ifdef DEBUG_BUILD
void MeshCollider::Node::VisualizeTreeDepth(const dx::XMFLOAT4X4 &worldMatrix, UINT depthLeft, bool compact, const dx::XMFLOAT4 &color, bool drawTris, bool overlay, bool recursive) const
{
	if (isEmpty)
		return;

	if (depthLeft == 0)
	{
		dx::BoundingOrientedBox drawnBounds;
		dx::BoundingOrientedBox::CreateFromBoundingBox(drawnBounds, compact ? compactBounds : bounds);
		drawnBounds.Transform(drawnBounds, Load(worldMatrix));

		DebugDrawer::Instance().DrawBoxOBB(drawnBounds, color, !overlay);

		if (isLeaf)
		{
			if (drawTris)
			{
				for (UINT index : triIndices)
				{
					Shape::Tri tri = (*triBufferPtr)[index].Transformed(worldMatrix);
					dx::XMFLOAT3 triNormal = tri.Normal();

					float normalOffset = 0.00025f; // To avoid z-fighting
					tri.v0 = { tri.v0.x + triNormal.x * normalOffset, tri.v0.y + triNormal.y * normalOffset, tri.v0.z + triNormal.z * normalOffset };
					tri.v1 = { tri.v1.x + triNormal.x * normalOffset, tri.v1.y + triNormal.y * normalOffset, tri.v1.z + triNormal.z * normalOffset };
					tri.v2 = { tri.v2.x + triNormal.x * normalOffset, tri.v2.y + triNormal.y * normalOffset, tri.v2.z + triNormal.z * normalOffset };

					DebugDrawer::Instance().DrawTri(tri, {0,1,0,0.5f}, !overlay, false);
				}
			}
		}

		if (!recursive)
		{
			if (!isLeaf && drawTris)
			{
				for (int i = 0; i < CHILD_COUNT; i++)
				{
					if (!children[i])
						continue;

					children[i]->VisualizeTreeDepth(worldMatrix, depthLeft, compact, {0,0,0,0}, drawTris, overlay, recursive);
				}
			}

			return;
		}
		depthLeft++;
	}

	if (isLeaf)
		return;

	for (int i = 0; i < CHILD_COUNT; i++)
	{
		if (!children[i])
			continue;

		children[i]->VisualizeTreeDepth(worldMatrix, depthLeft - 1, compact, color, drawTris, overlay, recursive);
	}
}

void MeshCollider::VisualizeTreeDepth(const dx::XMFLOAT4X4 &worldMatrix, UINT depth, bool compact, const dx::XMFLOAT4 &color, bool drawTris, bool overlay, bool recursive) const
{
#if (MESH_COLLISION_DETAIL_REDUCTION == 3)
	return;
#endif

	if (depth > MAX_DEPTH)
		depth = MAX_DEPTH;

	if (!_root)
		return;

	_root->VisualizeTreeDepth(worldMatrix, depth, compact, color, drawTris, overlay, recursive);
}
#endif