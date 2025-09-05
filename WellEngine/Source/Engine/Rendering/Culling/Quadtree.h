#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <DirectXCollision.h>
#include "NodePath.h"
#include "Entity.h"
#include "Behaviour.h"
#include "Collision/Raycast.h"
#include "Behaviours/MeshBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

class Quadtree
{
public:
	static constexpr UINT MAX_DEPTH = 5;

private:
	static constexpr UINT MAX_ITEMS_IN_NODE = 16;
	static constexpr UINT CHILD_COUNT = 4;

	struct Node
	{
		std::vector<Entity *> data;
		dx::BoundingBox bounds, cullingBounds;
		std::unique_ptr<Node> children[CHILD_COUNT];
		bool isLeaf = true, isDirty = true, isEmpty = true;

#ifdef USE_IMGUI
		bool drawBounds = false;
		bool recursiveDraw = false;
		bool drawData = false;
		int selectedChild = -1;
		dx::XMFLOAT4 boundsColor = { 1.0f, 0.0f, 0.0f, 0.05f };
		dx::XMFLOAT4 dataColor = { 0.0f, 0.0f, 1.0f, 0.2f };
#endif

		void Split(const UINT depth)
		{
			ZoneScopedXC(RandomUniqueColor());

			const dx::XMFLOAT3
				center = bounds.Center,
				extents = bounds.Extents,
				min = { center.x - extents.x, center.y - extents.y, center.z - extents.z },
				max = { center.x + extents.x, center.y + extents.y, center.z + extents.z };

			children[0] = std::make_unique<Node>();
			children[1] = std::make_unique<Node>();
			children[2] = std::make_unique<Node>();
			children[3] = std::make_unique<Node>();

			dx::BoundingBox::CreateFromPoints(children[0]->bounds, { min.x, min.y, min.z, 0 }, { center.x, max.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[1]->bounds, { center.x, min.y, min.z, 0 }, { max.x, max.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[2]->bounds, { min.x, min.y, center.z, 0 }, { center.x, max.y, max.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[3]->bounds, { center.x, min.y, center.z, 0 }, { max.x, max.y, max.z, 0 });

			for (int i = 0; i < data.size(); i++)
			{
				if (data[i] != nullptr)
				{
					bool hasBounds = false;
					dx::BoundingOrientedBox itemBounds;
					if (!data[i]->HasBounds(false, itemBounds))
					{
						dx::XMFLOAT3A pos = data[i]->GetTransform()->GetPosition();
						itemBounds.Center = { pos.x, pos.y, pos.z };
						itemBounds.Extents = { 0.1f, 0.1f, 0.1f };
					}

					for (int j = 0; j < CHILD_COUNT; j++)
						children[j]->Insert(data[i], itemBounds, depth + 1);
				}
			}

			data.clear();
			isEmpty = false;
			isLeaf = false;
			isDirty = true;
		}

		bool Insert(Entity *item, const dx::BoundingOrientedBox &itemBounds, const UINT depth = 0)
		{
			ZoneScopedXC(RandomUniqueColor());

			if (!bounds.Intersects(itemBounds))
				return false;

			isDirty = true;
			isEmpty = false;

			if (isLeaf)
			{
				if (depth >= MAX_DEPTH || data.size() < MAX_ITEMS_IN_NODE)
				{
					data.emplace_back(item);
					return true;
				}

				Split(depth);
			}

			for (int i = 0; i < CHILD_COUNT; i++)
			{
				if (children[i])
					children[i]->Insert(item, itemBounds, depth + 1);
			}

			return true;
		}

		void Remove(Entity *item, const dx::BoundingOrientedBox &itemBounds, const UINT depth = 0, const bool skipIntersection = false)
		{
			ZoneScopedXC(RandomUniqueColor());

			if (!skipIntersection)
				if (!bounds.Intersects(itemBounds))
					return;

			if (isLeaf)
			{
				size_t num = std::erase_if(data, [item](const Entity *otherItem) { return item == otherItem; });

				if (num > 0)
					isDirty = true;

				return;
			}

			isDirty = true;

			for (int i = 0; i < CHILD_COUNT; i++)
			{
				if (children[i])
					children[i]->Remove(item, itemBounds, depth + 1, skipIntersection);
			}

			std::vector<Entity *> containingItems;
			containingItems.reserve(MAX_ITEMS_IN_NODE);

			for (int i = 0; i < CHILD_COUNT; i++)
				if (children[i])
				{
					if (!children[i]->isLeaf)
						return;

					if (!children[i]->data.empty())
					{
						for (Entity *childItem : children[i]->data)
						{
							if (!childItem)
								continue;

							if (std::ranges::find(containingItems, childItem) == containingItems.end())
							{
								if (containingItems.size() >= MAX_ITEMS_IN_NODE)
									return;

								containingItems.emplace_back(childItem);
							}
						}
					}
				}

			for (int i = 0; i < CHILD_COUNT; i++)
				children[i] = nullptr;

			isLeaf = true;
			data.clear();
			for (Entity *newItem : containingItems)
				data.emplace_back(newItem);
		}


		void RecalculateCullingBounds()
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

		void AddToVector(std::vector<Entity *> &containingItems, const UINT depth) const
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

		void FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems, const UINT depth = 0) const
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
#ifdef EXTRA_CULL_CHECK
							dx::BoundingOrientedBox itemBounds;
							item->StoreEntityBounds(itemBounds);

							if (frustum.Intersects(itemBounds))
								containingItems.emplace_back(item);
#else
							containingItems.emplace_back(item);
#endif
						}
					}

					return;
				}

				for (int i = 0; i < CHILD_COUNT; i++)
				{
					if (children[i] == nullptr)
						continue;

					children[i]->FrustumCull(frustum, containingItems, depth + 1);
				}
				break;
			}
		}

		void BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems, const UINT depth = 0) const
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
#ifdef EXTRA_CULL_CHECK
							dx::BoundingOrientedBox itemBounds;
							item->StoreEntityBounds(itemBounds);

							if (box.Intersects(itemBounds))
								containingItems.emplace_back(item);
#else
							containingItems.emplace_back(item);
#endif
						}
					}

					return;
				}

				for (int i = 0; i < CHILD_COUNT; i++)
				{
					if (children[i] == nullptr)
						continue;

					children[i]->BoxCull(box, containingItems, depth + 1);
				}
				break;
			}
		}
		
		void BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems, const UINT depth = 0) const
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
#ifdef EXTRA_CULL_CHECK
							dx::BoundingOrientedBox itemBounds;
							item->StoreEntityBounds(itemBounds);

							if (box.Intersects(itemBounds))
								containingItems.emplace_back(item);
#else
							containingItems.emplace_back(item);
#endif
						}
					}

					return;
				}

				for (int i = 0; i < CHILD_COUNT; i++)
				{
					if (children[i] == nullptr)
						continue;

					children[i]->BoxCull(box, containingItems, depth + 1);
				}
				break;
			}
		}

		bool RaycastNode(const dx::XMFLOAT3 &orig, const dx::XMFLOAT3 &dir, float &length, Entity *&entity, bool cheap) const;
		bool RaycastNode(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const;

		void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const
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
		void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const
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

#ifdef USE_IMGUI
		bool RenderUI(std::string &path, bool drawFullPath, bool drawDataRec = false, const UINT depth = 0);
#endif
	};

	std::unique_ptr<Node> _root;

public:
	Quadtree() = default;
	~Quadtree() = default;
	Quadtree(const Quadtree &other) = default;
	Quadtree &operator=(const Quadtree &other) = default;
	Quadtree(Quadtree &&other) = default;
	Quadtree &operator=(Quadtree &&other) = default;

	[[nodiscard]] bool Initialize(const dx::BoundingBox &sceneBounds)
	{
		_root = std::make_unique<Node>();
		_root->bounds = sceneBounds;

		return true;
	}


	void Insert(Entity *data, const dx::BoundingOrientedBox &bounds) const
	{
		ZoneScopedC(RandomUniqueColor());
		const std::string &name = data->GetName();
		ZoneText(name.c_str(), name.size());

		data->UpdateCullingBounds();

		if (_root != nullptr)
			_root->Insert(data, bounds);
	}

	[[nodiscard]] bool Remove(Entity *data, bool skipIntersectionTests = false) const
	{
		// TODO: This could be optimized by tracking how many leaf nodes an entity has been inserted into (stored in Entity)
		// These values would be updated by the leaf nodes upon insertion/removal.
		// After this is done, removal could be exited early as soon as the entity being removed counts 0 containing nodes.

		ZoneScopedC(RandomUniqueColor());
		const std::string &name = data->GetName();
		ZoneText(name.c_str(), name.size());

		if (_root == nullptr)
			return false;

		_root->Remove(data, data->GetLastCullingBounds(), 0, skipIntersectionTests); // TODO: Is this enough?
		return true;
	}


	void RecalculateCullingBounds()
	{
		if (_root != nullptr)
			_root->RecalculateCullingBounds();
	}

	[[nodiscard]] dx::BoundingBox *GetBounds() const
	{
		if (_root == nullptr)
			return nullptr;

		return &_root->bounds;
	}


	[[nodiscard]] bool FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems) const
	{
		if (_root == nullptr)
			return false;

		_root->FrustumCull(frustum, containingItems);
		return true;
	}

	[[nodiscard]] bool BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems) const
	{
		if (_root == nullptr)
			return false;

		_root->BoxCull(box, containingItems);
		return true;
	}

	[[nodiscard]] bool BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems) const
	{
		if (_root == nullptr)
			return false;

		_root->BoxCull(box, containingItems);
		return true;
	}

	bool RaycastTree(const dx::XMFLOAT3A &orig, const dx::XMFLOAT3A &dir, float &length, Entity *&entity, bool cheap = false) const
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
	bool RaycastTree(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const
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


	void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const
	{
		if (_root == nullptr)
			return;

		_root->DebugGetStructure(boxCollection, full, culling);
	}
	void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const
	{
		if (_root == nullptr)
			return;

		_root->DebugGetStructure(boxCollection, frustum, full, culling);
	}


#ifdef USE_IMGUI
	bool drawFullPath = false;

	bool RenderUI()
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

	TESTABLE()
};