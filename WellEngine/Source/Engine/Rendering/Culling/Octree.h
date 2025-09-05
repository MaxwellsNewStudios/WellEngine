#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <DirectXCollision.h>
#include "Entity.h"
#include "Behaviour.h"
#include "Collision/Raycast.h"
#include "Behaviours/MeshBehaviour.h"

namespace dx = DirectX;

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

class Octree
{
private:
	static constexpr UINT MAX_ITEMS_IN_NODE = 16;
	static constexpr UINT MAX_DEPTH = 3;
	static constexpr UINT CHILD_COUNT = 8;

	struct Node
	{
		std::vector<Entity *> data;
		dx::BoundingBox bounds;
		std::unique_ptr<Node> children[CHILD_COUNT];
		bool isLeaf = true;


		void Split(const UINT depth)
		{
			const dx::XMFLOAT3
				center = bounds.Center,
				extents = bounds.Extents,
				min = { center.x - extents.x, center.y - extents.y, center.z - extents.z },
				max = { center.x + extents.x, center.y + extents.y, center.z + extents.z };

			for (int i = 0; i < CHILD_COUNT; i++)
				children[i] = std::make_unique<Node>();

			dx::BoundingBox::CreateFromPoints(children[0]->bounds, { min.x, min.y, min.z, 0 }, { center.x, center.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[1]->bounds, { center.x, min.y, min.z, 0 }, { max.x, center.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[2]->bounds, { min.x, min.y, center.z, 0 }, { center.x, center.y, max.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[3]->bounds, { center.x, min.y, center.z, 0 }, { max.x, center.y, max.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[4]->bounds, { min.x, center.y, min.z, 0 }, { center.x, max.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[5]->bounds, { center.x, center.y, min.z, 0 }, { max.x, max.y, center.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[6]->bounds, { min.x, center.y, center.z, 0 }, { center.x, max.y, max.z, 0 });
			dx::BoundingBox::CreateFromPoints(children[7]->bounds, { center.x, center.y, center.z, 0 }, { max.x, max.y, max.z, 0 });

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
			isLeaf = false;
		}


		bool Insert(Entity *item, const dx::BoundingOrientedBox &itemBounds, const UINT depth = 0)
		{
			if (!bounds.Intersects(itemBounds))
				return false;

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
				if (children[i] != nullptr)
					children[i]->Insert(item, itemBounds, depth + 1);
			}

			return true;
		}

		void Remove(Entity *item, const dx::BoundingOrientedBox &itemBounds, const UINT depth = 0, const bool skipIntersection = false)
		{
			if (!skipIntersection)
				if (!bounds.Intersects(itemBounds))
					return;

			if (isLeaf)
			{
				std::erase_if(data, [item](const Entity *otherItem) { return item == otherItem; });
				return;
			}

			for (int i = 0; i < CHILD_COUNT; i++)
			{
				if (children[i] != nullptr)
					children[i]->Remove(item, itemBounds, depth + 1, skipIntersection);
			}

			std::vector<Entity *> containingItems;
			containingItems.reserve(MAX_ITEMS_IN_NODE);
			for (int i = 0; i < CHILD_COUNT; i++)
			{
				if (children[i] != nullptr)
				{
					if (!children[i]->isLeaf)
						return;

					if (!children[i]->data.empty())
					{
						for (Entity *childItem : children[i]->data)
						{
							if (childItem == nullptr)
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
			}

			for (int i = 0; i < CHILD_COUNT; i++)
			{
				children[i].release();
				children[i] = nullptr;
			}

			isLeaf = true;
			data.clear();
			for (Entity *newItem : containingItems)
				data.emplace_back(newItem);
		}


		void AddToVector(std::vector<Entity *> &containingItems, const UINT depth) const
		{
			if (isLeaf)
			{
				for (Entity *item : data)
				{
					if (item == nullptr)
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
			switch (frustum.Contains(bounds))
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

						if (std::ranges::find(containingItems, item) == containingItems.end())
							containingItems.emplace_back(item);
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
			switch (box.Contains(bounds))
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

						if (std::ranges::find(containingItems, item) == containingItems.end())
							containingItems.emplace_back(item);
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
			switch (box.Contains(bounds))
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

						if (std::ranges::find(containingItems, item) == containingItems.end())
							containingItems.emplace_back(item);
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

		bool RaycastNode(const dx::XMFLOAT3 &orig, const dx::XMFLOAT3 &dir, float &length, Entity *&entity) const
		{
			if (isLeaf)
			{ // Check all items in leaf for intersection & return result.
				for (Entity *item : data)
				{
					if (item == nullptr)
						continue;

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

				return (entity != nullptr);
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
				while (childHits[j].length < childHits[j - 1ull].length)
				{
					std::swap(childHits[j], childHits[j - 1ull]);
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

				if (!children[childHits[i].index]->RaycastNode(orig, dir, length, entity))
				{
					length = FLT_MAX;
					entity = nullptr;
				}
			}

			return (entity != nullptr);
		}

		void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection) const
		{
			if (isLeaf)
			{
				boxCollection.emplace_back(bounds);
				return;
			}

			for (int i = 0; i < CHILD_COUNT; i++)
				children[i]->DebugGetStructure(boxCollection);
		}
	};

	std::unique_ptr<Node> _root;


public:
	Octree() = default;
	~Octree() = default;
	Octree(const Octree &other) = delete;
	Octree &operator=(const Octree &other) = delete;
	Octree(Octree &&other) = delete;
	Octree &operator=(Octree &&other) = delete;

	[[nodiscard]] bool Initialize(const dx::BoundingBox &sceneBounds)
	{
		_root = std::make_unique<Node>();
		_root->bounds = sceneBounds;

		return true;
	}

	void Insert(Entity *data, const dx::BoundingOrientedBox &bounds) const
	{
		if (_root != nullptr)
			_root->Insert(data, bounds);
	}


	[[nodiscard]] bool Remove(Entity *data, const dx::BoundingOrientedBox &bounds) const
	{
		if (_root == nullptr)
			return false;

		_root->Remove(data, bounds);
		return true;
	}

	[[nodiscard]] bool Remove(Entity *data) const
	{
		if (_root == nullptr)
			return false;

		dx::BoundingOrientedBox rootBounds;
		dx::BoundingOrientedBox().CreateFromBoundingBox(rootBounds, _root->bounds);

		_root->Remove(data, rootBounds, 0, true);
		return true;
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


	bool RaycastTree(const dx::XMFLOAT3A &orig, const dx::XMFLOAT3A &dir, float &length, Entity *&entity) const
	{
		if (_root == nullptr)
			return false;

		float len = FLT_MAX; // In case Intersects() uses the initial dist value as a maximum. Docs don't specify.
		if (!Raycast(orig, dir, _root->bounds, len))
			return false;

		length = FLT_MAX;
		entity = nullptr;

		return _root->RaycastNode(orig, dir, length, entity);
	}


	[[nodiscard]] dx::BoundingBox *GetBounds() const
	{
		if (_root == nullptr)
			return nullptr;

		return &_root->bounds;
	}


	void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection) const
	{
		if (_root == nullptr)
			return;

		_root->DebugGetStructure(boxCollection);
	}
};