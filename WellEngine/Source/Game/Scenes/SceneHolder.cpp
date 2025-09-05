#include "stdafx.h"
#include "SceneHolder.h"
#include "Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

constexpr UINT EntityIDHashMaxSize = 64;
constexpr UINT EntityIDHashRemoveAge = 16;

constexpr UINT EntityNameHashMaxSize = 32;
constexpr UINT EntityNameHashRemoveAge = 32;

namespace SceneContents
{
	struct SceneEntity
	{
		bool includeInTree = true;
		Entity *entity = nullptr;

		explicit SceneEntity(const UINT id, const dx::BoundingOrientedBox &bounds, bool includeInTree)
		{
			this->includeInTree = includeInTree;
			entity = new Entity(id, bounds);
		}

		~SceneEntity()
		{
			if (entity)
			{
				delete entity;
				entity = nullptr;
			}
		}

		SceneEntity(const SceneEntity &other) = delete;
		SceneEntity &operator=(const SceneEntity &other) = delete;
		SceneEntity(SceneEntity &&other) = delete;
		SceneEntity &operator=(SceneEntity &&other) = delete;

		// Update the assignment to properly cast the SceneEntity pointer to an Entity pointer.
		[[nodiscard]] Entity *operator->() const { return entity; }
		[[nodiscard]] Entity &operator*() const { return *entity; }
		[[nodiscard]] operator Entity *() const { return entity; }


		inline Entity *GetEntity() const
		{
			return entity;
		}
	};
	

	Entity *SceneIterator::operator[](UINT index) const
	{
		if (index >= _entities.size())
			return nullptr;
		return _entities[index]->GetEntity();
	}
	Entity *SceneIterator::Step(bool skipInvalid, bool skipDisabled)
	{
		Entity *ent = nullptr;

		if (!skipInvalid)
		{
			if (_current == _end)
				return nullptr;

			ent = (*_current)->entity;
			++_current;

			return ent;
		}

		// Repeat until we find a valid entity or reach the end.
		while (true)
		{
			if (_current == _end)
				return nullptr;

			ent = (*_current)->entity;
			++_current;

			if (!ent)
				continue;

			if (ent->IsRemoved())
				continue;

			if (skipDisabled)
				if (!ent->IsEnabled())
					continue;

			break;
		}

		return ent;
	}
	[[nodiscard]] Entity *SceneIterator::Peek() const
	{
		if (_current == _end)
			return nullptr;
		return (*_current)->entity;
	}
}


SceneHolder::~SceneHolder()
{
	for (const SceneContents::SceneEntity *ent : _entities)
		delete ent;
}
bool SceneHolder::Initialize(const BoundingBox &sceneBounds)
{
	_bounds = sceneBounds;
	if (!_volumeTree.Initialize(sceneBounds))
	{
		ErrMsg("Failed to initialize volume tree!");
		return false;
	}

	return true;
}

bool SceneHolder::Update()
{
	ZoneScopedC(RandomUniqueColor());

	// Reorder Entities
	{
		ZoneNamedXNC(reorderEntitiesZone, "Reorder Entities", RandomUniqueColor(), true);

		for (auto &reorderPair : _entityReorderQueue)
		{
			ZoneNamedXNC(ReorderEntityZone, "Reorder Entity", RandomUniqueColor(), true);

			Entity *entity = reorderPair.first.Get();
			UINT newIndex = reorderPair.second;

			if (entity == nullptr)
				continue;

			if (_entities.size() < newIndex)
				continue;

			UINT currentIndex = GetEntityIndex(entity);
			if (currentIndex == CONTENT_NULL)
				continue;

			if (currentIndex == newIndex)
				continue;

			SceneContents::SceneEntity *ent = _entities[currentIndex];
			_entities.erase(_entities.begin() + currentIndex);
			_entities.insert(_entities.begin() + newIndex, ent);
		}
		_entityReorderQueue.clear();
	}

	// Update Hashes
	{
		ZoneNamedXNC(updateHashesZone, "Update Hashes", RandomUniqueColor(), true);

		for (auto it = _entNameSearchHash.begin(); it != _entNameSearchHash.end(); /*This space intentionally left blank*/ )
		{
			std::string_view name = it->first;
			SceneContents::HashedEntity &hashedEnt = it->second;

			ZoneNamedXNC(updateNameHashZone, "Update Name Hash", RandomUniqueColor(), true);
			ZoneTextXVF(updateNameHashZone, "name:\"%s\"  ref:%d  age:%d", name.data(), &hashedEnt.entRef, hashedEnt.age);

			if (hashedEnt.age >= EntityNameHashRemoveAge)
			{
				_entNameSearchHash.erase(it++);
				continue;
			}

			++it;
			hashedEnt.age++;
		}

		for (auto it = _entIDSearchHash.begin(); it != _entIDSearchHash.end(); /*This space intentionally left blank*/ )
		{
			UINT id = it->first;
			SceneContents::HashedEntity &hashedEnt = it->second;

			ZoneNamedXNC(updateIDHashZone, "Update ID Hash", RandomUniqueColor(), true);
			ZoneTextXVF(updateIDHashZone, "ID:%d  ref:%d  age:%d", id, &hashedEnt.entRef, hashedEnt.age);

			if (hashedEnt.age >= EntityIDHashRemoveAge)
			{
				_entIDSearchHash.erase(it++);
				continue;
			}

			++it;
			hashedEnt.age++;
		}
	}

	{
		ZoneNamedXNC(updateTreeInsertionZone, "Update Tree Insertion Queue", RandomUniqueColor(), true);

		for (Ref<Entity> &entRef : _treeInsertionQueue)
		{
			ZoneNamedXNC(insertEntZone, "Insert Entity", RandomUniqueColor(), true);

			Entity *entity;
			if (!entRef.TryGet(entity))
				continue;

			dx::BoundingOrientedBox entityBounds;
			entity->StoreEntityBounds(entityBounds);

			_volumeTree.Insert(entity, entityBounds);
		}
		_treeInsertionQueue.clear();
	}

	{
		ZoneNamedXNC(updateEntRemovalZone, "Update Entity Removal Queue", RandomUniqueColor(), true);

		for (Ref<Entity> &entRef : _entityRemovalQueue)
		{
			ZoneNamedXNC(removeEntZone, "Remove Entity", RandomUniqueColor(), true);

			Entity *entity;
			if (!entRef.TryGet(entity))
				continue;

			if (!RemoveEntityImmediate(entity))
			{
				ErrMsg("Failed to flush removal of entity!");
				return false;
			}
		}
		_entityRemovalQueue.clear();
	}

	_recalculateColliders = false;
	return true;
}

Entity *SceneHolder::AddEntity(const BoundingOrientedBox &bounds, bool addToTree)
{
	ZoneScopedC(RandomUniqueColor());

	if (_entities.size() == 0)
		_entityCounter = 0;

	SceneContents::SceneEntity *newEntity = new SceneContents::SceneEntity(_entityCounter, bounds, addToTree);
	_entities.emplace_back(newEntity);
	_entityCounter++;
	_recalculateColliders = true;

	if (addToTree)
		_treeInsertionQueue.emplace_back(newEntity->GetEntity()->AsRef());

	return _entities.back()->GetEntity();
}

bool SceneHolder::RemoveEntityImmediate(Entity *entity)
{
	ZoneScopedC(RandomUniqueColor());

	if (entity == nullptr)
		return false;

	entity->MarkAsRemoved();

	for (auto &child : *entity->GetChildren())
	{
		if (!RemoveEntityImmediate(child))
		{
			ErrMsg("Failed to remove child entity!");
			return false;
		}
	}

	if (!_volumeTree.Remove(entity))
	{
		delete entity;
		ErrMsg("Failed to remove entity from volume tree!");
		return false;
	}

	for (int i = 0; i < _entities.size(); i++)
	{
		Entity *ent = _entities[i]->GetEntity();
		if ((ent == entity) || (ent->GetID() == entity->GetID()))
		{
			delete _entities[i];
			_entities.erase(_entities.begin() + i);
			_recalculateColliders = true;
			break;
		}
	}

	return true;
}

bool SceneHolder::RemoveEntity(Entity *entity)
{
	ZoneScopedXC(RandomUniqueColor());

	if (entity == nullptr)
		return false;

	if (entity->IsRemoved())
		return true;

	size_t currentQueueSize = _entityRemovalQueue.size();
	for (size_t i = 0; i < currentQueueSize; i++)
	{
		Ref<Entity> &queuedEntityRef = _entityRemovalQueue[i];
		Entity *queuedEnt = queuedEntityRef.Get();

		if (entity == queuedEnt)
			return true;

		if (entity->IsChildOf(queuedEnt))
			return true;

		if (entity->IsParentOf(queuedEnt))
		{
			_entityRemovalQueue.erase(_entityRemovalQueue.begin() + i);
			currentQueueSize--;
			i--;
			continue;
		}
	}

	_entityRemovalQueue.emplace_back(*entity);
	entity->MarkAsRemoved();
	_recalculateColliders = true;
	return true;
}
bool SceneHolder::RemoveEntity(const UINT index)
{
	if (index >= _entities.size())
	{
		ErrMsgF("Failed to remove entity, ID:{} out of range!", index);
		return false;
	}

	_recalculateColliders = true;
	return RemoveEntity(_entities[index]->GetEntity());
}
bool SceneHolder::RemoveEntityByID(const UINT id)
{
	UINT index = GetEntityIndex(id);
	if (index == CONTENT_NULL)
		return false;

	return RemoveEntity(index);
}

bool SceneHolder::IncludeEntityInTree(Entity *entity)
{
	if (!entity)
	{
		ErrMsg("Failed to include entity in tree, entity is null!");
		return false;
	}

	return IncludeEntityInTree(GetEntityIndex(entity));
}
bool SceneHolder::IncludeEntityInTree(UINT index)
{
	if (index >= _entities.size())
	{
		ErrMsgF("Failed to include entity in tree, ID:{} out of range!", index);
		return false;
	}

	SceneContents::SceneEntity *sceneEntity = _entities[index];

	if (sceneEntity->includeInTree)
		return true; // Already included

	Entity *entity = sceneEntity->GetEntity();

	auto insertionQueueIt = std::find(_treeInsertionQueue.begin(), _treeInsertionQueue.end(), entity);
	while (insertionQueueIt != _treeInsertionQueue.end())
	{
		_treeInsertionQueue.erase(insertionQueueIt);
		insertionQueueIt = std::find(_treeInsertionQueue.begin(), _treeInsertionQueue.end(), entity);
	}

	BoundingOrientedBox entityBounds;
	entity->StoreEntityBounds(entityBounds);

	_volumeTree.Insert(entity, entityBounds);
	sceneEntity->includeInTree = true;

	return true;
}

bool SceneHolder::ExcludeEntityFromTree(Entity *entity)
{
	if (!entity)
	{
		ErrMsg("Failed to exclude entity from tree, entity is null!");
		return false;
	}

	return ExcludeEntityFromTree(GetEntityIndex(entity));
}
bool SceneHolder::ExcludeEntityFromTree(UINT index)
{
	if (index >= _entities.size())
	{
		ErrMsgF("Failed to exclude entity from tree, ID:{} out of range!", index);
		return false;
	}

	SceneContents::SceneEntity *sceneEntity = _entities[index];

	if (!sceneEntity->includeInTree)
		return true; // Already excluded

	Entity *entity = sceneEntity->GetEntity();

	auto insertionQueueIt = std::find(_treeInsertionQueue.begin(), _treeInsertionQueue.end(), entity);
	while (insertionQueueIt != _treeInsertionQueue.end())
	{
		_treeInsertionQueue.erase(insertionQueueIt);
		insertionQueueIt = std::find(_treeInsertionQueue.begin(), _treeInsertionQueue.end(), entity);
	}

	if (!_volumeTree.Remove(entity))
	{
		ErrMsg("Failed to remove entity from volume tree!");
		return false;
	}

	sceneEntity->includeInTree = false;

	return true;
}

bool SceneHolder::IsEntityIncludedInTree(const Entity *entity) const
{
	if (!entity)
		return false;

	return IsEntityIncludedInTree(GetEntityIndex(entity));
}
bool SceneHolder::IsEntityIncludedInTree(UINT index) const
{
	if (index >= _entities.size())
		return false;

	SceneContents::SceneEntity *sceneEntity = _entities[index];
	return sceneEntity->includeInTree;
}

bool SceneHolder::UpdateEntityPosition(Entity *entity)
{
	// Skip if entity is already in tree insertion queue
	if (std::find(_treeInsertionQueue.begin(), _treeInsertionQueue.end(), entity) != _treeInsertionQueue.end())
		return true;

	// HACK: Possibly more expensive than neccesary 
	for (auto &child : *entity->GetChildren())
	{
		if (!UpdateEntityPosition(child))
		{
			ErrMsg("Failed to update child entity position!");
			return false;
		}
	}

	UINT entityIndex = GetEntityIndex(entity);
	if (entityIndex >= 0)
	{
		if (_entities[entityIndex]->includeInTree)
		{
			dx::BoundingOrientedBox entityBounds;
			entity->StoreEntityBounds(entityBounds);

			if (!_volumeTree.Remove(entity))
			{
				ErrMsg("Failed to remove entity from volume tree!");
				return false;
			}

			_volumeTree.Insert(entity, entityBounds);
		}
	}

	entity->GetTransform()->CleanScenePos();
	return true;
}

const dx::BoundingBox &SceneHolder::GetBounds() const
{
	return _bounds;
}

Entity *SceneHolder::GetEntity(const UINT i) const
{
	if (i < 0)
		return nullptr;

	if (i >= _entities.size())
		return nullptr;

	return _entities[i]->GetEntity();
}
Entity *SceneHolder::GetEntityByID(const UINT id)
{
	ZoneScopedXC(RandomUniqueColor());

	// Try to find in hash first
	if (auto hashIter = _entIDSearchHash.find(id); hashIter != _entIDSearchHash.end())
	{
		if (!hashIter->second.found)
		{
			// Hash says entity is not found
			hashIter->second.age = 0;
			return nullptr;
		}

		Entity *ent = nullptr;
		if (hashIter->second.entRef.TryGet(ent))
			hashIter->second.age = 0;

		return ent;
	}

	// Not found in hash, do a binary search
	auto entIter = std::lower_bound(
		_entities.begin(), _entities.end(), &id,
		[](const void *entity, const void *id) {
			return ((const SceneContents::SceneEntity *)entity)->entity->GetID() < *((const UINT *)id);
		}
	);

	Entity *ent = nullptr;
	if (entIter != _entities.end() && (*entIter)->entity->GetID() == id)
		Entity *ent = (*entIter)->entity;

	// If not found in binary search, do a linear search as fallback
	if (!ent)
	{
		const UINT entityCount = GetEntityCount();
		for (UINT i = 0; i < entityCount; i++)
		{
			Entity *entAtI = _entities[i]->GetEntity();

			if (!entAtI)
				continue;

			if (entAtI->IsRemoved())
				continue;

			if (entAtI->GetID() != id)
				continue;
			
			ent = entAtI;
			break;
		}
	}

	// If found, add to hash to speed up future searches
	if (ent)
	{
		if (_entIDSearchHash.size() < EntityIDHashMaxSize)
		{
			_entIDSearchHash.emplace(id, ent);
			auto &newHash = _entIDSearchHash[id];

			newHash.entRef.AddDestructCallback([this, id](Ref<Entity> *entRef) {
				// If this entity is destructed, mark the hash entry as not found
				auto hashIter = _entIDSearchHash.find(id);
				if (hashIter == _entIDSearchHash.end())
					return;

				hashIter->second.found = false;
				hashIter->second.entRef = nullptr;
			});
		}

		return ent;
	}

	// Not found, add to hash to avoid future lookups
	_entIDSearchHash.emplace(id, nullptr);
	return nullptr;
}
Entity *SceneHolder::GetEntityByName(const std::string &name)
{
	ZoneScopedXC(RandomUniqueColor());

	// Try to find in hash first
	if (auto hashIter = _entNameSearchHash.find(name); hashIter != _entNameSearchHash.end())
	{
		if (!hashIter->second.found)
		{
			// Hash says entity is not found
			hashIter->second.age = 0;
			return nullptr;
		}

		Entity *ent = nullptr;
		if (hashIter->second.entRef.TryGet(ent))
			hashIter->second.age = 0;

		return ent;
	}

	// Not found in hash, search all entities for name
	const UINT entityCount = GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		Entity *ent = _entities[i]->GetEntity();

		if (!ent)
			continue;

		if (ent->IsRemoved())
			continue;

		if (ent->GetName() == name)
		{
			if (_entNameSearchHash.size() < EntityNameHashMaxSize)
			{
				_entNameSearchHash.emplace(name, ent);
				auto &newHash = _entNameSearchHash[name];

				newHash.entRef.AddDestructCallback([this, name](Ref<Entity> *entRef) {
					// If this entity is destructed, mark the hash entry as not found
					auto hashIter = _entNameSearchHash.find(name);
					if (hashIter == _entNameSearchHash.end())
						return;

					hashIter->second.found = false;
					hashIter->second.entRef = nullptr;
				});
			}

			return ent;
		}
	}

	// Not found, add to hash to avoid future lookups
	_entNameSearchHash.emplace(name, nullptr);
	return nullptr;
}
Entity *SceneHolder::GetEntityByDeserializedID(UINT id) const
{
	ZoneScopedXC(RandomUniqueColor());

	const UINT entityCount = GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		Entity *ent = _entities[i]->GetEntity();

		if (!ent)
			continue;

		if (ent->IsRemoved())
			continue;

		if (ent->GetDeserializedID() == id)
			return ent;
	}

	return nullptr;
}
SceneContents::SceneIterator SceneHolder::GetEntities()
{
	ZoneScopedXC(RandomUniqueColor());

	SceneContents::SceneIterator it(_entities);
	return it;
}

void SceneHolder::HashEntityName(Entity *ent)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!ent)
		return;

	if (_entNameSearchHash.find(ent->GetName()) != _entNameSearchHash.end())
		return;

	if (_entNameSearchHash.size() < EntityNameHashMaxSize)
		_entNameSearchHash.emplace(ent->GetName(), ent);
}
void SceneHolder::RemoveNameFromHash(const std::string &name)
{
	ZoneScopedXC(RandomUniqueColor());

	if (_entNameSearchHash.find(name) == _entNameSearchHash.end())
		return;

	_entNameSearchHash.erase(name);
}
void SceneHolder::RemoveNameFromHashIfNull(const std::string &name)
{
	ZoneScopedXC(RandomUniqueColor());

	auto hashIter = _entNameSearchHash.find(name);
	if (hashIter == _entNameSearchHash.end())
		return;

	if (hashIter->second.found)
		return;

	_entNameSearchHash.erase(hashIter);
}
void SceneHolder::ResetNameHash()
{
	ZoneScopedXC(RandomUniqueColor());

	_entNameSearchHash.clear();
}

void SceneHolder::HashEntityID(Entity *ent)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!ent)
		return;

	for (UINT i = 0; i < (UINT)(_entIDSearchHash.size()); i++)
	{
		if (_entIDSearchHash[i].entRef == ent)
			return;
	}

	if (_entIDSearchHash.size() < EntityIDHashMaxSize)
		_entIDSearchHash.emplace(ent->GetID(), ent);
}
void SceneHolder::RemoveIDFromHash(UINT id)
{
	ZoneScopedXC(RandomUniqueColor());

	if (_entIDSearchHash.find(id) == _entIDSearchHash.end())
		return;

	_entIDSearchHash.erase(id);
}
void SceneHolder::RemoveIDFromHashIfNull(UINT id)
{
	ZoneScopedXC(RandomUniqueColor());

	auto hashIter = _entIDSearchHash.find(id);
	if (hashIter == _entIDSearchHash.end())
		return;

	if (hashIter->second.found)
		return;

	_entIDSearchHash.erase(hashIter);
}
void SceneHolder::ResetIDHash()
{
	ZoneScopedXC(RandomUniqueColor());

	_entIDSearchHash.clear();
}

UINT SceneHolder::GetEntityIndex(const Entity *entity) const
{
	ZoneScopedXC(RandomUniqueColor());

	if (!entity)
		return -1;

	const UINT entityCount = GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		if (entity == _entities[i]->GetEntity())
			return i;
	}

	return -1;
}
UINT SceneHolder::GetEntityIndex(UINT id) const
{
	ZoneScopedXC(RandomUniqueColor());

	const UINT entityCount = GetEntityCount();
	for (UINT i = 0; i < entityCount; i++)
	{
		if (id == _entities[i]->GetEntity()->GetID())
			return i;
	}

	return -1;
}
UINT SceneHolder::GetEntityCount() const
{
	return static_cast<UINT>(_entities.size());
}

void SceneHolder::ReorderEntity(Entity *entity, UINT newIndex)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!entity)
		return;

	_entityReorderQueue.emplace_back(*entity, newIndex);
}
void SceneHolder::ReorderEntity(Entity *entity, const Entity *after)
{
	ZoneScopedXC(RandomUniqueColor());

	if (entity == after)
	{
		DbgMsg("Cannot reorder entity, entity and after entity are the same!");
		return;
	}

	UINT currIndex = GetEntityIndex(entity);
	if (currIndex == CONTENT_NULL)
	{
		DbgMsg("Cannot reorder entity, entity not found in scene!");
		return;
	}

	UINT newIndex = GetEntityIndex(after);
	if (newIndex == CONTENT_NULL)
	{
		DbgMsg("Cannot reorder entity, destination not found in scene!");
		return;
	}

	if (currIndex > newIndex)
		newIndex++;

	_entityReorderQueue.emplace_back(*entity, newIndex);
}

bool SceneHolder::FrustumCull(const dx::BoundingFrustum &frustum, std::vector<Entity *> &containingItems) const
{
	ZoneScopedC(RandomUniqueColor());

	std::vector<Entity *> containingInterfaces;
	containingInterfaces.reserve(_entities.capacity());

	if (!_volumeTree.FrustumCull(frustum, containingInterfaces))
	{
		ErrMsg("Failed to frustum cull volume tree!");
		return false;
	}

	for (Entity *iEnt : containingInterfaces)
		containingItems.emplace_back(iEnt);

	return true;
}
bool SceneHolder::BoxCull(const dx::BoundingOrientedBox &box, std::vector<Entity *> &containingItems) const
{
	ZoneScopedC(RandomUniqueColor());

	std::vector<Entity *> containingInterfaces;
	containingInterfaces.reserve(_entities.capacity());

	if (!_volumeTree.BoxCull(box, containingInterfaces))
	{
		ErrMsg("Failed to box cull volume tree!");
		return false;
	}

	for (Entity *iEnt : containingInterfaces)
		containingItems.emplace_back(iEnt);

	return true;
}
bool SceneHolder::BoxCull(const dx::BoundingBox &box, std::vector<Entity *> &containingItems) const
{
	ZoneScopedC(RandomUniqueColor());

	std::vector<Entity *> containingInterfaces;
	containingInterfaces.reserve(_entities.capacity());

	if (!_volumeTree.BoxCull(box, containingInterfaces))
	{
		ErrMsg("Failed to box cull volume tree!");
		return false;
	}

	for (Entity *iEnt : containingInterfaces)
		containingItems.emplace_back(iEnt);

	return true;
}
bool SceneHolder::RaycastScene(const dx::XMFLOAT3A &origin, const dx::XMFLOAT3A &direction, RaycastOut &result, bool cheap) const
{
	ZoneScopedC(RandomUniqueColor());

	return _volumeTree.RaycastTree(origin, direction, result.distance, result.entity, cheap);
}

bool SceneHolder::RaycastScene(const Shape::Ray &ray, Shape::RayHit &hit, Entity *&ent) const
{
	ZoneScopedC(RandomUniqueColor());

	return _volumeTree.RaycastTree(ray, hit, ent);
}

void SceneHolder::DebugGetTreeStructure(std::vector<dx::BoundingBox> &boxCollection, bool full, bool culling) const
{
	_volumeTree.DebugGetStructure(boxCollection, full, culling);
}
void SceneHolder::DebugGetTreeStructure(std::vector<dx::BoundingBox> &boxCollection, const dx::BoundingFrustum &frustum, bool full, bool culling) const
{
	_volumeTree.DebugGetStructure(boxCollection, frustum, full, culling);
}

void SceneHolder::ResetSceneHolder()
{
	ZoneScopedXC(RandomUniqueColor());

	for (const SceneContents::SceneEntity *ent : _entities)
		delete ent;

	_entityCounter = 0;

	_bounds = {};
	_entities = {};
	_recalculateColliders = false;

#ifdef QUADTREE_CULLING
	_volumeTree = {};
#elif defined OCTREE_CULLING
	Octree _volumeTree;
#endif

	_treeInsertionQueue = {};
	_entityRemovalQueue = {};
}

void SceneHolder::SetRecalculateColliders()
{
	_recalculateColliders = true;
}
bool SceneHolder::GetRecalculateColliders() const
{
	return _recalculateColliders;
}

void SceneHolder::RecalculateTreeCullingBounds()
{
	ZoneScopedC(RandomUniqueColor());

	_volumeTree.RecalculateCullingBounds();
}

#ifdef USE_IMGUI
bool SceneHolder::RenderUI()
{
	// TODO: Set bounds

	ImGui::Separator();

	ImGui::Text("Scene Entities: %d", GetEntityCount());

	ImGuiChildFlags hashChildFlags = ImGuiChildFlags_None;
	hashChildFlags |= ImGuiChildFlags_Borders;
	hashChildFlags |= ImGuiChildFlags_ResizeY;

	ImGuiWindowFlags hashWindowFlags = ImGuiWindowFlags_None;
	
	ImGui::Text("Name Hash Size: %d / %d", _entNameSearchHash.size(), EntityNameHashMaxSize);
	if (ImGui::TreeNode("Hashed Names"))
	{
		ImGui::BeginChild("NameHashChild", { ImGui::GetContentRegionAvail().x, 100 }, hashChildFlags, hashWindowFlags );
		for (auto it = _entNameSearchHash.begin(); it != _entNameSearchHash.end(); it++)
		{
			std::string_view name = it->first;
			SceneContents::HashedEntity &hashedEnt = it->second;

			if (ImGui::Button(std::format("\"{}\": {} (age:{})", name.data(), hashedEnt.found ? "Found" : "Not Found", hashedEnt.age).c_str()))
			{
				if (Entity *ent = hashedEnt.entRef.Get())
				{
					Scene *scene = ent->GetScene();
					scene->SetSelection(ent, ImGui::GetIO().KeyShift);
				}
			}
		}
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::Text("ID Hash Size: %d / %d", _entIDSearchHash.size(), EntityIDHashMaxSize);
	if (ImGui::TreeNode("Hashed IDs"))
	{
		ImGui::BeginChild("IDHashChild", { ImGui::GetContentRegionAvail().x, 100 }, hashChildFlags, hashWindowFlags);
		for (auto it = _entIDSearchHash.begin(); it != _entIDSearchHash.end(); it++)
		{
			UINT id = it->first;
			SceneContents::HashedEntity &hashedEnt = it->second;

			if (ImGui::Button(std::format("{}: {} (age:{})", id, hashedEnt.found ? "Found" : "Not Found", hashedEnt.age).c_str()))
			{
				if (Entity *ent = hashedEnt.entRef.Get())
				{
					Scene *scene = ent->GetScene();
					scene->SetSelection(ent, ImGui::GetIO().KeyShift);
				}
			}
		}
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::Separator();

	if (!_volumeTree.RenderUI())
	{
		ErrMsg("Failed to render quadtree UI!");
		return false;
	}

	return true;
}
#endif
