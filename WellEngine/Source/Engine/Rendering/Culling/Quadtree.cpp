#include "stdafx.h"
#include "Quadtree.h"
#include "Scenes/Scene.h"

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

				MeshBehaviour *meshBehaviour = nullptr;
				if (item->GetBehaviourByType<MeshBehaviour>(meshBehaviour))
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
			
			MeshBehaviour *meshBehaviour = nullptr;
			if (item->GetBehaviourByType<MeshBehaviour>(meshBehaviour))
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
#include "Scenes/Scene.h"
#include "Behaviours/DebugPlayerBehaviour.h"

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
						item->GetScene()->GetDebugPlayer()->Select(item, ImGui::GetIO().KeyShift);
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

