#include "stdafx.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#include "Behaviours/MeshBehaviour.h"
#include "BehaviourFactory.h"

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
	_cullingTreePaths = std::move(other._cullingTreePaths);
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

	for (int i = 0; i < _behaviours.size(); i++)
	{
		Behaviour *beh = _behaviours[i].get();

		if (beh != behaviour)
			continue;

		_behaviours.erase(_behaviours.begin() + i);
	}
}
Behaviour *Entity::GetBehaviour(UINT index) const
{
	if (index >= _behaviours.size())
		return nullptr;
	return _behaviours[index].get();
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
		ErrMsg("Cannot parent an entity to itself!");
		return;
	}

	// Check if new parent is a child of this entity
	if (newParent)
	{
		Entity *parentInHierarchy = newParent->GetParent();
		while (parentInHierarchy)
		{
			if (parentInHierarchy == this)
			{
				ErrMsg("Cannot parent an entity to it's child! (Did you mean to unparent the child first?)");
				return;
			}

			parentInHierarchy = parentInHierarchy->GetParent();
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
bool Entity::IsChildOf(const Entity *ent) const
{
	if (!ent)
		return false;

	if (this == ent)
		return false;

	Entity *parentIter = _parent;
	while (parentIter)
	{
		if (parentIter == ent)
			return true;

		parentIter = parentIter->GetParent();
	}

	return false;
}
bool Entity::IsParentOf(const Entity *ent) const
{
	return ent->IsChildOf(this);
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

bool Entity::GetShowInHierarchy() const
{
	return _showInHierarchy;
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
bool Entity::InitialRenderUI()
{
	DebugPlayerBehaviour *debugPlayer = _scene->GetDebugPlayer();
	SceneHolder *sceneHolder = _scene->GetSceneHolder();
	int entIndex = sceneHolder->GetEntityIndex(this);
	int entID = _entityID;

	// Entity Header
	{
		bool entEnabled = IsEnabled();
		if (ImGui::Checkbox("##EntEnabled", &entEnabled))
		{
			if (entEnabled) Enable();
			else			Disable();
		}
		ImGui::SetItemTooltip("Enable/Disable Entity");

		std::string entName = GetName();
		ImGui::SameLine();
		if (ImGui::InputText("##EntName", &entName))
			SetName(entName);
		ImGui::SetItemTooltip("Entity Name");

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
	}

	ImGui::Separator();

	// Buttons
	{
		// Dock/Undock button
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.6f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.6f, 0.75f, 0.7f));

			const std::string windowID = std::format("Ent#{}", entID);

			// Check if entity is undocked
			if (ImGuiUtils::Utils::GetWindow(windowID, nullptr))
			{
				// If undocked, show dock button
				if (ImGui::Button("Dock", { 60, 20 }))
				{
					if (!ImGuiUtils::Utils::CloseWindow(windowID))
					{
						ImGui::PopStyleColor(3);
						ErrMsg("Failed to dock entity window!");
						return false;
					}
				}
			}
			else
			{
				// If docked, show undock button
				if (ImGui::Button("Undock", { 60, 20 }))
				{
					const std::string windowName = std::format("Entity '{}'", GetName());
					if (!ImGuiUtils::Utils::OpenWindow(windowName, windowID, std::bind(&Entity::InitialRenderUI, this)))
					{
						ImGui::PopStyleColor(3);
						ErrMsg("Failed to undock entity window!");
						return false;
					}
				}
			}

			ImGui::PopStyleColor(3);
		}

		ImGui::SameLine();

		// Copy button
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.3f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.3f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.3f, 0.75f, 0.7f));
			if (ImGui::Button("Copy", { 60, 20 }))
			{
				// Copy by serializing and deserializing the entity
				json::Document doc;
				json::Value entObj = json::Value(json::kObjectType);

				if (!_scene->SerializeEntity(doc.GetAllocator(), entObj, this, true))
				{
					ErrMsg("Failed to serialize entity!");
					ImGui::PopStyleColor(3);
					return false;
				}

				Entity *ent = nullptr;
				if (!_scene->DeserializeEntity(entObj, &ent))
				{
					ErrMsg("Failed to deserialize entity!");
					ImGui::PopStyleColor(3);
					return false;
				}

				_scene->RunPostDeserializeCallbacks();

				debugPlayer->Select(ent, ImGui::GetIO().KeyShift);
			}
			ImGui::PopStyleColor(3);
		}

		ImGui::SameLine();

		// Delete button
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.75f, 0.7f));
			if (ImGui::Button("Delete", { 60, 20 }))
			{
				debugPlayer->Deselect(this);

				if (!sceneHolder->RemoveEntity(entIndex))
				{
					ErrMsg("Failed to remove entity!");
					ImGui::PopStyleColor(3);
					return false;
				}
			}
			ImGui::PopStyleColor(3);
		}

		// Duplicate bind button
		{
			ImVec2 buttonSize = { 196, 20 };

			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.15f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.15f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.15f, 0.75f, 0.7f));
			if (debugPlayer->HasDuplicateBind(entID))
			{
				if (ImGui::Button("Clear Duplicate Bind", buttonSize))
					debugPlayer->RemoveDuplicateBind(entID);

				ImGui::PopStyleColor(3);

				KeyCode keyCode = debugPlayer->GetDuplicateBind(entID);
				std::string keyCodeName = "?";

				for (const auto &[key, value] : KeyCodeNames)
				{
					if (value == keyCode)
					{
						keyCodeName = key;
						break;
					}
				}

				ImGui::Text(("Duplicate Bind: " + keyCodeName).c_str());
			}
			else
			{
				if (debugPlayer->IsAssigningDuplicateToKey(entID))
				{
					if (ImGui::Button("Cancel Duplicate Bind", buttonSize))
						debugPlayer->AssignDuplicateToKey(-1);
					ImGui::PopStyleColor(3);

					ImGui::Text("Press the key you want to assign...");
				}
				else if (ImGui::Button("Add Duplicate Bind", buttonSize))
				{
					debugPlayer->AssignDuplicateToKey(entID);
					ImGui::PopStyleColor(3);
				}
				else
					ImGui::PopStyleColor(3);
			}
		}

		// Save as prefab button
		{
			static std::string prefabName = "";
			bool doSave = false;

			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.8f, 0.625f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.8f, 0.725f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.8f, 0.825f, 0.7f));
			if (ImGui::Button("Save as Prefab", { 196, 20 }))
			{
				ImGui::OpenPopup("Save as Prefab");
				prefabName = GetName();
			}
			ImGui::PopStyleColor(3);

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
								break;

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

		// Replace with prefab button
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.1f, 0.55f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.1f, 0.65f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.1f, 0.75f, 0.7f));
			if (ImGui::Button("Replace With Prefab", { 196, 20 }))
				ImGui::OpenPopup("Replace With Prefab");
			ImGui::PopStyleColor(3);

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
	}

	ImGui::Separator();

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

		ImGui::Checkbox("Static", &_isStatic);
		ImGui::Checkbox("Selectable", &_isDebugSelectable);
		ImGui::Checkbox("Serialize##EntSerialize", &_doSerialize);

		ImGui::TreePop();
	}

	ImGui::Separator();

	ImGui::PushID("Transform");
	if (ImGui::CollapsingHeader("Transform"))
	{
		if (!_transform.RenderUI(debugPlayer->GetEditSpace()))
		{
			ErrMsg("Failed to render transform UI!");
			ImGui::PopID();
			return false;
		}
	}
	ImGui::PopID();

	ImGui::PushID("Behaviour List");
	if (ImGui::CollapsingHeader("Behaviours"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;
		childFlags |= ImGuiChildFlags_ResizeY;

		for (int i = 0; i < _behaviours.size(); i++)
		{
			auto &behaviour = _behaviours[i];
			ImGui::PushID(("Behaviour " + std::to_string(i)).c_str());
			if (ImGui::TreeNode(behaviour.get()->GetName().c_str()))
			{
				ImGui::BeginChild("Behaviour", ImVec2(0, 0), childFlags);
				if (!behaviour.get()->InitialRenderUI())
				{
					ErrMsg("Failed to render behaviour UI!");
					ImGui::EndChild();
					ImGui::TreePop();
					ImGui::PopID();
					return false;
				}
				ImGui::EndChild();
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		ImGui::Dummy({ 0.0f, 4.0f });

		if (ImGui::TreeNode("Add Behaviour"))
		{
			auto &behaviourMap = BehaviourRegistry::Get();

			// Get vector of behaviour names from map
			std::vector<std::string_view> behaviourNames;
			behaviourNames.reserve(behaviourMap.size());

			for (const auto &pair : behaviourMap)
				behaviourNames.push_back(pair.first);

			// Sort the behaviour names
			std::sort(behaviourNames.begin(), behaviourNames.end());

			// Render the combo box for behaviour selection
			static int selectedIndex = 0;
			if (ImGui::BeginCombo("Behaviour Type", behaviourNames[selectedIndex].data()))
			{
				for (int i = 0; i < behaviourNames.size(); i++)
				{
					const bool isSelected = (selectedIndex == i);
					if (ImGui::Selectable(behaviourNames[i].data(), isSelected))
						selectedIndex = i;

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (ImGui::Button("Add Behaviour"))
			{
				const std::string_view behaviourName = behaviourNames[selectedIndex];

				auto it = behaviourMap.find(behaviourName);
				if (it == behaviourMap.end())
				{
					ErrMsgF("Behaviour '{}' not found in map!", behaviourName);
					return false;
				}

				std::function<Behaviour *()> behaviourConstructor = it->second;
				Behaviour *newBehaviour = behaviourConstructor();

				if (!newBehaviour->Initialize(this))
				{
					ErrMsg("Failed to bind behaviour to entity!");
					return false;
				}
			}

			ImGui::TreePop();
			ImGui::Separator();
		}
	}
	ImGui::PopID();

	return true;
}
#endif

bool Entity::InitialBindBuffers(ID3D11DeviceContext *context)
{
	//if (!_transform.UpdateConstantBuffer(GetScene()->GetContext()))
	//{
	//	ErrMsg("Failed to update entity constant buffers!");
	//	return false;
	//}

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
