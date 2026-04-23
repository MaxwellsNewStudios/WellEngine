#include "stdafx.h"
#include "Quadtree.h"
#include "Engine/Collision/Raycast.h"
#include "Game/Scenes/Scene.h"
#include "Game/Entity.h"
#include "Game/Behaviour.h"
#include "Game/Behaviours/Rendering/Mesh/B_Mesh.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


void Quadtree::Node::Split(const UINT depth)
{
	ZoneScopedXC(RandomUniqueColor());

	const dx::XMFLOAT3
		center = bounds.Center,
		extents = bounds.Extents,
		min = { center.x - extents.x, center.y - extents.y, center.z - extents.z },
		max = { center.x + extents.x, center.y + extents.y, center.z + extents.z };

	children[0] = std::make_unique<Node>(_maxDepth, _maxItemsInNode);
	children[1] = std::make_unique<Node>(_maxDepth, _maxItemsInNode);
	children[2] = std::make_unique<Node>(_maxDepth, _maxItemsInNode);
	children[3] = std::make_unique<Node>(_maxDepth, _maxItemsInNode);

	dx::BoundingBox::CreateFromPoints(children[0]->bounds, { min.x, min.y, min.z, 0 }, { center.x, max.y, center.z, 0 });
	dx::BoundingBox::CreateFromPoints(children[1]->bounds, { center.x, min.y, min.z, 0 }, { max.x, max.y, center.z, 0 });
	dx::BoundingBox::CreateFromPoints(children[2]->bounds, { min.x, min.y, center.z, 0 }, { center.x, max.y, max.z, 0 });
	dx::BoundingBox::CreateFromPoints(children[3]->bounds, { center.x, min.y, center.z, 0 }, { max.x, max.y, max.z, 0 });

	for (int i = 0; i < data.size(); i++)
	{
		Entity *item = data[i];
		if (item != nullptr)
		{
			bool hasBounds = false;
			dx::BoundingOrientedBox itemBounds;
			if (!item->HasBounds(false, itemBounds))
			{
				dx::XMFLOAT3A pos = item->GetTransform()->GetPosition();
				itemBounds.Center = { pos.x, pos.y, pos.z };
				itemBounds.Extents = { 0.1f, 0.1f, 0.1f };
			}

			uint8_t lastChildInsertedInto;
			UINT totalChildrenInserts = 0;

			for (int j = 0; j < CHILD_COUNT; j++)
			{
				bool inserted = children[j]->Insert(item, itemBounds, nullptr, depth + 1);

				if (inserted)
				{
					totalChildrenInserts++;
					lastChildInsertedInto = (uint8_t)j;
				}
			}

			// Update the item's culling placement, only if the following criteria are met:
			// - The current path reaches this depth.
			// - It was only inserted into one child.
			if (totalChildrenInserts == 1)
			{
				auto &itemPath = item->GetCullingPlacement().quadTreePath;
				if (itemPath.steps.size() == depth)
				{
					itemPath.steps.emplace_back(lastChildInsertedInto);
				}
			}
		}
	}

	data.clear();
	isEmpty = false;
	isLeaf = false;
	isDirty = true;
}

UINT Quadtree::Node::Insert(Entity *item, const dx::BoundingOrientedBox &itemBounds, Culling::TreePath *path, const UINT depth)
{
	ZoneScopedXC(RandomUniqueColor());

	// DirectXCollision OBB-AABB intersection seems to be impercise, causing entities to flicker in and out of nodes from tiny movements.
	// HACK: To rely less on the faulty intersection check, we first check only if center point is within bounds.
	// If it isn't, we check if any of the corners are within bounds. Only if all of these checks fail, we do the intersection check.
	if (!bounds.Contains(Load(itemBounds.Center)))
	{
		dx::XMFLOAT3 corners[dx::BoundingOrientedBox::CORNER_COUNT];
		itemBounds.GetCorners(corners);

		bool anyCornerInside = false;
		for (const dx::XMFLOAT3 &corner : corners)
		{
			if (bounds.Contains(Load(corner)))
			{
				anyCornerInside = true;
				break;
			}
		}

		if (!anyCornerInside)
			if (!bounds.Intersects(itemBounds))
				return false;
	}

	isDirty = true;
	isEmpty = false;

	if (isLeaf)
	{
		if (depth >= _maxDepth || data.size() < _maxItemsInNode)
		{
#ifdef TRACY_DETAILED
			ZoneNamedXNC(emplaceInfoZone, "Emplaced Entity", RandomUniqueColor(), true);
			const std::string boundsStr = std::format("C:({}, {}), E:({}, {})",
				bounds.Center.x, bounds.Center.z,
				bounds.Extents.x, bounds.Extents.z
			);
			ZoneTextXV(emplaceInfoZone, boundsStr.c_str(), boundsStr.size());
#endif
			data.emplace_back(item);
			return true;
		}

		Split(depth);
	}

	uint8_t lastChildInsertedInto;
	UINT totalChildrenInserts = 0;

	for (UINT i = 0; i < CHILD_COUNT; i++)
	{
		auto &child = children[i];
		if (!child)
			continue;

		bool inserted = child->Insert(item, itemBounds, path, depth + 1);

		if (inserted)
		{
			totalChildrenInserts++;
			lastChildInsertedInto = (uint8_t)i;
		}
	}

	if (path)
	{
		if (totalChildrenInserts == 1)
		{
			path->steps.insert(path->steps.begin(), lastChildInsertedInto);
		}
		else if (totalChildrenInserts > 1)
		{
			// If the item was inserted into multiple children, we can't determine a single path.
			// Clear the path to indicate this. The parent node will start a new path with this as the tip.
			path->steps.clear();
		}
	}

	return totalChildrenInserts > 0;
}

void Quadtree::Node::Remove(Entity *item, const Culling::TreePath &path, const UINT depth)
{
	ZoneScopedXC(RandomUniqueColor());

	if (isLeaf) // End of the path, check for entity here
	{
		size_t num = std::erase_if(data, [item](const Entity *otherItem) {
			return item == otherItem;
			});

		if (num > 0)
		{
#ifdef TRACY_DETAILED
			ZoneNamedXNC(eraseInfoZone, "Erased Entity", RandomUniqueColor(), true);
			const std::string boundsStr = std::format("C:({}, {}), E:({}, {})",
				bounds.Center.x, bounds.Center.z,
				bounds.Extents.x, bounds.Extents.z
			);
			ZoneTextXV(eraseInfoZone, boundsStr.c_str(), boundsStr.size());
#endif

			isDirty = true;
		}

		return;
	}
	else if (depth >= path.steps.size()) // Look for entity in all children
	{
		for (int i = 0; i < CHILD_COUNT; i++)
		{
			auto &child = children[i];
			if (child)
			{
				child->Remove(item, path, depth + 1);
				isDirty |= child->isDirty;
			}
		}
	}
	else // Follow the path only, ignore other children
	{
		uint8_t step = path.steps[depth];
		if (step >= CHILD_COUNT)
			return;

		auto &child = children[step];
		if (!child)
			return;

		child->Remove(item, path, depth + 1);
		isDirty |= child->isDirty;
	}

	std::vector<Entity *> containingItems;
	containingItems.reserve(_maxItemsInNode);

	for (int i = 0; i < CHILD_COUNT; i++)
	{
		auto &child = children[i];

		if (child)
		{
			if (!child->isLeaf)
				return;

			if (!child->data.empty())
			{
				for (Entity *childItem : child->data)
				{
					if (!childItem)
						continue;

					if (std::ranges::find(containingItems, childItem) == containingItems.end())
					{
						if (containingItems.size() >= _maxItemsInNode)
							return;

						containingItems.emplace_back(childItem);
					}
				}
			}
		}
	}

	for (int i = 0; i < CHILD_COUNT; i++)
	{
		children[i] = nullptr;
	}

	isLeaf = true;
	data.clear();
	for (Entity *newItem : containingItems)
		data.emplace_back(newItem);
}

void Quadtree::Node::RecalculateCullingBounds()
{
	ZoneScopedXC(RandomUniqueColor());

	if (!isDirty)
		return;
	isDirty = false;
	isEmpty = false;

	if (isLeaf)
	{
		if (data.empty())
		{
			// If no data, set node to empty
			isEmpty = true;
			return;
		}
		else
		{
			std::vector<dx::XMFLOAT3> allCorners;
			allCorners.reserve(data.size() * 8);

			for (Entity *item : data)
			{
				if (item == nullptr || !item->IsEnabled())
					continue;

				dx::BoundingOrientedBox itemBounds;
				item->StoreEntityBounds(itemBounds);

				dx::XMFLOAT3 corners[8];
				itemBounds.GetCorners(corners);
				allCorners.insert(allCorners.end(), std::begin(corners), std::end(corners));
			}

			if (allCorners.empty())
			{
				// If no valid items, set node to empty
				isEmpty = true;
				return;
			}

			dx::BoundingBox::CreateFromPoints(
				cullingBounds,
				allCorners.size(),
				allCorners.data(),
				sizeof(dx::XMFLOAT3)
			);

			// Clamp the culling bounds to the node's bounds
			dx::XMFLOAT3 realCenter = bounds.Center;
			dx::XMFLOAT3 realExtents = bounds.Extents;

			dx::XMFLOAT3 cullingCorners[8];
			cullingBounds.GetCorners(cullingCorners);

			for (int i = 0; i < 8; i++)
			{
				cullingCorners[i].x = std::clamp(cullingCorners[i].x, realCenter.x - realExtents.x, realCenter.x + realExtents.x);
				cullingCorners[i].z = std::clamp(cullingCorners[i].z, realCenter.z - realExtents.z, realCenter.z + realExtents.z);
			}

			dx::BoundingBox::CreateFromPoints(
				cullingBounds,
				8,
				cullingCorners,
				sizeof(dx::XMFLOAT3)
			);
		}
	}
	else
	{
		for (int i = 0; i < CHILD_COUNT; i++)
			children[i].get()->RecalculateCullingBounds();

		// Combine all children bounds into this node's culling bounds
		dx::BoundingBox *nonEmpty[CHILD_COUNT]{};
		UINT added = 0;
		for (int i = 0; i < CHILD_COUNT; i++)
		{
			auto child = children[i].get();
			if (child->isEmpty)
				continue;

			nonEmpty[added++] = &child->cullingBounds;
		}

		if (added == 0)
		{
			isEmpty = true;
			return;
		}

		if (added == 1)
		{
			cullingBounds = *nonEmpty[0];
			return;
		}

		dx::BoundingBox::CreateMerged(cullingBounds, *nonEmpty[0], *nonEmpty[1]);
		for (int i = 2; i < added; i++)
			dx::BoundingBox::CreateMerged(cullingBounds, cullingBounds, *nonEmpty[i]);
	}
}

void Quadtree::Node::AddToVector(std::vector<Entity *> &containingItems, const UINT depth) const
{
	if (isEmpty)
		return;

	if (isLeaf)
	{
		for (Entity *item : data)
		{
			if (item == nullptr)
				continue;

			if (!item->IsEnabled())
				continue;

			if (std::ranges::find(containingItems, item) == containingItems.end())
				containingItems.emplace_back(item);
		}

		return;
	}

	for (int i = 0; i < CHILD_COUNT; i++)
	{
		if (children[i] == nullptr)
			continue;

		children[i]->AddToVector(containingItems, depth + 1);
	}
}

void Quadtree::Node::FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems, bool mustFullyContain, const UINT depth) const
{
	if (isEmpty)
		return;

	ZoneScopedXC(RandomUniqueColor());

	switch (frustum.Contains(cullingBounds))
	{
	case dx::DISJOINT:
		return;

	case dx::CONTAINS:
		AddToVector(containingItems, depth + 1);
		break;

	case dx::INTERSECTS:
		if (isLeaf)
		{
			for (Entity *item : data)
			{
				if (item == nullptr)
					continue;

				if (!item->IsEnabled())
					continue;

				if (std::ranges::find(containingItems, item) == containingItems.end())
				{
					dx::BoundingOrientedBox itemBounds;
					item->StoreEntityBounds(itemBounds);

					if (mustFullyContain)
					{
						if (frustum.Contains(itemBounds) == dx::CONTAINS)
							containingItems.emplace_back(item);
					}
					else
					{
						if (frustum.Intersects(itemBounds))
							containingItems.emplace_back(item);
					}
				}
			}

			return;
		}

		for (int i = 0; i < CHILD_COUNT; i++)
		{
			if (children[i] == nullptr)
				continue;

			children[i]->FrustumCull(frustum, containingItems, mustFullyContain, depth + 1);
		}
		break;
	}
}

void Quadtree::Node::BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain, const UINT depth) const
{
	if (isEmpty)
		return;

	ZoneScopedXC(RandomUniqueColor());

	switch (box.Contains(cullingBounds))
	{
	case dx::DISJOINT:
		return;

	case dx::CONTAINS:
		AddToVector(containingItems, depth + 1);
		break;

	case dx::INTERSECTS:
		if (isLeaf)
		{
			for (Entity *item : data)
			{
				if (item == nullptr)
					continue;

				if (!item->IsEnabled())
					continue;

				if (std::ranges::find(containingItems, item) == containingItems.end())
				{
					dx::BoundingOrientedBox itemBounds;
					item->StoreEntityBounds(itemBounds);

					if (mustFullyContain)
					{
						if (box.Contains(itemBounds) == dx::CONTAINS)
							containingItems.emplace_back(item);
					}
					else if (box.Intersects(itemBounds))
						containingItems.emplace_back(item);
				}
			}

			return;
		}

		for (int i = 0; i < CHILD_COUNT; i++)
		{
			if (children[i] == nullptr)
				continue;

			children[i]->BoxCull(box, containingItems, mustFullyContain, depth + 1);
		}
		break;
	}
}

void Quadtree::Node::BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain, const UINT depth) const
{
	if (isEmpty)
		return;

	ZoneScopedXC(RandomUniqueColor());

	switch (box.Contains(cullingBounds))
	{
	case dx::DISJOINT:
		return;

	case dx::CONTAINS:
		AddToVector(containingItems, depth + 1);
		break;

	case dx::INTERSECTS:
		if (isLeaf)
		{
			for (Entity *item : data)
			{
				if (item == nullptr)
					continue;

				if (!item->IsEnabled())
					continue;

				if (std::ranges::find(containingItems, item) == containingItems.end())
				{
					dx::BoundingOrientedBox itemBounds;
					item->StoreEntityBounds(itemBounds);

					if (mustFullyContain)
					{
						if (box.Contains(itemBounds) == dx::CONTAINS)
							containingItems.emplace_back(item);
					}
					else if (box.Intersects(itemBounds))
						containingItems.emplace_back(item);
				}
			}

			return;
		}

		for (int i = 0; i < CHILD_COUNT; i++)
		{
			if (children[i] == nullptr)
				continue;

			children[i]->BoxCull(box, containingItems, mustFullyContain, depth + 1);
		}
		break;
	}
}

bool Quadtree::Node::RaycastNode(const dx::XMFLOAT3 &orig, const dx::XMFLOAT3 &dir, float &length, Entity *&entity, bool cheap) const
{
	if (isEmpty)
		return false;

	ZoneScopedXC(RandomUniqueColor());

	if (isLeaf)
	{
		// Check all items in leaf for intersection & return result.
		for (Entity *item : data)
		{
			if (item == nullptr)
				continue;

			if (!item->IsEnabled())
				continue;

			if (!item->IsDebugSelectable())
				continue;

			if (!item->IsRaycastTarget())
				continue;
			
			bool performCheapCheck = true;

			if (!cheap)
			{
				const Shape::Ray ray(orig, dir);

				B_Mesh *meshBehaviour = nullptr;
				if (item->GetBehaviourByType<B_Mesh>(meshBehaviour))
				{
					performCheapCheck = false;

					MeshD3D11 *mesh = item->GetScene()->GetContent()->GetMesh(meshBehaviour->GetMeshID());
					const MeshCollider &meshCollider = mesh->GetMeshCollider();

					const dx::XMFLOAT4X4A &meshMatrix = item->GetTransform()->GetMatrix(World);
					dx::XMFLOAT4X4A meshMatrixInv; Store(meshMatrixInv, XMMatrixInverse(nullptr, Load(meshMatrix)));

					const Shape::Ray localRay = ray.Transformed(meshMatrixInv);
					Shape::RayHit localHit;

					if (meshCollider.RaycastMesh(localRay, localHit))
					{
						localHit.Transform(meshMatrix);

						if (localHit.length >= length)
							continue;

						length = localHit.length;
						entity = item;
					}
				}
			}

			if (performCheapCheck)
			{
				bool hasBounds = false;
				dx::BoundingOrientedBox itemBounds;
				if (!item->HasBounds(false, itemBounds))
					continue;

				float newLength = 0.0f;
				if (Raycast(orig, dir, itemBounds, newLength))
				{
					if (newLength >= length)
						continue;

					length = newLength;
					entity = item;
				}
			}
		}

		return (entity != nullptr);
	}

	struct ChildHit { int index; float length; };
	std::vector<ChildHit> childHits = {
		{ 0, FLT_MAX }, { 1, FLT_MAX }, { 2, FLT_MAX }, { 3, FLT_MAX }
	};

	int childHitCount = CHILD_COUNT;
	for (int i = 0; i < childHitCount; i++)
	{
		const Node *child = children[childHits[i].index].get();
		if (Raycast(orig, dir, child->bounds, childHits[i].length))
			continue;

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
	length = FLT_MAX;
	entity = nullptr;

	for (int i = 0; i < childHitCount; i++)
	{
		if (length < childHits[i].length)
			return true;

		if (!children[childHits[i].index]->RaycastNode(orig, dir, length, entity, cheap))
		{
			length = FLT_MAX;
			entity = nullptr;
		}

	}

	return (entity != nullptr);
}

bool Quadtree::Node::RaycastNode(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const
{
	if (isEmpty)
		return false;

	ZoneScopedXC(RandomUniqueColor());

	if (isLeaf)
	{
		// Check all items in leaf for intersection & return result.
		for (Entity *item : data)
		{
			if (item == nullptr)
				continue;

			if (!item->IsEnabled())
				continue;

			if (!item->IsDebugSelectable())
				continue;

			if (!item->IsRaycastTarget())
				continue;
			
			B_Mesh *meshBehaviour = nullptr;
			if (item->GetBehaviourByType<B_Mesh>(meshBehaviour))
			{
				MeshD3D11 *mesh = item->GetScene()->GetContent()->GetMesh(meshBehaviour->GetMeshID());
				const MeshCollider &meshCollider = mesh->GetMeshCollider();

				const dx::XMFLOAT4X4A &meshMatrix = item->GetTransform()->GetMatrix(World);
				dx::XMFLOAT4X4A meshMatrixInv; Store(meshMatrixInv, XMMatrixInverse(nullptr, Load(meshMatrix)));

				const Shape::Ray localRay = ray.Transformed(meshMatrixInv);
				Shape::RayHit localHit;

				if (meshCollider.RaycastMesh(localRay, localHit))
				{
					localHit.Transform(meshMatrix);

					if (localHit.length >= hit.length)
						continue;

					hit = localHit;
					ent = item;
				}
			}
		}

		return (ent != nullptr);
	}

	struct ChildHit { int index; float length; };
	std::vector<ChildHit> childHits = {
		{ 0, FLT_MAX }, { 1, FLT_MAX }, { 2, FLT_MAX }, { 3, FLT_MAX }
	};

	int childHitCount = CHILD_COUNT;
	for (int i = 0; i < childHitCount; i++)
	{
		const Node *child = children[childHits[i].index].get();
		if (Raycast(ray.origin, ray.direction, child->bounds, childHits[i].length))
			continue;

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

	Entity *newEnt = nullptr;
	Shape::RayHit newHit;
	newHit.length = hit.length;

	// Check children in order of closest to furthest.
	for (int i = 0; i < childHitCount; i++)
	{
		if (children[childHits[i].index]->RaycastNode(ray, newHit, newEnt))
		{
			if (newHit.length >= hit.length)
				continue;

			ent = newEnt;
			hit = newHit;
		}

	}

	return (ent != nullptr);
}

#ifdef USE_IMGUI
#include "Game/Behaviours/Debug/B_DebugManager.h"

void Quadtree::Node::DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const
{
	if (isEmpty)
		return;

	if (full || isLeaf)
	{
		boxCollection.emplace_back(culling ? cullingBounds : bounds);

		if (isLeaf)
			return;
	}

	for (int i = 0; i < CHILD_COUNT; i++)
		children[i]->DebugGetStructure(boxCollection, full, culling);
}

void Quadtree::Node::DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const
{
	if (isEmpty)
		return;

	switch (frustum.Contains(cullingBounds))
	{
	case dx::DISJOINT:
		break;

	case dx::CONTAINS:
		if (full || isLeaf)
		{
			boxCollection.emplace_back(culling ? cullingBounds : bounds);

			if (isLeaf)
				return;
		}

		for (int i = 0; i < CHILD_COUNT; i++)
			children[i]->DebugGetStructure(boxCollection, full, culling);
		break;

	case dx::INTERSECTS:
		if (full || isLeaf)
		{
			boxCollection.emplace_back(culling ? cullingBounds : bounds);

			if (isLeaf)
				return;
		}

		for (int i = 0; i < CHILD_COUNT; i++)
			children[i]->DebugGetStructure(boxCollection, frustum, full, culling);
		break;
	}
}

bool Quadtree::Node::RenderUI(std::string &path, bool drawFullPath, bool drawDataRec, const UINT depth)
{
	if (drawBounds || drawFullPath)
	{
		DebugDrawer::Instance().DrawBoxAABB(cullingBounds, boundsColor, false, true);
	}

	if (isLeaf && (drawDataRec || drawData))
	{
		for (Entity *item : data)
		{
			if (item == nullptr)
				continue;

			dx::BoundingOrientedBox itemBounds;
			item->StoreEntityBounds(itemBounds);

			DebugDrawer::Instance().DrawBoxOBB(itemBounds, dataColor, false, true);
		}
	}

	if (selectedChild == -1)
	{
		ImGui::Text(path.c_str());

		ImGui::Separator();

		ImGui::Text("Depth: %u", depth);
		ImGui::Text("Data Count: %zu", data.size());
		ImGui::Text("Dirty: %s", isDirty ? "Yes" : "No");
		ImGui::Text("Leaf Node: %s", isLeaf ? "Yes" : "No");
		ImGui::Text("Empty: %s", isEmpty ? "Yes" : "No");

		ImGui::Separator();

		ImGui::ColorEdit4("Bounds Color", &boundsColor.x, ImGuiColorEditFlags_NoInputs);
		ImGui::ColorEdit4("Data Color", &dataColor.x, ImGuiColorEditFlags_NoInputs);

		ImGui::Checkbox("Draw Bounds", &drawBounds);
		ImGui::Checkbox("Draw Data", &drawData);
		ImGui::Checkbox("Recursive", &recursiveDraw);

		ImGui::Separator();

		if (ImGui::Button("Back"))
			return false;
		ImGui::Dummy({ 0, 8 });

		if (!isLeaf)
		{
			if (ImGui::Button("NW", { 64, 64 }))
				selectedChild = 2;
			ImGui::SameLine();
			if (ImGui::Button("NE", { 64, 64 }))
				selectedChild = 3;

			if (ImGui::Button("SW", { 64, 64 }))
				selectedChild = 0;
			ImGui::SameLine();
			if (ImGui::Button("SE", { 64, 64 }))
				selectedChild = 1;
		}
		else
		{
			if (ImGui::TreeNode("Leaf Data"))
			{
				for (Entity *item : data)
				{
					if (item == nullptr || !item->IsEnabled())
						continue;

					if (ImGui::Button(std::format("{}##TreeData{}", item->GetName(), (size_t)item).c_str()))
					{
						item->GetScene()->GetDebugManager()->Select(item, ImGui::GetIO().KeyShift);
					}
				}

				ImGui::TreePop();
			}
		}
	}
	else
	{
		std::string selectedChildName = "";
		if (selectedChild == 2)
			selectedChildName = "NW";
		else if (selectedChild == 3)
			selectedChildName = "NE";
		else if (selectedChild == 0)
			selectedChildName = "SW";
		else if (selectedChild == 1)
			selectedChildName = "SE";

		path = std::format("{}->{}", path, selectedChildName);

		if (!children[selectedChild]->RenderUI(path, 
			drawFullPath || (recursiveDraw && drawBounds), 
			drawDataRec || (recursiveDraw && drawData), 
			depth + 1))
		{
			selectedChild = -1;
		}
	}

	return true;
}
#endif


bool Quadtree::Initialize(const dx::BoundingBox &sceneBounds, UINT maxDepth, UINT maxItemsInNode)
{
	if (maxDepth != CONTENT_NULL)
	{
		if (maxDepth == 0)
			return false;

		_maxDepth = maxDepth;
	}

	if (maxItemsInNode != CONTENT_NULL)
	{
		if (maxItemsInNode == 0)
			return false;

		_maxItemsInNode = maxItemsInNode;
	}

	_root = std::make_unique<Node>(_maxDepth, _maxItemsInNode);
	_root->bounds = sceneBounds;

	return true;
}

void Quadtree::Insert(Entity *data, const dx::BoundingOrientedBox &bounds) const
{
	ZoneScopedC(RandomUniqueColor());
	std::string name = std::format("{}:{}", data->GetName(), data->GetID());
#ifdef TRACY_DETAILED
	name += std::format(" [C:({}, {}, {}), E:({}, {}, {})), O:({}, {}, {}, {}]",
		bounds.Center.x, bounds.Center.y, bounds.Center.z,
		bounds.Extents.x, bounds.Extents.y, bounds.Extents.z,
		bounds.Orientation.x, bounds.Orientation.y, bounds.Orientation.z, bounds.Orientation.w
	);
#endif
	ZoneText(name.c_str(), name.size());

	data->UpdateCullingBounds();

	auto &treePath = data->GetCullingPlacement().quadTreePath;
	treePath.steps.clear();

	if (_root != nullptr)
		_root->Insert(data, bounds, &treePath);
}

bool Quadtree::Remove(Entity *data, bool skipIntersectionTests) const
{
	// TODO: This could be optimized by tracking how many leaf nodes an entity has been inserted into (stored in Entity)
	// These values would be updated by the leaf nodes upon insertion/removal.
	// After this is done, removal could be exited early as soon as the entity being removed counts 0 containing nodes.

	ZoneScopedC(RandomUniqueColor());
	std::string name = std::format("{}:{}", data->GetName(), data->GetID());
	ZoneText(name.c_str(), name.size());

	if (_root == nullptr)
		return false;

	Culling::CullingPlacement &placement = data->GetCullingPlacement();

	_root->Remove(data, placement.quadTreePath, 0);

	placement.quadTreePath.steps.clear();

	return true;
}

void Quadtree::RecalculateCullingBounds()
{
	if (_root != nullptr)
		_root->RecalculateCullingBounds();
}

dx::BoundingBox *Quadtree::GetBounds() const
{
	if (_root == nullptr)
		return nullptr;

	return &_root->bounds;
}

UINT Quadtree::GetMaxDepth() const
{
	return _maxDepth;
}

UINT Quadtree::GetMaxItemsInNode() const
{
	return _maxItemsInNode;
}

bool Quadtree::FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems, bool mustFullyContain) const
{
	if (_root == nullptr)
		return false;

	_root->FrustumCull(frustum, containingItems, mustFullyContain);
	return true;
}

bool Quadtree::BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain) const
{
	if (_root == nullptr)
		return false;

	_root->BoxCull(box, containingItems, mustFullyContain);
	return true;
}

bool Quadtree::BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain) const
{
	if (_root == nullptr)
		return false;

	_root->BoxCull(box, containingItems, mustFullyContain);
	return true;
}

bool Quadtree::RaycastTree(const dx::XMFLOAT3A &orig, const dx::XMFLOAT3A &dir, float &length, Entity *&entity, bool cheap) const
{
	if (_root == nullptr)
		return false;

	float len = FLT_MAX; // In case Intersects() uses the initial dist value as a maximum. Docs don't specify.
	if (!Raycast(orig, dir, _root->bounds, len))
		return false;

	length = FLT_MAX;
	entity = nullptr;

	return _root->RaycastNode(orig, dir, length, entity, cheap);
}

bool Quadtree::RaycastTree(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const
{
	if (_root == nullptr)
		return false;

	hit.length = ray.length > 0.0f ? ray.length : FLT_MAX;
	ent = nullptr;

	float len = hit.length; // In case Intersects() uses the initial dist value as a maximum. Docs don't specify.
	if (!Raycast(ray.origin, ray.direction, _root->bounds, len))
		return false;

	if (len > hit.length)
		return false;

	return _root->RaycastNode(ray, hit, ent);
}

#ifdef USE_IMGUI
void Quadtree::DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const
{
	if (_root == nullptr)
		return;

	_root->DebugGetStructure(boxCollection, full, culling);
}

void Quadtree::DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const
{
	if (_root == nullptr)
		return;

	_root->DebugGetStructure(boxCollection, frustum, full, culling);
}

bool Quadtree::RenderUI()
{
	if (!_root)
		return true;

	ImGui::Checkbox("Draw full path", &drawFullPath);

	ImGui::PushID("QuadtreeUI");
	std::string path = "";
	_root->RenderUI(path, drawFullPath);
	ImGui::PopID();

	return true;
}
#endif
