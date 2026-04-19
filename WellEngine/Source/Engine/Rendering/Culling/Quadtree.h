#pragma once

#include <memory>
#include <utility>
#include <vector>
#include <DirectXCollision.h>

#include "CullingPlacement.h"

namespace WellEngine
{
	class Quadtree
	{
	private:
		static constexpr UINT CHILD_COUNT = 4;
		UINT _maxDepth = 5;
		UINT _maxItemsInNode = 16;

		struct Node
		{
			const UINT _maxDepth, _maxItemsInNode;

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

			Node(UINT maxDepth, UINT maxItemsInNode) : _maxDepth(maxDepth), _maxItemsInNode(maxItemsInNode) {}

			void Split(const UINT depth);

			UINT Insert(Entity *item, const dx::BoundingOrientedBox &itemBounds, Culling::TreePath *path, const UINT depth = 0);

			void Remove(Entity *item, const Culling::TreePath &path, const UINT depth = 0);


			void RecalculateCullingBounds();

			void AddToVector(std::vector<Entity *> &containingItems, const UINT depth) const;

			void FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems, bool mustFullyContain = false, const UINT depth = 0) const;

			void BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain = false, const UINT depth = 0) const;
		
			void BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain = false, const UINT depth = 0) const;

			bool RaycastNode(const dx::XMFLOAT3 &orig, const dx::XMFLOAT3 &dir, float &length, Entity *&entity, bool cheap) const;

			bool RaycastNode(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const;

	#ifdef USE_IMGUI
			void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const;
			void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const;

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

		[[nodiscard]] bool Initialize(const dx::BoundingBox &sceneBounds, UINT maxDepth = -1, UINT maxItemsInNode = -1);

		void Insert(Entity *data, const dx::BoundingOrientedBox &bounds) const;

		[[nodiscard]] bool Remove(Entity *data, bool skipIntersectionTests = false) const;

		void RecalculateCullingBounds();

		[[nodiscard]] dx::BoundingBox *GetBounds() const;
		[[nodiscard]] UINT GetMaxDepth() const;
		[[nodiscard]] UINT GetMaxItemsInNode() const;

		[[nodiscard]] bool FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems, bool mustFullyContain = false) const;

		[[nodiscard]] bool BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain = false) const;

		[[nodiscard]] bool BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems, bool mustFullyContain = false) const;

		bool RaycastTree(const dx::XMFLOAT3A &orig, const dx::XMFLOAT3A &dir, float &length, Entity *&entity, bool cheap = false) const;

		bool RaycastTree(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const;

	#ifdef USE_IMGUI
		bool drawFullPath = false;

		void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const;
		void DebugGetStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const;

		bool RenderUI();
	#endif

		TESTABLE
	};
}
