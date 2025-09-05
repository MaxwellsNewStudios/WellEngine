#pragma once
#include <type_traits>
#include "Transform.h"
#include "Timing/TimeUtils.h"
#include "Input/Input.h"
#include "Rendering/Graphics.h"
#include "Behaviour.h"
#include "Rendering/Culling/NodePath.h"
#include "Rendering/RenderQueuer.h"
#include "Collision/Colliders.h"

namespace dx = DirectX;

// Forward declarations
class Game;
class Scene;
struct SceneEntity;

class Entity : public IRefTarget<Entity>
{
private:
	UINT _entityID = -1;
	UINT _deserializedID = -1;
	std::string _name = "";
	std::string _prefabName = "";

	bool _isRemoved = false;
	bool _showInHierarchy = true;
	bool _isStatic = false;
	bool _isDebugSelectable = true;
	bool _recalculateBounds = true;
	bool _doSerialize = true;
	std::atomic_bool _doRender = false;
	UINT _inheritedDisabled = 0;

	bool _skipInRaycast = false;

	dx::BoundingOrientedBox _bounds = {{0,0,0}, {1,1,1}, {0,0,0,1}};
	dx::BoundingOrientedBox _transformedBounds = {{0,0,0}, {1,1,1}, {0,0,0,1}};
	dx::BoundingOrientedBox _lastTransformedCullingBounds = _transformedBounds;

	void IncrementDisable();
	void DecrementDisable();
	void SetInheritedDisableCount(UINT count);

protected:
	bool _isInitialized = false;
	bool _isEnabled = true;
	bool _recalculateCollider = true;

	Scene *_scene = nullptr;
	Transform _transform;
	Entity *_parent = nullptr;
	std::vector<Entity *> _children;
	std::vector<std::unique_ptr<Behaviour>> _behaviours;

	// Tracks all paths in the culling tree this entity is currently placed in 
	std::vector<TreePath> _cullingTreePaths;

	inline void AddChild(Entity *child, bool keepWorldTransform = false);
	inline void RemoveChild(Entity *child, bool keepWorldTransform = false);

public:
	Entity(UINT id, const dx::BoundingOrientedBox &bounds) : _entityID(id), _bounds(bounds) {}
	virtual ~Entity();
	Entity(const Entity &other) = delete;
	Entity &operator=(const Entity &other) = delete;
	Entity(Entity &&other) noexcept; // Move constructor, internal use only, DO NOT USE
	Entity &operator=(Entity &&other) noexcept; // Move assignment operator, internal use only, DO NOT USE

	[[nodiscard]] bool Initialize(ID3D11Device *device, Scene *scene, const std::string &name);
	[[nodiscard]] bool IsInitialized() const;

	void SetSerialization(bool state);
	[[nodiscard]] bool IsSerializable() const;

	[[nodiscard]] bool IsEnabled() const;
	[[nodiscard]] bool IsEnabledSelf() const;

	void SetStatic(bool state);
	[[nodiscard]] bool IsStatic() const;

	void SetDebugSelectable(bool state);
	[[nodiscard]] bool IsDebugSelectable() const;

	void SetRaycastTarget(bool state);
	[[nodiscard]] bool IsRaycastTarget() const;

	void AddBehaviour(Behaviour *behaviour);
	void RemoveBehaviour(Behaviour *behaviour);
	[[nodiscard]] Behaviour *GetBehaviour(UINT index) const;
	[[nodiscard]] const std::vector<std::unique_ptr<Behaviour>> *GetBehaviours() const;
	[[nodiscard]] UINT GetBehaviourCount() const;
	template <class T>
	bool HasBehaviourOfType() const;
	template <class T>
	bool GetBehaviourByType(T *&behaviour) const;
	template <class T>
	bool GetBehaviourByType(Behaviour *&behaviour) const;
	template <class T>
	bool GetBehavioursByType(std::vector<T *> &behaviours) const;
	template <class T>
	bool GetBehavioursByType(std::vector<Behaviour *> &behaviours) const;

	void Enable();
	void Disable();
	void SetEnabledSelf(bool state);

	void SetDirty();
	void SetDirtyImmediate();

	void MarkAsRemoved();
	[[nodiscard]] bool IsRemoved() const;

	[[nodiscard]] bool IsPrefab() const;
	[[nodiscard]] const std::string &GetPrefabName() const;
	void SetPrefabName(const std::string &name);
	void UnlinkFromPrefab();

	void SetParent(Entity *parent, bool keepWorldTransform = false);
	[[nodiscard]] Entity *GetParent() const;
	[[nodiscard]] UINT GetChildCount() const;
	[[nodiscard]] const std::vector<Entity *> *GetChildren() const;
	void GetChildrenRecursive(std::vector<Entity *> &children) const;
	[[nodiscard]] bool IsChildOf(const Entity *ent) const;
	[[nodiscard]] bool IsParentOf(const Entity *ent) const;

	void SetScene(Scene *scene);
	// If this is called from a class with a circular dependency to Scene, you'll have to explicity include Scene.h from the caller.
	[[nodiscard]] Scene *GetScene() const;

	Game *GetGame() const;

	[[nodiscard]] Transform *GetTransform();

	void SetName(const std::string &name);
	[[nodiscard]] const std::string &GetName() const;

	[[nodiscard]] UINT GetID() const;

	[[nodiscard]] UINT GetDeserializedID() const;
	void SetDeserializedID(UINT id);

	[[nodiscard]] bool GetShowInHierarchy() const;
	void SetShowInHierarchy(bool show);

	[[nodiscard]] bool HasBounds(bool includeTriggers, dx::BoundingOrientedBox &out);
	void SetEntityBounds(dx::BoundingOrientedBox &bounds);
	void StoreEntityBounds(dx::BoundingOrientedBox &bounds, ReferenceSpace space = ReferenceSpace::World);

	[[nodiscard]] const dx::BoundingOrientedBox &GetLastCullingBounds() const;
	void UpdateCullingBounds();

	[[nodiscard]] bool InitialUpdate(TimeUtils &time, const Input &input);
	[[nodiscard]] bool InitialParallelUpdate(const TimeUtils &time, const Input &input);
	[[nodiscard]] bool InitialLateUpdate(TimeUtils &time, const Input &input);
	[[nodiscard]] bool InitialFixedUpdate(float deltaTime, const Input &input);
	[[nodiscard]] bool InitialBeforeRender();
	[[nodiscard]] bool InitialRender(const RenderQueuer &queuer, const RendererInfo &rendererInfo);
#ifdef USE_IMGUI
	[[nodiscard]] bool InitialRenderUI();
#endif
	[[nodiscard]] bool InitialBindBuffers(ID3D11DeviceContext *context);
	[[nodiscard]] bool InitialOnHover();
	[[nodiscard]] bool InitialOffHover();
	[[nodiscard]] bool InitialOnSelect();
	[[nodiscard]] bool InitialOnDebugSelect();

	TESTABLE()
};

template<class T>
inline bool Entity::HasBehaviourOfType() const
{
	if (_isRemoved)
		return false;

	if (!std::is_base_of<Behaviour, T>::value)
		return false;

	UINT behaviourCount = static_cast<UINT>(_behaviours.size());
	for (UINT i = 0; i < behaviourCount; i++)
	{
		T *castedBehaviour = dynamic_cast<T*>(_behaviours[i].get());

		if (castedBehaviour)
			return true;
	}

	return false;
}

template<class T>
inline bool Entity::GetBehaviourByType(T *&behaviour) const
{
	if (_isRemoved)
		return false;

	if (!std::is_base_of<Behaviour, T>::value)
		return false;

	UINT behaviourCount = static_cast<UINT>(_behaviours.size());
	for (UINT i = 0; i < behaviourCount; i++)
	{
		T *castedBehaviour = dynamic_cast<T*>(_behaviours[i].get());

		if (!castedBehaviour)
			continue;

		behaviour = castedBehaviour;
		return true;
	}

	behaviour = nullptr;
	return false;
}

template<class T>
inline bool Entity::GetBehaviourByType(Behaviour *&behaviour) const
{
	if (_isRemoved)
		return false;

	if (!std::is_base_of<Behaviour, T>::value)
		return false;

	UINT behaviourCount = static_cast<UINT>(_behaviours.size());
	for (UINT i = 0; i < behaviourCount; i++)
	{
		T *castedBehaviour = dynamic_cast<T *>(_behaviours[i].get());

		if (!castedBehaviour)
			continue;

		behaviour = _behaviours[i].get();
		return true;
	}

	behaviour = nullptr;
	return false;
}

template<class T>
inline bool Entity::GetBehavioursByType(std::vector<T*> &behaviours) const
{
	if (_isRemoved)
		return false;

	if (!std::is_base_of<Behaviour, T>::value)
		return false;
	
	bool found = false;

	UINT behaviourCount = static_cast<UINT>(_behaviours.size());
	for (UINT i = 0; i < behaviourCount; i++)
	{
		T *castedBehaviour = dynamic_cast<T*>(_behaviours[i].get());

		if (!castedBehaviour)
			continue;

		behaviours.emplace_back(castedBehaviour);
		found = true;
	}

	return found;
}

template<class T>
inline bool Entity::GetBehavioursByType(std::vector<Behaviour *> &behaviours) const
{
	if (_isRemoved)
		return false;

	if (!std::is_base_of<Behaviour, T>::value)
		return false;
	
	bool found = false;

	UINT behaviourCount = static_cast<UINT>(_behaviours.size());
	for (UINT i = 0; i < behaviourCount; i++)
	{
		T *castedBehaviour = dynamic_cast<T*>(_behaviours[i].get());

		if (!castedBehaviour)
			continue;

		behaviours.emplace_back(_behaviours[i].get());
		found = true;
	}

	return found;
}
