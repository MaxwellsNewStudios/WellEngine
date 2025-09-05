#pragma once

#include "Behaviour.h"
#include "Collision/Colliders.h"

class [[register_behaviour]] ColliderBehaviour : public Behaviour
{
private:
	Collisions::Collider* _baseCollider = nullptr;
	Collisions::Collider* _transformedCollider = nullptr;

	bool _debugDraw = false;
	bool _isDirty = true;
	bool _isIntersecting = false;
	bool _toCheck = false;

#ifdef USE_IMGUI
	int _selectedColliderType = 0;
#endif

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] bool Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo) override;
#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif
	void OnDirty() override;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

public:
	ColliderBehaviour() = default;
	~ColliderBehaviour();

	// Note: collider vaues should be defined in local-space
	void SetCollider(Collisions::Collider *collider);
	const Collisions::Collider *GetCollider() const;

	void AddOnIntersection(std::function<void(const Collisions::CollisionData &)> callback);
	void AddOnCollisionEnter(std::function<void(const Collisions::CollisionData &)> callback);
	void AddOnCollisionExit(std::function<void(const Collisions::CollisionData &)> callback);

#ifdef DEBUG_BUILD
	[[nodiscard]] bool SetDebugCollider(Scene* scene, const Content *content);
#endif

	void SetIntersecting(bool value);
	bool GetIntersecting();

	void CheckDone();
	bool GetToCheck();
};

