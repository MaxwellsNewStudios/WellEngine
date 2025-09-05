#pragma once

#include "Entity.h"
#include "Collision/Raycast.h"
#include "Debug/DebugNew.h"

#define QUADTREE_CULLING
//#define OCTREE_CULLING

#ifdef QUADTREE_CULLING
#include "Rendering/Culling/Quadtree.h"
#elif defined OCTREE_CULLING
#include "Rendering/Culling/Octree.h"
#endif

namespace SceneContents
{
	struct SceneEntity;

	struct HashedEntity
	{
		Ref<Entity> entRef = nullptr;
		UINT age = 0;
		bool found = false;

		HashedEntity() = default;
		HashedEntity(Entity *ent) : found(ent != nullptr), age(0)
		{
			entRef.Set(ent);
		}
	};

	class SceneIterator
	{
	private:
		std::vector<SceneContents::SceneEntity *> &_entities;
		std::vector<SceneContents::SceneEntity *>::iterator _begin, _end, _current;

	public:
		SceneIterator(std::vector<SceneContents::SceneEntity *> &entities)
			: _entities(entities), _begin(_entities.begin()), _end(_entities.end()), _current(_entities.begin()) {}
		SceneIterator(const SceneIterator &other) = default;
		SceneIterator &operator=(const SceneIterator &other) = default;
		SceneIterator(SceneIterator &&other) = default;
		SceneIterator &operator=(SceneIterator &&other) = default;

		SceneIterator &operator++()
		{
			if (_current == _end)
			{
				Warn("SceneIterator: Attempted to increment past end of iterator!");
				return *this;
			}
			_current++;
			return *this;
		}
		SceneIterator &operator--() 
		{
			if (_current == _begin)
			{
				Warn("SceneIterator: Attempted to decrement before start of iterator!");
				return *this;
			}
			_current--;
			return *this;
		}
		operator Entity *() const { return Peek(); }
		Entity *operator[](UINT index) const;

		size_t size() const { return _entities.size(); }

		void ToBegin() { _current = _begin; }
		void ToEnd() { _current = _end; }

		[[nodiscard]] size_t GetIndex() const 
		{
			if (_current == _end)
				return -1;
			return std::distance(_begin, _current);
		}
		void SetIndex(size_t index) 
		{
			if (index >= _entities.size())
				return;
			_current = _begin + index;
		}

		Entity *Step(bool skipInvalid = true, bool skipDisabled = false);
		[[nodiscard]] Entity *Peek() const;

		TESTABLE()
	};
}

class SceneHolder
{
private:
	UINT _entityCounter = 0;

	dx::BoundingBox _bounds;
	std::vector<SceneContents::SceneEntity *> _entities; 
	bool _recalculateColliders = false;

#ifdef QUADTREE_CULLING
	Quadtree _volumeTree;
#elif defined OCTREE_CULLING
	Octree _volumeTree;
#endif

	std::vector<Ref<Entity>> _treeInsertionQueue;
	std::vector<Ref<Entity>> _entityRemovalQueue;
	std::vector<std::pair<Ref<Entity>, UINT>> _entityReorderQueue;

	std::map<const UINT, SceneContents::HashedEntity> _entIDSearchHash;
	std::map<const std::string, SceneContents::HashedEntity> _entNameSearchHash;

public:
	enum BoundsType {
		Frustum		= 0,
		OrientedBox = 1
	};

	SceneHolder() = default;
	~SceneHolder();
	SceneHolder(const SceneHolder &other) = delete;
	SceneHolder &operator=(const SceneHolder &other) = delete;
	SceneHolder(SceneHolder &&other) = delete;
	SceneHolder &operator=(SceneHolder &&other) = delete;

	[[nodiscard]] bool Initialize(const dx::BoundingBox &sceneBounds);
	[[nodiscard]] bool Update();

	// Entity is Not initialized automatically. Initialize manually through the returned pointer.
	[[nodiscard]] Entity *AddEntity(const dx::BoundingOrientedBox &bounds, bool addToTree);

	// Expensive. Please consider if the entity actually needs to be removed immediately.
	[[nodiscard]] bool RemoveEntityImmediate(Entity *entity);

	bool RemoveEntity(Entity *entity);
	bool RemoveEntity(UINT index);
	bool RemoveEntityByID(UINT id);

	[[nodiscard]] bool IncludeEntityInTree(Entity *entity);
	[[nodiscard]] bool IncludeEntityInTree(UINT index);
	[[nodiscard]] bool ExcludeEntityFromTree(Entity *entity);
	[[nodiscard]] bool ExcludeEntityFromTree(UINT index);

	[[nodiscard]] bool IsEntityIncludedInTree(const Entity *entity) const;
	[[nodiscard]] bool IsEntityIncludedInTree(UINT index) const;

	[[nodiscard]] bool UpdateEntityPosition(Entity *entity);

	[[nodiscard]] const dx::BoundingBox &GetBounds() const;

	[[nodiscard]] Entity *GetEntity(UINT i) const;
	[[nodiscard]] Entity *GetEntityByID(UINT id);
	[[nodiscard]] Entity *GetEntityByName(const std::string &name);
	[[nodiscard]] Entity *GetEntityByDeserializedID(UINT id) const;
	[[nodiscard]] SceneContents::SceneIterator GetEntities();

	void HashEntityName(Entity *ent);
	void RemoveNameFromHash(const std::string &name);
	void RemoveNameFromHashIfNull(const std::string &name);
	void ResetNameHash();

	void HashEntityID(Entity *ent);
	void RemoveIDFromHash(UINT id);
	void RemoveIDFromHashIfNull(UINT id);
	void ResetIDHash();

	[[nodiscard]] UINT GetEntityIndex(const Entity *entity) const;
	[[nodiscard]] UINT GetEntityIndex(UINT id) const;
	[[nodiscard]] UINT GetEntityCount() const;

	void ReorderEntity(Entity *entity, UINT newIndex);
	void ReorderEntity(Entity *entity, const Entity *after);

	[[nodiscard]] bool FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems) const;
	[[nodiscard]] bool BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems) const;
	[[nodiscard]] bool BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems) const;

	bool RaycastScene(const dx::XMFLOAT3A &origin, const dx::XMFLOAT3A &direction, RaycastOut &result, bool cheap = true) const;
	bool RaycastScene(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const;

	void DebugGetTreeStructure(std::vector<dx::BoundingBox> &boxCollection, bool full = false, bool culling = false) const;
	void DebugGetTreeStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full = false, bool culling = false) const;

	bool GetRecalculateColliders() const;
	void SetRecalculateColliders();

	void ResetSceneHolder();

	void RecalculateTreeCullingBounds();

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI();
#endif

	TESTABLE()
};
