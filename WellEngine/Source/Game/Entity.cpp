#include "stdafx.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Behaviours/Debug/DebugPlayerBehaviour.h"
#include "Behaviours/Rendering/Mesh/MeshBehaviour.h"
#include "BehaviourFactory.h"
#include "Source/Engine/Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

Entity::~Entity()
{
	if (_entityID == -1)
		return;

#ifdef USE_IMGUI
	const std::string windowID = std::format("Ent#{}", GetID());

	// Check if entity is undocked
	if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
	{
		if (!ImGuiUtils::Utils::CloseWindow(windowID))
			ErrMsgF("Failed to close deleted entity window (ID:'{}')!", windowID);
	}
#endif

	_isRemoved = true;
	_behaviours.clear();

	for (auto& child : _children)
	{
		if (child != nullptr)
			child->SetParent(nullptr);
	}
	_children.clear();

	if (_parent)
		_parent->RemoveChild(this);
}

Entity &Entity::operator=(Entity &&other) noexcept  
{  
	_lastTransformedCullingBounds = std::move(other._lastTransformedCullingBounds);
	_recalculateCollider = other._recalculateCollider;  
	_isDebugSelectable = other._isDebugSelectable;  
	_transformedBounds = std::move(other._transformedBounds);
	_recalculateBounds = other._recalculateBounds;
	_inheritedDisabled = other._inheritedDisabled;  
	_cullingPlacement = std::move(other._cullingPlacement);
	_showInHierarchy = other._showInHierarchy;  
	_deserializedID = other._deserializedID;  
	_isInitialized = other._isInitialized;  
	_doSerialize = other._doSerialize;  
	_behaviours = std::move(other._behaviours);
	_isEnabled = other._isEnabled;  
	_transform = std::move(other._transform);  
	_isRemoved = other._isRemoved;  
	_doRender.store(other._doRender.load());
	_children = std::move(other._children);
	_entityID = other._entityID; other._entityID = -1;
	_isStatic = other._isStatic;  
	_parent = other._parent; other._parent = nullptr;
	_bounds = std::move(other._bounds);
	_scene = other._scene; other._scene = nullptr;
	_name = std::move(other._name);
	return *this;  
}
Entity::Entity(Entity &&other) noexcept
{
	*this = std::move(other); // Use move assignment operator
}

bool Entity::Initialize(ID3D11Device *device, Scene *scene, const std::string &name)
{
	if (_isInitialized)
	{
		ErrMsg("Entity is already initialized!");
		return false;
	}

	SetScene(scene);
	SetName(name);

	if (scene)
	{
		if (SceneHolder *sceneHolder = scene->GetSceneHolder())
		{
			sceneHolder->RemoveIDFromHashIfNull(_entityID); // Remove id from hash if it's hashed as not found
		}
	}

	if (!_transform.Initialize(device))
	{
		ErrMsg("Failed to initialize entity transform!");
		return false;
	}

	_transform.AddDirtyCallback(std::bind(&Entity::SetDirtyImmediate, this));

	_isInitialized = true;
	return true;
}
bool Entity::IsInitialized() const
{
	return _isInitialized;
}

void Entity::Destroy()
{
	if (!_isInitialized)
		return;

	if (_isRemoved)
		return;

	_scene->GetSceneHolder()->RemoveEntity(this);
}

void Entity::SetSerialization(bool state)
{
	_doSerialize = state;
}
bool Entity::IsSerializable() const
{
	return _doSerialize;
}

void Entity::AddBehaviour(Behaviour *behaviour)
{
	if (!behaviour)
	{
		ErrMsg("Behaviour must not be null!");
		return;
	}

	if (behaviour->IsInitialized())
	{
		ErrMsg("Behaviour must not be initialized before being added to an entity!");
		return;
	}

	_behaviours.emplace_back(behaviour);
}
void Entity::RemoveBehaviour(Behaviour *behaviour)
{
	if (!behaviour)
	{
		ErrMsg("Behaviour must not be null!");
		return;
	}

	std::unique_ptr<Behaviour> behPtr = nullptr;
	for (int i = 0; i < _behaviours.size(); i++)
	{
		Behaviour *beh = _behaviours[i].get();

		if (beh != behaviour)
			continue;

		behPtr = std::move(_behaviours[i]);
		_behaviours.erase(_behaviours.begin() + i);
		break;
	}

	behPtr = nullptr;
}
void Entity::ReorderBehaviour(Behaviour *behaviour, UINT newIndex)
{
	if (!behaviour)
	{
		ErrMsg("Behaviour must not be null!");
		return;
	}

	if (newIndex >= _behaviours.size())
	{
		ErrMsg("New index is out of bounds!");
		return;
	}

	UINT behIndex = GetBehaviourIndex(behaviour);
	if (behIndex == CONTENT_NULL)
	{
		ErrMsg("Behaviour not found in entity!");
		return;
	}

	if (behIndex == newIndex)
		return;

	if (behIndex < newIndex)
		newIndex--; // Account for the removal of the behaviour from its current position

	auto it = _behaviours.begin() + behIndex;
	std::unique_ptr<Behaviour> beh = std::move(*it);

	_behaviours.erase(it);
	_behaviours.insert(_behaviours.begin() + newIndex, std::move(beh));
}

Behaviour *Entity::GetBehaviour(UINT index) const
{
	if (index >= _behaviours.size())
		return nullptr;

	return _behaviours[index].get();
}
UINT Entity::GetBehaviourIndex(Behaviour *behaviour) const
{
	if (!behaviour)
		return CONTENT_NULL;

	for (int i = 0; i < _behaviours.size(); i++)
	{
		Behaviour *beh = _behaviours[i].get();
		if (beh != behaviour)
			continue;

		return i;
	}

	return CONTENT_NULL;
}
const std::vector<std::unique_ptr<Behaviour>> *Entity::GetBehaviours() const
{
	return &_behaviours;
}
UINT Entity::GetBehaviourCount() const
{
	return static_cast<UINT>(_behaviours.size());
}

bool Entity::IsEnabled() const
{
	return _inheritedDisabled <= 0 && _isEnabled;
}
bool Entity::IsEnabledSelf() const
{
	return _isEnabled;
}

void Entity::SetStatic(bool state)
{
	_isStatic = state;
}
bool Entity::IsStatic() const
{
	return _isStatic;
}

void Entity::SetDebugSelectable(bool state)
{
	_isDebugSelectable = state;
}
bool Entity::IsDebugSelectable() const
{
	return _isDebugSelectable;
}

void Entity::SetRaycastTarget(bool state)
{
	_skipInRaycast = !state;
}
bool Entity::IsRaycastTarget() const
{
	return !_skipInRaycast;
}

void Entity::Enable()
{
	if (IsEnabledSelf())
		return;

	_isEnabled = true;

	if (_inheritedDisabled <= 0)
	{
		for (auto &behaviour : _behaviours)
			behaviour->InheritEnabled(true);
	}

	for (auto &child : _children)
		child->DecrementDisable();
}
void Entity::Disable()
{
	if (!IsEnabledSelf())
		return;

	_isEnabled = false;

	if (_inheritedDisabled <= 0)
	{
		for (auto &behaviour : _behaviours)
			behaviour->InheritEnabled(false);
	}

	for (auto &child : _children)
		child->IncrementDisable();
}
void Entity::SetEnabledSelf(bool state)
{
	if (state)
		Enable();
	else
		Disable();
}

void Entity::IncrementDisable()
{
	if (_inheritedDisabled++ <= 0 && _isEnabled)
	{
		for (auto &behaviour : _behaviours)
			behaviour->InheritEnabled(false);
	}

	for (auto &child : _children)
		child->IncrementDisable();
}
void Entity::DecrementDisable()
{
	if (_inheritedDisabled <= 0)
		return;

	_inheritedDisabled--;

	if (IsEnabled())
	{
		for (auto &behaviour : _behaviours)
			behaviour->InheritEnabled(true);
	}

	for (auto &child : _children)
		child->DecrementDisable();
}
void Entity::SetInheritedDisableCount(UINT count)
{
	bool prevIsEnabled = _inheritedDisabled <= 0;
	bool newIsEnabled = count <= 0;

	_inheritedDisabled = count;

	if (_isEnabled && prevIsEnabled != newIsEnabled)
	{
		for (auto &behaviour : _behaviours)
			behaviour->InheritEnabled(newIsEnabled);
	}

	for (auto &child : _children)
		child->SetInheritedDisableCount(count);
}

void Entity::SetDirty()
{
	for (auto &behaviour : _behaviours)
		behaviour.get()->SetDirty();

	_recalculateBounds = true;
	_recalculateCollider = true;

	for (auto &child : _children)
		child->SetDirty();
}
void Entity::SetDirtyImmediate()
{
	_recalculateBounds = true;
	_recalculateCollider = true;
	for (auto &behaviour : _behaviours)
		behaviour.get()->SetDirty();
}

void Entity::CallTransformEdited(bool first)
{
	for (auto &behaviour : _behaviours)
	{
		if (first)
			behaviour->InitialOnEditTransform();
		behaviour->InitialOnEditTransformRec();
	}

	for (auto &child : _children)
		child->CallTransformEdited(false);
}
void Entity::SignalTransformEdited()
{
	CallTransformEdited(true);
}

void Entity::MarkAsRemoved()
{
	_isRemoved = true;

	for (auto &child : _children)
		child->MarkAsRemoved();
}
bool Entity::IsRemoved() const
{
	return _isRemoved;
}

bool Entity::IsPrefab() const
{
	return !_prefabName.empty();
}
const std::string &Entity::GetPrefabName() const
{
	return _prefabName;
}
void Entity::SetPrefabName(const std::string &name)
{
	_prefabName = name;
}
void Entity::UnlinkFromPrefab()
{
	_prefabName.clear();
}

inline void Entity::AddChild(Entity *child, bool keepWorldTransform)
{
	if (!child)
		return;

	if (!_children.empty())
	{
		auto it = std::find(_children.begin(), _children.end(), child);
		if (it != _children.end())
			return;
	}

	_children.emplace_back(child);

	child->SetParent(this, keepWorldTransform);
	child->_transform.SetParent(&_transform, keepWorldTransform);
}
inline void Entity::RemoveChild(Entity *child, bool keepWorldTransform)
{
	if (!child)
		return;

	if (_children.empty())
		return;

	auto it = std::find(_children.begin(), _children.end(), child);
	if (it != _children.end())
		_children.erase(it);

	child->_transform.SetParent(nullptr, keepWorldTransform);
}

void Entity::SetParent(Entity *newParent, bool keepWorldTransform)
{
	if (_parent == newParent)
		return;

	if (newParent == this)
	{
		DbgMsg("Cannot parent an entity to itself!");
		return;
	}

	// Check if new parent is a child of this entity
	if (newParent)
	{
		if (newParent->IsChildOf(this))
		{
			Warn("Cannot parent an entity to it's child! (Did you mean to unparent the child first?)");
			return;
		}
	}

	if (_parent)
	{
		_parent->RemoveChild(this, keepWorldTransform);
	}

	_parent = newParent;

	if (newParent)
	{
		newParent->AddChild(this, keepWorldTransform);
		SetInheritedDisableCount(newParent->_inheritedDisabled + (newParent->_isEnabled ? 0 : 1));
	}
	else
	{
		_transform.SetParent(nullptr, keepWorldTransform);
		SetInheritedDisableCount(0);
	}

	SetDirty();
}
Entity *Entity::GetParent() const
{
	return _parent;
}
UINT Entity::GetChildCount() const
{
	return _children.size();
}
const std::vector<Entity *> *Entity::GetChildren() const
{
	return &_children;
}
void Entity::GetChildrenRecursive(std::vector<Entity *> &children) const
{
	children.insert(children.end(), _children.begin(), _children.end());

	for (auto &child : _children)
		child->GetChildrenRecursive(children);
}
void Entity::ReorderChild(Entity *child, UINT newIndex)
{
	if (!child)
		return;

	if (newIndex >= _children.size())
		return;

	UINT currIndex = CONTENT_NULL;
	for (UINT i = 0; i < _children.size(); i++)
	{
		if (_children[i] == child)
		{
			currIndex = i;
			break;
		}
	}

	if (currIndex == CONTENT_NULL)
		return;

	if (currIndex == newIndex)
		return;

	if (currIndex < newIndex)
		newIndex--; // Account for the removal of the child from its current position

	auto it = _children.begin() + currIndex;
	_children.erase(it);
	_children.insert(_children.begin() + newIndex, child);
}
void Entity::ReorderChild(Entity *child, Entity *after)
{
	if (!child)
		return;

	if (!after)
		return;

	if (child == after)
		return;

	UINT currIndex = CONTENT_NULL;
	for (UINT i = 0; i < _children.size(); i++)
	{
		if (_children[i] == child)
		{
			currIndex = i;
			break;
		}
	}

	if (currIndex == CONTENT_NULL)
		return;

	UINT afterIndex = CONTENT_NULL;
	for (UINT i = 0; i < _children.size(); i++)
	{
		if (_children[i] == after)
		{
			afterIndex = i;
			break;
		}
	}

	if (afterIndex == CONTENT_NULL)
		return;

	afterIndex++; // Insert one after the specified child

	if (currIndex < afterIndex)
		afterIndex--; // Account for the removal of the child from its current position

	auto it = _children.begin() + currIndex;
	_children.erase(it);
	_children.insert(_children.begin() + afterIndex, child);
}
bool Entity::IsChildOf(const Entity *ent, bool immediate) const
{
	if (!ent)
		return false;

	if (this == ent)
		return false;

	if (immediate)
		return _parent == ent;

	Entity *parentIter = _parent;
	while (parentIter)
	{
		if (parentIter == ent)
			return true;

		parentIter = parentIter->GetParent();
	}

	return false;
}
bool Entity::IsParentOf(const Entity *ent, bool immediate) const
{
	return ent->IsChildOf(this, immediate);
}

Culling::CullingPlacement &Entity::GetCullingPlacement()
{
	return _cullingPlacement;
}

void Entity::SetScene(Scene *scene)
{
	_scene = scene;
}
Scene *Entity::GetScene() const
{
	return _scene;
}
Game *Entity::GetGame() const
{
	return _scene->GetGame();
}

UINT Entity::GetID() const
{
	return _entityID;
}
void Entity::SetName(const std::string &name)
{
	// Update name search hash in SceneHolder
	if (_scene)
	{
		if (SceneHolder *sceneHolder = _scene->GetSceneHolder())
		{
			if (!_name.empty())
				sceneHolder->RemoveNameFromHash(_name); // Remove old name from hash

			sceneHolder->RemoveNameFromHashIfNull(name); // Remove new name from hash if it's hashed as not found
		}
	}

	_name.assign(name);
}
const std::string &Entity::GetName() const
{
	return _name;
}
Transform *Entity::GetTransform()
{
	return &_transform;
}

UINT Entity::GetDeserializedID() const
{
	return _deserializedID;
}
void Entity::SetDeserializedID(UINT id)
{
	_deserializedID = id;
}

bool Entity::GetShowInHierarchy(bool ignoreShowHidden) const
{
	if (_showInHierarchy || ignoreShowHidden)
		return _showInHierarchy;

#ifdef DEBUG_BUILD
	return DebugData::Get().hierarchyShowHidden;
#else
	return false;
#endif
}
void Entity::SetShowInHierarchy(bool show)
{
	_showInHierarchy = show;
}

bool Entity::HasBounds(bool includeTriggers, BoundingOrientedBox &out)
{
	for (auto &behaviour : _behaviours)
	{
		MeshBehaviour *meshBehaviour = dynamic_cast<MeshBehaviour *>(behaviour.get());
		if (meshBehaviour)
		{
			meshBehaviour->StoreBounds(out);
			return true;
		}
	}

	return false;
}
void Entity::GetFullBoundsPoints(bool includeTriggers, std::vector<dx::XMFLOAT3> &points)
{
	if (!_isInitialized)
		return;
	if (!IsEnabled())
		return;

	dx::XMFLOAT3 corners[8];

	dx::BoundingOrientedBox combinedBounds;
	if (HasBounds(includeTriggers, combinedBounds))
	{
		combinedBounds.GetCorners(corners);
		points.insert(points.end(), std::begin(corners), std::end(corners));
	}

	for (auto &child : _children)
		child->GetFullBoundsPoints(includeTriggers, points);
}
bool Entity::GetFullBounds(bool includeTriggers, dx::BoundingOrientedBox &bounds)
{
	// Get bounds recursively, merging all children's bounds
	std::vector<dx::XMFLOAT3> points;
	GetFullBoundsPoints(includeTriggers, points);

	if (points.empty())
		return false;

	dx::BoundingOrientedBox::CreateFromPoints(bounds, points.size(), points.data(), sizeof(dx::XMFLOAT3));
	return true;
}
void Entity::SetEntityBounds(dx::BoundingOrientedBox &bounds)
{
	_bounds = bounds;
	_recalculateBounds = false;
	SetDirtyImmediate();
}
void Entity::StoreEntityBounds(dx::BoundingOrientedBox &bounds, ReferenceSpace space)
{
	if (space == ReferenceSpace::Local)
	{
		bounds = _bounds;
		return;
	}
	
	if (_recalculateBounds)
	{
		XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		_bounds.Transform(_transformedBounds, Load(&worldMatrix));
		_recalculateBounds = false;
	}

	bounds = _transformedBounds;
}

const dx::BoundingOrientedBox &Entity::GetLastCullingBounds() const
{
	return _lastTransformedCullingBounds;
}
void Entity::UpdateCullingBounds()
{
	StoreEntityBounds(_lastTransformedCullingBounds, ReferenceSpace::World);
}

bool Entity::InitialUpdate(TimeUtils &time, const Input &input)
{
	if (!_isEnabled)
		return true;

	ZoneScopedC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialUpdate(time, input))
		{
			ErrMsg("Failed to update behaviour!");
			return false;
		}
	}

	return true;
}
bool Entity::InitialParallelUpdate(const TimeUtils &time, const Input &input)
{
	if (!_isEnabled)
		return true;

	ZoneScopedC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialParallelUpdate(time, input))
		{
#pragma omp critical
			{
				ErrMsg("Failed to update behaviour in parallel!");
			}
			return false;
		}
	}

	return true;
}
bool Entity::InitialLateUpdate(TimeUtils &time, const Input &input)
{
	if (!_isEnabled)
		return true;

	ZoneScopedC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialLateUpdate(time, input))
		{
			ErrMsg("Failed to late update behaviour!");
			return false;
		}
	}

	return true;
}
bool Entity::InitialFixedUpdate(float deltaTime, const Input &input)
{
	if (!_isEnabled)
		return true;

	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	ZoneScopedC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialFixedUpdate(deltaTime, input))
		{
			ErrMsg("Failed to update behaviour at fixed step!");
			return false;
		}
	}

	return true;
}

bool Entity::InitialBeforeRender()
{
	if (!_doRender)
		return true;

	_doRender = false;

	ZoneScopedC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialBeforeRender())
		{
			ErrMsg("Failed to run BeforeRender on behaviour!");
			return false;
		}
	}

	if (!_transform.UpdateConstantBuffer(GetScene()->GetContext()))
	{
		ErrMsg("Failed to update entity constant buffers!");
		return false;
	}
	return true;
}
bool Entity::InitialRender(const RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
	if (!_isEnabled)
		return true;

	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialRender(queuer, rendererInfo))
		{
			ErrMsg("Failed to render behaviour!");
			return false;
		}
	}

	_doRender = true;
	return true;
}

#ifdef USE_IMGUI
void Entity::CopyToClipboard()
{
	std::string entJSON = "[[ENTITY_JSON]] ";
	{
		json::Document doc;
		json::Value entObj(json::kObjectType);

		if (!Serialize(doc.GetAllocator(), entObj, true))
		{
			ErrMsg("Failed to serialize entity!");
			return;
		}

		json::StringBuffer buffer;
		json::Writer<json::StringBuffer> writer(buffer);
		entObj.Accept(writer);
		entJSON += buffer.GetString();
	}
	ImGui::SetClipboardText(entJSON.c_str());
}

bool Entity::UIContextMenu()
{
	DebugPlayerBehaviour *debugPlayer = _scene->GetDebugPlayer();

	// Select

	if (ImGui::MenuItem("Select Siblings") && debugPlayer)
	{
		std::vector<Entity *> siblings;

		if (_parent)
		{
			siblings.reserve(_parent->_children.size());

			for (auto &child : _parent->_children)
			{
				if (!child->GetShowInHierarchy())
					continue;
				siblings.emplace_back(child);
			}
		}
		else if (SceneHolder *sceneHolder = _scene->GetSceneHolder()) // No parent, select all root entities
		{
			auto entIter = sceneHolder->GetEntities();
			siblings.reserve(sceneHolder->GetEntityCount());

			while (Entity *ent = entIter.RootStep(true))
			{
				if (!ent->GetShowInHierarchy())
					continue;
				siblings.emplace_back(ent);
			}
		}
		
		debugPlayer->Select(siblings.data(), siblings.size(), true);
	}

	if (ImGui::MenuItem("Select Children") && debugPlayer)
	{
		// Select all immediate children
		std::vector<Entity *> visibleChildren;
		visibleChildren.reserve(_children.size());

		for (auto &child : _children)
		{
			if (!child->GetShowInHierarchy())
				continue;
			visibleChildren.emplace_back(child);
		}

		debugPlayer->Select(visibleChildren.data(), visibleChildren.size(), true);
	}

	ImGui::Dummy({1,0}); ImGui::Separator(); ImGui::Dummy({1,0}); // Create Entity

	if (ImGui::MenuItem("New Sibling"))
	{
		// Create new empty entity
		Entity *ent = nullptr;
		dx::BoundingOrientedBox defaultBounds = dx::BoundingOrientedBox();

		if (!_scene->CreateEntity(&ent, "New Entity", defaultBounds, true))
		{
			ErrMsg("Failed to create new entity!");
			return false;
		}

		// Parent to same parent as this entity
		if (_parent)
			_parent->AddChild(ent);

		// Set order to be after this entity
		if (_parent)
			_parent->ReorderChild(ent, this);
		if (SceneHolder *sceneHolder = _scene->GetSceneHolder())
			sceneHolder->ReorderEntity(ent, this);

		// Select new entity
		if (debugPlayer)
			debugPlayer->Select(ent);
	}

	if (ImGui::MenuItem("New Child"))
	{
		// Create new empty entity
		Entity *ent = nullptr;
		dx::BoundingOrientedBox defaultBounds = dx::BoundingOrientedBox();

		if (!_scene->CreateEntity(&ent, "New Entity", defaultBounds, true))
		{
			ErrMsg("Failed to create new entity!");
			return false;
		}

		// Parent to this entity
		AddChild(ent);

		// Select new entity
		if (debugPlayer)
			debugPlayer->Select(ent);
	}

	ImGui::Dummy({1,0}); ImGui::Separator(); ImGui::Dummy({1,0}); // View Align / Move

	if (CameraBehaviour *camera = _scene->GetViewCamera())
	{
		if (ImGui::MenuItem("Move View To Entity"))
		{
			// Translate camera to entity position subtracted by camera forward vector multiplied by some distance.
			// If entity has bounds, use bounds center & size, otherwise use transform position.

			XMFLOAT3 targetPos{};
			float distance = 5.0f;

			if (BoundingOrientedBox bounds; GetFullBounds(false, bounds))
			{
				targetPos = bounds.Center;
				// Increase distance based on bounds size
				distance = max(distance, max(bounds.Extents.x, max(bounds.Extents.y, bounds.Extents.z)) * 2.0f);
			}
			else
			{
				targetPos = _transform.GetPosition(World);
			}

			Transform *cameraTrans = camera->GetTransform();
			XMFLOAT3 camForward = cameraTrans->GetForward(World);

			XMFLOAT3A newCamPos;
			Store(newCamPos, Load(targetPos) - (Load(camForward) * distance));

			cameraTrans->SetPosition(newCamPos, World);
		}

		if (ImGui::MenuItem("Move Entity To View"))
		{
			// Translate entity to camera position plus camera forward vector multiplied by some distance.
			// If entity has bounds, use bounds center & size, otherwise use transform position.

			XMFLOAT3 originPos{};
			float distance = 5.0f;

			if (BoundingOrientedBox bounds; GetFullBounds(false, bounds))
			{
				originPos = bounds.Center;
				// Increase distance based on bounds size
				distance = max(distance, max(bounds.Extents.x, max(bounds.Extents.y, bounds.Extents.z)) * 2.0f);
			}
			else
			{
				originPos = _transform.GetPosition(World);
			}

			Transform *cameraTrans = camera->GetTransform();
			XMFLOAT3 camPos = cameraTrans->GetPosition(World);
			XMFLOAT3 camForward = cameraTrans->GetForward(World);

			XMFLOAT3A entMovement;
			Store(entMovement, (Load(camPos) + (Load(camForward) * distance)) - Load(originPos));

			_transform.Move(entMovement, World);
			SignalTransformEdited();
		}

		if (ImGui::MenuItem("Align View With Entity"))
		{
			// Copy entity position & rotation to camera.
			Transform *cameraTrans = camera->GetTransform();
			cameraTrans->SetPosition(_transform.GetPosition(World), World);
			cameraTrans->SetRotation(_transform.GetRotation(World), World);
		}

		if (ImGui::MenuItem("Align Entity With View"))
		{
			// Copy camera position & rotation to entity.
			Transform *cameraTrans = camera->GetTransform();
			_transform.SetPosition(cameraTrans->GetPosition(World), World);
			_transform.SetRotation(cameraTrans->GetRotation(World), World); 
			SignalTransformEdited();
		}

		if (ImGui::MenuItem("Position With Cursor") && debugPlayer)
		{
			debugPlayer->PositionWithCursor(this);
		}
	}

	ImGui::Dummy({1,0}); ImGui::Separator(); ImGui::Dummy({1,0}); // Prefab
	
	if (ImGui::MenuItem("New Prefab"))
	{
		static std::string prefabSaveName = "";
		prefabSaveName = _name;

		static const std::string windowID = "SaveAsPrefabWindow";
		ImGuiUtils::ImGuiAutoWindow *window;

		// Close any existing window with the same ID
		if (ImGuiUtils::Utils::GetWindow(windowID, &window))
		{
			if (!ImGuiUtils::Utils::CloseWindow(window))
			{
				ErrMsg("Failed to close existing SaveAsPrefab window!");
				return false;
			}
		}

		std::function<bool()> saveAsPrefabFunc = [&]() -> bool {
			bool doSavePrefab = false;

			ImGui::Text("Name:"); ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::InputText("##PrefabName", &prefabSaveName);

			static bool listenForEnter = true;
			if (ImGui::Button("Save") || (ImGui::IsWindowFocused() && Input::Instance().GetKey(KeyCode::Enter, true) == KeyState::Pressed))
			{
				if (!prefabSaveName.empty())
				{
					std::vector<std::string> prefabs;
					_scene->GetPrefabNames(prefabs);

					bool nameCollision = false;
					for (const auto &name : prefabs)
					{
						if (name != prefabSaveName)
							continue;

						nameCollision = true;
					}

					if (nameCollision)
					{
						ImGui::OpenPopup("Confirm Overwrite Prefab");
						listenForEnter = false;
					}
					else
					{
						doSavePrefab = true;
					}
				}
			}

			static float cancelButtonWidth = 30.0f;
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - cancelButtonWidth);

			if (ImGui::Button("Cancel") || (ImGui::IsWindowFocused() && Input::Instance().GetKey(KeyCode::Escape, true) == KeyState::Pressed))
			{
				if (!ImGuiUtils::Utils::CloseWindow(windowID))
				{
					ErrMsg("Failed to close SaveAsPrefab window!");
					return false;
				}

				return true;
			}
			cancelButtonWidth = ImGui::GetItemRectSize().x;

			bool closeSavePrefabPopup = false;
			if (ImGui::BeginPopup("Confirm Overwrite Prefab"))
			{
				ImGui::Text("This prefab already exists.\nOverwrite it?", prefabSaveName.c_str());
				ImGui::Separator();
				
				KeyState enter = Input::Instance().GetKey(KeyCode::Enter, true);
				if (!listenForEnter && enter == KeyState::None)
					listenForEnter = true;

				if (ImGui::Button("Yes") || (listenForEnter && enter == KeyState::Pressed))
				{
					doSavePrefab = true;
					ImGui::CloseCurrentPopup();
				}

				static float noButtonWidth = 30.0f;
				ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - noButtonWidth);

				if (ImGui::Button("No") || Input::Instance().GetKey(KeyCode::Escape, true) == KeyState::Pressed)
					ImGui::CloseCurrentPopup();
				noButtonWidth = ImGui::GetItemRectSize().x;

				ImGui::EndPopup();
			}

			if (doSavePrefab)
			{
				if (!_scene->SaveAsPrefab(prefabSaveName, this))
					WarnF("Failed to save entity '{}' as prefab '{}'!", _name, prefabSaveName);

				closeSavePrefabPopup = true;
			}

			if (closeSavePrefabPopup || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			{
				if (!ImGuiUtils::Utils::CloseWindow(windowID))
				{
					ErrMsg("Failed to close SaveAsPrefab window!");
					return false;
				}
			}

			return true;
		};

		const std::string windowName = std::format("Save '{}' as Prefab", GetName());
		if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, saveAsPrefabFunc, ImRect(ImGui::GetCursorScreenPos(), ImVec2(350, 80))))
		{
			ErrMsg("Failed to open SaveAsPrefab window!");
			return false;
		}
	}

	if (IsPrefab())
	{
		if (ImGui::MenuItem("Overwrite Prefab"))
		{
			std::string prefabSaveName = GetPrefabName();

			if (!_scene->SaveAsPrefab(prefabSaveName, this))
				WarnF("Failed to save entity '{}' as prefab '{}'!", _name, prefabSaveName);
		}

		if (ImGui::MenuItem("Reset Prefab"))
		{
			Entity *ent = _scene->SpawnPrefab(GetPrefabName());

			if (ent)
			{
				// Set parent to this entity's parent
				ent->SetParent(_parent);

				// Copy transform from this entity to the new prefab instance
				const dx::XMFLOAT4X4A &localMatrix = _transform.GetMatrix(Local);
				ent->GetTransform()->SetMatrix(localMatrix, Local);

				// Set order in hierarchy to be after this entity
				if (_parent != nullptr)
					_parent->ReorderChild(ent, this);

				_scene->GetSceneHolder()->ReorderEntity(ent, this);

				// Check if this entity is selected
				bool isSelected = false;
				if (debugPlayer)
					isSelected = debugPlayer->IsSelected(this);

				// Transfer references from this entity to the new prefab instance
				ent->ReplaceTarget(*this);

				// Delete this entity
				if (!_scene->GetSceneHolder()->RemoveEntity(this))
				{
					ErrMsg("Failed to remove entity!");
					return false;
				}

				if (isSelected)
					debugPlayer->Select(ent, true);
			}
			else
				WarnF("Failed to spawn prefab '{}'", GetPrefabName());
		}

		if (ImGui::MenuItem("Unlink Prefab"))
		{
			UnlinkFromPrefab();
		}
	}

	if (ImGui::MenuItem("Replace with Prefab"))
	{
		static const std::string windowID = "ReplaceWithPrefabWindow";
		ImGuiUtils::ImGuiAutoWindow *window;

		// Close any existing window with the same ID
		if (ImGuiUtils::Utils::GetWindow(windowID, &window))
		{
			if (!ImGuiUtils::Utils::CloseWindow(window))
			{
				ErrMsg("Failed to close existing ReplaceWithPrefab window!");
				return false;
			}
		}
		
		std::function<bool()> replaceWithPrefabFunc = [&]() -> bool {
			std::vector<std::string> prefabs;
			_scene->GetPrefabNames(prefabs);

			static std::string selectedPrefab = "";

			ImGui::Text("Selected: '%s'", selectedPrefab.c_str());
			ImGui::Separator();

			// Search filter
			{
				static std::string search = "";
				if (ImGui::Button("Clear"))
					search.clear();
				ImGui::SameLine();

				if (ImGui::InputText("##PrefabSearch", &search))
					std::transform(search.begin(), search.end(), search.begin(), ::tolower);

				for (int i = 0; i < prefabs.size(); i++)
				{
					std::string prefabLower = prefabs[i];
					std::transform(prefabLower.begin(), prefabLower.end(), prefabLower.begin(), ::tolower);

					if (prefabLower.find(search) == std::string::npos)
					{
						prefabs.erase(prefabs.begin() + i);
						i--;
					}
				}
			}

			ImGuiChildFlags childFlags = ImGuiChildFlags_None;
			childFlags |= ImGuiChildFlags_Borders;
			childFlags |= ImGuiChildFlags_ResizeY;

			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
			windowFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;

			if (ImGui::BeginChild("Prefab List", ImVec2(0, 300), childFlags, windowFlags))
			{
				for (int i = 0; i < prefabs.size(); i++)
				{
					std::string &prefab = prefabs[i];

					if (ImGui::Selectable(prefab.c_str(), selectedPrefab == prefab))
						selectedPrefab = std::move(prefab);
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			if ((ImGui::Button("Confirm") || Input::Instance().GetKey(KeyCode::Enter, true) == KeyState::Pressed) 
				&& !selectedPrefab.empty())
			{
				Entity *ent = _scene->SpawnPrefab(selectedPrefab);

				if (ent)
				{
					// Set parent to this entity's parent
					ent->SetParent(_parent);

					// Copy transform from this entity to the new prefab instance
					const dx::XMFLOAT4X4A &localMatrix = _transform.GetMatrix(Local);
					ent->GetTransform()->SetMatrix(localMatrix, Local);

					// Set order in hierarchy to be after this entity
					if (_parent != nullptr)
						_parent->ReorderChild(ent, this);

					_scene->GetSceneHolder()->ReorderEntity(ent, this);

					// Check if this entity is selected
					bool isSelected = false;
					if (debugPlayer)
						isSelected = debugPlayer->IsSelected(this);

					// Transfer references from this entity to the new prefab instance
					ent->ReplaceTarget(*this);

					// Delete this entity
					if (!_scene->GetSceneHolder()->RemoveEntity(this))
					{
						ErrMsg("Failed to remove entity!");
						return false;
					}

					if (isSelected)
						debugPlayer->Select(ent, true);
				}
				else
					WarnF("Failed to spawn prefab '{}'", selectedPrefab);

				if (!ImGuiUtils::Utils::CloseWindow(windowID))
				{
					ErrMsg("Failed to close ReplaceWithPrefab window!");
					return false;
				}
			}

			static float cancelButtonWidth = 30.0f;
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - cancelButtonWidth);

			if (ImGui::Button("Cancel") || (Input::Instance().GetKey(KeyCode::Escape, true) == KeyState::Pressed) 
				|| !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			{
				if (!ImGuiUtils::Utils::CloseWindow(windowID))
				{
					ErrMsg("Failed to close ReplaceWithPrefab window!");
					return false;
				}
			}
			cancelButtonWidth = ImGui::GetItemRectSize().x;

			return true;
		};

		const std::string windowName = std::format("Replace '{}' with Prefab", GetName());
		if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, replaceWithPrefabFunc, ImRect(ImGui::GetCursorScreenPos(), ImVec2(0, 0))))
		{
			ErrMsg("Failed to open ReplaceWithPrefab window!");
			return false;
		}
	}

	ImGui::Dummy({1,0}); ImGui::Separator(); ImGui::Dummy({1,0}); // Copy / Paste / Remove

	if (ImGui::MenuItem("Copy"))
	{
		CopyToClipboard();
	}

	// If clipboard has pastable data, show paste option
	{
		constexpr const char *entDataPrefix = "[[ENTITY_JSON]] ";
		constexpr const char *behDataPrefix = "[[BEHAVIOUR_JSON]] ";
		std::string clipboardData = ImGui::GetClipboardText();
		
		// Check if clipboard data starts with behaviour data prefix
		if (clipboardData.rfind(entDataPrefix, 0) == 0)
		{
			if (ImGui::MenuItem("Paste##PasteEnt"))
			{
				clipboardData = clipboardData.substr(strlen(entDataPrefix)); // Remove prefix

				json::Document doc;
				doc.Parse(clipboardData.c_str());
				if (doc.HasParseError())
				{
					ErrMsg("Failed to parse entity JSON data!");
					return false;
				}

#pragma push_macro("GetObject")
#undef GetObject
				Entity *childEntity = nullptr;
				if (!_scene->DeserializeEntity(doc.GetObject(), &childEntity))
				{
					ErrMsg("Failed to deserialize entity!");
					return false;
				}
#pragma pop_macro("GetObject")

				if (childEntity)
				{
					// Set the parent of the child entity
					childEntity->SetParent(this, false);
				}

				_scene->RunPostDeserializeCallbacks();
			}
		}
		else if (clipboardData.rfind(behDataPrefix, 0) == 0)
		{
			if (ImGui::MenuItem("Paste##PasteBeh"))
			{
				clipboardData = clipboardData.substr(strlen(behDataPrefix)); // Remove prefix

				json::Document doc;
				doc.Parse(clipboardData.c_str());
				if (doc.HasParseError())
				{
					ErrMsg("Failed to parse behaviour JSON data!");
					return false;
				}

				if (doc.HasMember("Name") && doc.HasMember("Attributes"))
				{
					const std::string behName = doc["Name"].GetString();
					const json::Value &behAttributes = doc["Attributes"];

					Behaviour *beh = BehaviourFactory::CreateBehaviour(behName);
					if (beh && !beh->InitialDeserialize(behAttributes, GetScene()))
					{
						ErrMsg("Failed to deserialize behaviour!");
						return false;
					}

					if (!beh->Initialize(this))
					{
						ErrMsg("Failed to bind behaviour to entity!");
						return false;
					}

					GetScene()->RunPostDeserializeCallbacks();
				}
			}
		}
		else
		{
			// No valid clipboard data for pasting
			ImGui::MenuItem("Paste##PasteNULL", nullptr, false, false);
		}
	}

	if (ImGui::MenuItem("Remove"))
	{
		Destroy();
	}

	return true;
}

bool Entity::InitialRenderUI()
{
	DebugPlayerBehaviour *debugPlayer = _scene->GetDebugPlayer();
	SceneHolder *sceneHolder = _scene->GetSceneHolder();
	int entIndex = sceneHolder->GetEntityIndex(this);
	int entID = _entityID;

	// Entity Header
	{
		ImGuiStorage *storage = ImGui::GetStateStorage();
		ImGuiID headerButtonsWidthID = ImGui::GetID("HeaderButtonsWidth");
		float buttonsWidth = storage->GetFloat(headerButtonsWidthID, 60.0f);

		bool entEnabled = IsEnabledSelf();
		if (ImGui::Checkbox("##EntEnabled", &entEnabled))
			SetEnabledSelf(entEnabled);
		ImGui::SetItemTooltip(entEnabled ? "Disable" : "Enable");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(max(0, ImGui::GetContentRegionAvail().x - buttonsWidth));

		std::string entName = GetName();
		if (ImGui::InputText("##EntName", &entName, ImGuiInputTextFlags_AutoSelectAll))
			SetName(entName);
		ImGui::SetItemTooltip("Entity Name");

		ImGui::SameLine();
		float buttonStartX = ImGui::GetCursorScreenPos().x;
		
		// Buttons
		{
			// Dock/Undock button
			{
				const std::string windowID = std::format("Ent#{}:{}", entID, _scene->GetUID());

				// Check if entity is undocked
				if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
				{
					// If undocked, show dock button
					if (ImGuiUtils::ButtonWithFont(ICON_LC_SQUARE_ARROW_OUT_DOWN_LEFT "##Dock", FONT_ICON_FILE_NAME_LC, 14.0f))
					{
						if (!ImGuiUtils::Utils::CloseWindow(windowID))
						{
							ErrMsg("Failed to dock entity window!");
							return false;
						}
					}

					ImGui::SetItemTooltip("Dock Entity Inspector Window");
				}
				else
				{
					// If docked, show undock button
					if (ImGuiUtils::ButtonWithFont(ICON_LC_SQUARE_ARROW_OUT_UP_RIGHT "##Undock", FONT_ICON_FILE_NAME_LC, 14.0f))
					{
						const std::string windowName = std::format("Entity '{}'", GetName());
						if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Entity::InitialRenderUI, this)))
						{
							ErrMsg("Failed to undock entity window!");
							return false;
						}
					}

					ImGui::SetItemTooltip("Undock Entity Inspector Window");
				}
			}

			ImGui::SameLine();

			// Copy button
			{
				if (ImGuiUtils::ButtonWithFont(ICON_LC_COPY "##Copy", FONT_ICON_FILE_NAME_LC, 14.0f))
				{
					Entity *ent = debugPlayer->DuplicateEntity(this);
					debugPlayer->Select(ent, ImGui::GetIO().KeyShift);
				}

				ImGui::SetItemTooltip("Duplicate");
			}

			ImGui::SameLine();

			// Save as prefab button
			{
				static std::string prefabName = "";
				bool doSave = false;

				if (ImGuiUtils::ButtonWithFont(ICON_LC_BOOK_MARKED "##SavePrefab", FONT_ICON_FILE_NAME_LC, 14.0f))
				{
					ImGui::OpenPopup("Save as Prefab");
					prefabName = GetName();
				}

				ImGui::SetItemTooltip("Save as Prefab");

				if (ImGui::BeginPopup("Save as Prefab"))
				{
					ImGui::Text("Prefab Name:");
					ImGui::SameLine();
					ImGui::InputText("##PrefabName", &prefabName);

					if (ImGui::Button("Save"))
					{
						if (!prefabName.empty())
						{
							std::vector<std::string> prefabs;
							_scene->GetPrefabNames(prefabs);

							bool nameCollision = false;
							for (const auto &name : prefabs)
							{
								if (name != prefabName)
									continue;

								nameCollision = true;
							}

							if (nameCollision)
							{
								ImGui::OpenPopup("Confirm Overwrite Prefab");
							}
							else 
							{
								doSave = true;
								ImGui::CloseCurrentPopup();
							}
						}
					}
					ImGui::SameLine();

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();

					bool closeSavePrefabPopup = false;
					if (ImGui::BeginPopup("Confirm Overwrite Prefab"))
					{
						ImGui::Text("This prefab already exists.\nOverwrite it?", prefabName.c_str());
						ImGui::Separator();

						if (ImGui::Button("Yes"))
						{
							doSave = true;
							closeSavePrefabPopup = true;
							ImGui::CloseCurrentPopup();
						}

						static float noButtonWidth = 30.0f;
						ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - noButtonWidth);

						if (ImGui::Button("No"))
							ImGui::CloseCurrentPopup();
						noButtonWidth = ImGui::GetItemRectSize().x;

						ImGui::EndPopup();
					}

					if (closeSavePrefabPopup)
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				if (doSave)
				{
					if (!_scene->SaveAsPrefab(prefabName, this))
						WarnF("Failed to save entity '{}' as prefab '{}'!", _name, prefabName);
				}
			}

			ImGui::SameLine();

			// Replace with prefab button
			{
				if (ImGuiUtils::ButtonWithFont(ICON_LC_BOOK_COPY "##ReplaceWithPrefab", FONT_ICON_FILE_NAME_LC, 14.0f))
					ImGui::OpenPopup("Replace With Prefab");

				ImGui::SetItemTooltip("Replace With Prefab");

				if (ImGui::BeginPopup("Replace With Prefab", NULL))
				{
					std::vector<std::string> prefabs;
					_scene->GetPrefabNames(prefabs);

					static std::string selectedPrefab = "";

					ImGui::Text("Selected: '%s'", selectedPrefab.c_str());
					ImGui::Separator();

					// Search filter
					{
						static std::string search = "";
						if (ImGui::Button("Clear"))
							search.clear();
						ImGui::SameLine();

						if (ImGui::InputText("##PrefabSearch", &search))
							std::transform(search.begin(), search.end(), search.begin(), ::tolower);

						for (int i = 0; i < prefabs.size(); i++)
						{
							std::string prefabLower = prefabs[i];
							std::transform(prefabLower.begin(), prefabLower.end(), prefabLower.begin(), ::tolower);

							if (prefabLower.find(search) == std::string::npos)
							{
								prefabs.erase(prefabs.begin() + i);
								i--;
							}
						}
					}

					ImGuiChildFlags childFlags = ImGuiChildFlags_None;
					childFlags |= ImGuiChildFlags_Borders;
					childFlags |= ImGuiChildFlags_ResizeY;

					ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
					windowFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;

					if (ImGui::BeginChild("Prefab List", ImVec2(0, 300), childFlags, windowFlags))
					{
						for (int i = 0; i < prefabs.size(); i++)
						{
							std::string &prefab = prefabs[i];

							if (ImGui::Selectable(prefab.c_str(), selectedPrefab == prefab))
								selectedPrefab = std::move(prefab);
						}
					}
					ImGui::EndChild();
					ImGui::Separator();

					if (ImGui::Button("Confirm") && !selectedPrefab.empty())
					{
						Entity *ent = _scene->SpawnPrefab(selectedPrefab);

						if (ent)
						{
							// Set parent to this entity's parent
							ent->SetParent(_parent);

							// Copy transform from this entity to the new prefab instance
							const dx::XMFLOAT4X4A &localMatrix = _transform.GetMatrix(Local);
							ent->GetTransform()->SetMatrix(localMatrix, Local);

							if (_parent != nullptr)
								_parent->ReorderChild(ent, this);

							_scene->GetSceneHolder()->ReorderEntity(ent, this);

							// Transfer references from this entity to the new prefab instance
							ent->ReplaceTarget(*this);

							// Delete this entity
							if (!sceneHolder->RemoveEntity(this))
							{
								ErrMsg("Failed to remove entity!");
								ImGui::CloseCurrentPopup();
								return false;
							}

							debugPlayer->Select(ent, true);
						}
						else
							WarnF("Failed to spawn prefab '{}'", selectedPrefab);

						ImGui::CloseCurrentPopup();
					}

					static float cancelButtonWidth = 30.0f;
					ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - cancelButtonWidth);

					if (ImGui::Button("Cancel"))
						ImGui::CloseCurrentPopup();
					cancelButtonWidth = ImGui::GetItemRectSize().x;

					ImGui::EndPopup();
				}
			}

			ImGui::SameLine();

			// Delete button
			{
				ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
				if (ImGuiUtils::ButtonWithFont(ICON_LC_X "##Delete", FONT_ICON_FILE_NAME_LC, 14.0f))
				{
					debugPlayer->Deselect(this);

					if (!sceneHolder->RemoveEntity(entIndex))
					{
						ErrMsg("Failed to remove entity!");
						ImGuiUtils::EndButtonStyle();
						return false;
					}
				}
				ImGuiUtils::EndButtonStyle();

				ImGui::SetItemTooltip("Delete");
			}
		}

		float buttonEndX = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;

		storage->SetFloat(headerButtonsWidthID, buttonEndX - buttonStartX);

		if (IsPrefab())
		{
			ImGui::TextColored({ 1.0f, 0.95f, 0.65f, 1.0f }, "Prefab Instance: %s", GetPrefabName().c_str());

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.55f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.65f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.75f, 0.8f));
			if (ImGui::SmallButton("X"))
				UnlinkFromPrefab();
			ImGui::PopStyleColor(3);
		}

		// Properties
		if (ImGui::TreeNode("Properties"))
		{
			ImGui::Text("Parent:"); ImGui::SameLine();
			Entity *parent = GetParent();
			if (parent)
			{
				UINT parentIndex = sceneHolder->GetEntityIndex(parent);

				if (ImGui::SmallButton(std::format("{} ({})", parent->GetName(), parentIndex).c_str()))
				{
					if (!ImGui::GetIO().KeyShift)
						debugPlayer->Deselect(this);

					debugPlayer->Select(parent, true);
				}

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.55f, 0.6f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.65f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.75f, 0.8f));
				if (ImGui::SmallButton("X"))
					SetParent(parent->GetParent(), debugPlayer->GetEditSpace() == World);
				ImGui::PopStyleColor(3);
			}
			else
			{
				ImGui::Text("None");
			}

			ImGui::Text("Index: %d", entIndex);
			ImGui::Text("ID: %d", entID);
			ImGui::Text("References: %d", GetRefs().size());

			ImGui::Separator();

			bool hidden = !_showInHierarchy;
			ImGui::Checkbox("Hidden", &hidden);
			_showInHierarchy = !hidden;

			ImGui::Checkbox("Static", &_isStatic);
			ImGui::Checkbox("Selectable", &_isDebugSelectable);
			ImGui::Checkbox("Serialized##EntSerialize", &_doSerialize);

			ImGui::TreePop();
		}
	}

	ImGui::Separator();

	ImGui::PushID("Transform");
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_AllowOverlap))
	{
		bool changed = false;
		if (!_transform.RenderUI(debugPlayer->GetEditSpace(), &changed))
		{
			ErrMsg("Failed to render transform UI!");
			ImGui::PopID();
			return false;
		}

		if (changed)
			SignalTransformEdited();
	}
	ImGui::PopID();

	ImGui::PushID("Behaviour List");
	{
		for (int i = 0; i < _behaviours.size(); i++)
		{
			ImGui::PushID(("Behaviour " + std::to_string(i)).c_str());
			auto behaviour = _behaviours[i]->AsRef();
			std::string behName = behaviour.Get()->GetName();

			// Header
			int openState = behaviour.Get()->PopUIOpenState();
			if (openState >= 0)
				ImGui::SetNextItemOpen(openState == 1);

			const ImVec2 headerScreenPos = ImGui::GetCursorScreenPos();
			const ImVec2 headerPos = ImGui::GetCursorPos();
			const float headerWidth = ImGui::GetContentRegionAvail().x;

			ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 7.0f);
			if (ImGui::CollapsingHeader(behName.c_str(), ImGuiTreeNodeFlags_AllowOverlap))
			{
				ImGui::PopStyleVar();

				float maxSize = behaviour.Get()->GetUISize();
				bool maximized = behaviour.Get()->GetUIMaximized();

				if (maxSize > 0.0f)
				{
					if (!behaviour.Get()->IsResizingUI() && behaviour.Get()->IsUIDirty() && maximized)
					{
						ImGui::SetNextWindowSize(ImVec2(-1.0f, maxSize), ImGuiCond_Always);
					}
					else
					{
						ImGui::SetNextWindowSizeConstraints(ImVec2(-1.0f, 1.0f), ImVec2(-1.0f, maxSize));
					}

					behaviour.Get()->SetUIDirty(false);
				}

				ImGui::BeginChild("Behaviour", ImVec2(0, 800.0f), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);

				if (!behaviour.Get()->InitialRenderUI())
				{
					ErrMsg("Failed to render behaviour UI!");
					ImGui::EndChild();
					ImGui::PopID();
					return false;
				}

				float windowSize = ImGui::GetWindowSize().y;
				float newMaxSize = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
				ImGuiID resizeBorderID = ImGui::GetWindowResizeBorderID(ImGui::GetCurrentWindow(), ImGuiDir_Down);

				ImGui::EndChild();
				
				if (behaviour.IsValid())
				{
					bool resizing = ImGui::GetActiveID() == resizeBorderID;
					behaviour.Get()->SetResizingUI(resizing);

					if (resizing)
					{
						behaviour.Get()->SetUIDirty(true);
						behaviour.Get()->SetUIMaximized(windowSize >= newMaxSize);
					}
					else if (maximized && maxSize != newMaxSize)
					{
						behaviour.Get()->SetUIDirty(true);
					}

					if (!ImGui::IsItemVisible())
					{
						newMaxSize = -1.0f;
					}

					behaviour.Get()->SetUISize(newMaxSize);
				}
			}
			else
				ImGui::PopStyleVar();
			
			const ImVec2 nextHeaderPos = ImGui::GetCursorPos();

			// Header Buttons
			{
				static ImVec2 cachedButtonsMin = ImVec2(0, 0);
				static ImVec2 cachedButtonsMax = ImVec2(0, 0);

				const ImVec2 buttonSize = ImVec2(20.0f, 20.0f);
				const ImVec2 buttonPadding = ImVec2(6.0f, 3.5f);

				// Buttons background
				{
					const ImVec2 bgPadding = ImVec2(4.0f, 3.5f);

					const ImVec2 bgMin = headerScreenPos + cachedButtonsMin - bgPadding + ImVec2(headerWidth, 0);
					const ImVec2 bgMax = headerScreenPos + cachedButtonsMax + bgPadding + ImVec2(headerWidth, 0);

					ImDrawList *drawList = ImGui::GetWindowDrawList();
					drawList->AddRectFilled(bgMin, bgMax, ImGui::GetColorU32(ImGuiCol_WindowBg), 2.5f);
					drawList->AddRect(bgMin, bgMax, ImGui::GetColorU32(ImGuiCol_Border), 2.5f);

					// Add invisible button to occlude header
					ImGui::SetCursorScreenPos(bgMin);
					ImGui::InvisibleButton("HeaderButtonsBg", bgMax - bgMin, ImGuiButtonFlags_AllowOverlap);
				}

				ImVec2 nextPos = headerPos + ImVec2(headerWidth - buttonSize.x, buttonPadding.y);

				// Delete
				{
					ImGui::SetCursorPos(nextPos);
					nextPos.x -= buttonSize.x + buttonPadding.x;

					ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
					if (ImGuiUtils::ButtonWithFont(ICON_LC_X "##Delete", FONT_ICON_FILE_NAME_LC, 14.0f, buttonSize))
						RemoveBehaviour(behaviour.Get());
					ImGuiUtils::EndButtonStyle();
					ImGui::SetItemTooltip("Delete Behaviour");
				}

				cachedButtonsMax = ImGui::GetItemRectMax() - headerScreenPos;
				cachedButtonsMax.x -= headerWidth;

				// Active checkmark
				{
					ImGui::SetCursorPos(nextPos);
					nextPos.x -= buttonSize.x + buttonPadding.x;

					bool behEnabled = behaviour.IsValid() ? behaviour.Get()->IsEnabledSelf() : false;
					if (ImGui::Checkbox("##BehEnabled", &behEnabled))
						behaviour.Get()->SetEnabled(behEnabled);
					ImGui::SetItemTooltip(behEnabled ? "Disable" : "Enable");
				}

				// Copy to clipboard
				{
					ImGui::SetCursorPos(nextPos);
					nextPos.x -= buttonSize.x + buttonPadding.x;

					ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Green);
					if (ImGuiUtils::ButtonWithFont(ICON_LC_CLIPBOARD_COPY "##CopyBehToClipboard", FONT_ICON_FILE_NAME_LC, 14.0f, buttonSize))
					{
						// Serialize behaviour to JSON and copy to clipboard
						std::string behJSON = "[[BEHAVIOUR_JSON]] ";
						{
							json::Document doc;
							json::Value behObj(json::kObjectType);

							if (!behaviour.Get()->InitialSerialize(doc.GetAllocator(), behObj))
							{
								ErrMsg("Failed to serialize behaviour!");
								ImGuiUtils::EndButtonStyle();
								return false;
							}

							json::StringBuffer buffer;
							json::Writer<json::StringBuffer> writer(buffer);
							behObj.Accept(writer);
							behJSON += buffer.GetString();
						}
						ImGui::SetClipboardText(behJSON.c_str());
					}
					ImGuiUtils::EndButtonStyle();

					ImGui::SetItemTooltip("Copy Behaviour to Clipboard");
				}

				// Open Script
				{
					ImGui::SetCursorPos(nextPos);
					nextPos.x -= buttonSize.x + buttonPadding.x;

					ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Yellow);
					if (ImGuiUtils::ButtonWithFont(ICON_FA_FILE_CODE "##OpenScript", FONT_ICON_FILE_NAME_FAS, 15.0f, buttonSize))
					{
						// Get path from Behaviour Registry
						const std::string &behCategory = BehaviourRegistry::GetCategories().at(behName);
						std::string scriptPath = std::format(BEHAVIOURS_PATH "/{}{}.cpp", behCategory, behName);

						// Replace all / with \ for Windows
						std::replace(scriptPath.begin(), scriptPath.end(), '/', '\\');

						DbgMsgF("Opening '{}'", scriptPath);

						// Open script with default program
						SHELLEXECUTEINFOA sei = { 0 };
						sei.cbSize = sizeof(SHELLEXECUTEINFOA);
						sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_WAITFORINPUTIDLE;
						sei.hwnd = nullptr;
						sei.lpVerb = "open";
						sei.lpFile = scriptPath.c_str();
						sei.lpParameters = nullptr;
						sei.lpDirectory = nullptr;
						sei.nShow = SW_SHOWNORMAL;

						if (!ShellExecuteExA(&sei))
						{
							WarnF("Failed to open script '{}' with error code {}", scriptPath, GetLastError());
						}
						else
						{
							if (!sei.hProcess)
							{
								std::this_thread::sleep_for(std::chrono::milliseconds(200));
								ShellExecuteExA(&sei);
							}

							struct WINDOWPROCESSINFO {
								DWORD pid;
								HWND hwnd;
							};

							// Get the window handle of the opened process and set it to foreground
							WINDOWPROCESSINFO info{};
							info.pid = GetProcessId(sei.hProcess);
							info.hwnd = 0;

							AllowSetForegroundWindow(info.pid);

							// Sleep for a short time to allow the process to open the file and create a window
							std::this_thread::sleep_for(std::chrono::milliseconds(500));

							EnumWindows(
								[](HWND hwnd, LPARAM lParam) -> BOOL {
									WINDOWPROCESSINFO *infoPtr = (WINDOWPROCESSINFO *)lParam;
									DWORD check = 0;
									BOOL br = TRUE;
									GetWindowThreadProcessId(hwnd, &check);

									if (check == infoPtr->pid)
									{
										infoPtr->hwnd = hwnd;
										br = FALSE;
									}

									return br;
								},
								(LPARAM)&info
							);

							if (info.hwnd != 0)
							{
								SetForegroundWindow(info.hwnd);
								SetActiveWindow(info.hwnd);
							}

							CloseHandle(sei.hProcess);
						}
					}
					ImGuiUtils::EndButtonStyle();

					ImGui::SetItemTooltip("Open Script in Editor");
				}

				cachedButtonsMin = ImGui::GetItemRectMin() - headerScreenPos;
				cachedButtonsMin.x -= headerWidth;
			}
			ImGui::PopID();

			ImGui::SetCursorPos(nextHeaderPos);
		}

		ImGui::Dummy({ 0.0f, 3.0f });

		// Add behaviour
		{
			auto &behaviourMap = BehaviourRegistry::Get();

			// Get vector of behaviour names from map
			std::vector<std::string> behaviourNames;
			behaviourNames.reserve(behaviourMap.size());

			for (const auto &pair : behaviourMap)
				behaviourNames.emplace_back(pair.first);

			// Sort the behaviour names
			std::sort(behaviourNames.begin(), behaviourNames.end());

			float addBehRegion = ImGui::GetContentRegionAvail().x;
			float addBehPadding = 60.0f;

			// Ensure button is always at least 50% of region
			if (addBehRegion < addBehPadding * 4.0f)
				addBehPadding = addBehRegion * 0.25f;

			ImGui::SetCursorPosX(addBehPadding);
			if (ImGui::Button("Add Behaviour", ImVec2(addBehRegion - addBehPadding * 2.0f, 25.0f)))
				ImGui::OpenPopup("Add Behaviour Popup");

			if (ImGui::BeginPopup("Add Behaviour Popup", ImGuiWindowFlags_NoMove))
			{
				static std::string filter = "";

				ImVec2 currSize = ImGui::GetWindowSize();
				const float popupMinWidth = 300.0f;
				float padding = ImGui::GetStyle().WindowPadding.x;
				float popupWidth = max(currSize.x - padding, popupMinWidth);
				float inputBoxPosX = ImGui::GetCursorPosX();

				if (ImGui::IsWindowAppearing())
					ImGui::SetKeyboardFocusHere(0);

				ImGui::SetNextItemWidth(popupWidth - padding);
				ImGui::InputText("##Filter", &filter, ImGuiInputTextFlags_AutoSelectAll);
				if (!ImGui::IsItemActive() && filter.empty())
				{
					ImGui::SameLine(inputBoxPosX + padding);
					ImGui::TextDisabled("Search");
				}

				if (!filter.empty())
					std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

				ImGui::Separator();

				ImGui::SetNextWindowSizeConstraints({ 50.0f, 50.0f }, { 500.0f, FLT_MAX });
				ImGui::SetWindowSize({ popupWidth, currSize.y }, ImGuiCond_Always);

				std::string selectedBehaviourName = "";
				ImGui::BeginChild("BehaviourList", ImVec2(popupWidth - padding, 350.0f), ImGuiChildFlags_ResizeY);
				if (filter.empty()) // Show categorized tree when not filtering
				{
					const BehaviourRegistry::CategoryTree &categoryTree = BehaviourRegistry::GetCategoryTree();

					std::function<void(const BehaviourRegistry::CategoryTree::CategoryNode &)> renderCategory =
						[&](const BehaviourRegistry::CategoryTree::CategoryNode &node)
					{
						for (const auto &[categoryName, subcategory] : node.subcategories)
						{
							ImGui::PushID(categoryName.c_str());
							if (ImGui::TreeNode(categoryName.c_str()))
							{
								renderCategory(subcategory);
								ImGui::TreePop();
							}
							ImGui::PopID();
						}

						for (const auto &behaviourName : node.behaviours)
						{
							if (ImGui::Selectable(behaviourName.c_str(), false))
								selectedBehaviourName = behaviourName;
						}
					};

					renderCategory(categoryTree.root);
				}
				else // Show flattened list when filtering
				{
					for (UINT i = 0; i < behaviourNames.size(); i++)
					{
						if (!filter.empty())
						{
							std::string lower = behaviourNames[i].c_str();
							std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

							if (lower.find(filter) == std::string::npos)
								continue;
						}

						if (ImGui::Selectable(behaviourNames[i].c_str(), false))
							selectedBehaviourName = behaviourNames[i].c_str();
					}
				}
				ImGui::EndChild();

				if (!selectedBehaviourName.empty())
				{
					auto it = behaviourMap.find(selectedBehaviourName);
					if (it == behaviourMap.end())
					{
						ErrMsgF("Behaviour '{}' not found in map!", selectedBehaviourName);
						return false;
					}

					std::function<Behaviour *()> behaviourConstructor = it->second;
					Behaviour *newBehaviour = behaviourConstructor();

					if (!newBehaviour->Initialize(this))
					{
						ErrMsg("Failed to bind behaviour to entity!");
						return false;
					}

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}
		}
	}
	ImGui::PopID();

	return true;
}
#endif

bool Entity::InitialBindBuffers(ID3D11DeviceContext *context)
{
	ID3D11Buffer *const wmBuffer = _transform.GetConstantBuffer();
	GetScene()->GetContext()->VSSetConstantBuffers(0, 1, &wmBuffer);

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialBindBuffers(context))
		{
			ErrMsg("Failed to bind behaviour buffers!");
			return false;
		}
	}

	return true;
}

bool Entity::InitialOnHover()
{
	if (!_isEnabled)
		return true;

	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialOnHover())
		{
			ErrMsgF("InitialOnHover() failed for behaviour '{}'!", behaviour->GetName());
			return false;
		}
	}

	return true;
}
bool Entity::InitialOffHover()
{
	if (!_isEnabled)
		return true;

	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialOffHover())
		{
			ErrMsgF("InitialOffHover() failed for behaviour '{}'!", behaviour->GetName());
			return false;
		}
	}

	return true;
}
bool Entity::InitialOnSelect()
{
	if (!_isEnabled)
		return true;

	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialOnSelect())
		{
			ErrMsgF("InitialOnSelect() failed for behaviour '{}'!", behaviour->GetName());
			return false;
		}
	}

	return true;
}
bool Entity::InitialOnDebugSelect()
{
	if (!_isInitialized)
	{
		ErrMsg("Entity is not initialized!");
		return false;
	}

	for (auto &behaviour : _behaviours)
	{
		if (!behaviour.get()->InitialOnDebugSelect())
		{
			ErrMsgF("InitialOnDebugSelect() failed for behaviour '{}'!", behaviour->GetName());
			return false;
		}
	}

	return true;
}

bool Entity::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj, bool forceSerialize)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!IsSerializable() && !forceSerialize)
		return true; // Skip non-serializable entities

	Entity *parentEntity = GetParent();
	Transform *entTransform = GetTransform();
	dx::XMFLOAT3A pos = entTransform->GetPosition();
	dx::XMFLOAT3A euler = entTransform->GetEuler();
	dx::XMFLOAT3A scale = entTransform->GetScale();

	json::Value nameStr(json::kStringType);
	nameStr.SetString(GetName().c_str(), docAlloc);
	obj.AddMember("Name", nameStr, docAlloc);
	obj.AddMember("ID", GetID(), docAlloc);
	obj.AddMember("Enabled", IsEnabledSelf(), docAlloc);

	json::Value posArr(json::kArrayType);
	posArr.PushBack(pos.x, docAlloc);
	posArr.PushBack(pos.y, docAlloc);
	posArr.PushBack(pos.z, docAlloc);
	obj.AddMember("Pos", posArr, docAlloc);

	json::Value rotArr(json::kArrayType);
	rotArr.PushBack(euler.x, docAlloc);
	rotArr.PushBack(euler.y, docAlloc);
	rotArr.PushBack(euler.z, docAlloc);
	obj.AddMember("Rot", rotArr, docAlloc);

	json::Value scaleArr(json::kArrayType);
	scaleArr.PushBack(scale.x, docAlloc);
	scaleArr.PushBack(scale.y, docAlloc);
	scaleArr.PushBack(scale.z, docAlloc);
	obj.AddMember("Scale", scaleArr, docAlloc);

	if (IsPrefab())
	{
		json::Value prefabStr(json::kStringType);
		prefabStr.SetString(GetPrefabName().c_str(), docAlloc);
		obj.AddMember("Prefab", prefabStr, docAlloc);
	}
	else
	{
		obj.AddMember("Static", IsStatic(), docAlloc);
		obj.AddMember("Select", IsDebugSelectable(), docAlloc);
		obj.AddMember("InTree", GetScene()->GetSceneHolder()->IsEntityIncludedInTree(this), docAlloc);
		obj.AddMember("Hidden", !GetShowInHierarchy(true), docAlloc);

		json::Value behArr(json::kArrayType);
		UINT count = GetBehaviourCount();

		for (int i = 0; i < count; i++)
		{
			Behaviour *beh = GetBehaviour(i);
			json::Value behObj(json::kObjectType);

			if (!beh->InitialSerialize(docAlloc, behObj))
			{
				ErrMsgF("Failed to serialize behaviour '{}'!", beh->GetName());
				return false;
			}

			// Add behaviour to the array if it isn't empty
			if (behObj.MemberCount() > 0)
				behArr.PushBack(behObj, docAlloc);
		}
		obj.AddMember("Beh", behArr, docAlloc);


		json::Value childArr(json::kArrayType);
		const std::vector<Entity *> *children = GetChildren();

		for (auto &child : *children)
		{
			if (!child)
				continue;

			// HACK: Does forceSerialize not being recursive cause problems?
			json::Value childObj(json::kObjectType);
			if (!child->Serialize(docAlloc, childObj))
			{
				ErrMsgF("Failed to serialize child entity '{}'!", child->GetName());
				return false;
			}

			// Add entity to the array if it isn't empty
			if (childObj.MemberCount() > 0)
				childArr.PushBack(childObj, docAlloc);
		}

		// Add the child arrat unless it is empty
		if (!childArr.Empty())
			obj.AddMember("Child", childArr, docAlloc);
	}

	return true;
}