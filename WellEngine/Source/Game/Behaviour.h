#pragma once

#include <d3d11.h>
#include "EngineSettings.h"
#include "Timing/TimeUtils.h"
#include "Input/Input.h"
#include "Transform.h"
#include "Rendering/RendererInfo.h"
#include "rapidjson/document.h"

namespace dx = DirectX;
namespace json = rapidjson;

class Game;
class Scene;
class Entity;
class CameraBehaviour;
class RenderQueuer;

class Behaviour : public IRefTarget<Behaviour>
{
private:
	bool _isInitialized = false;
	bool _isDestroyed = false;
	bool _isEnabledSelf = true;
	bool _doSerialize = true;

	Entity *_entity = nullptr;

protected:
	std::string _name = "";

	void QueueUpdate();
	void DequeueUpdate();

	void QueueParallelUpdate();
	void DequeueParallelUpdate();

	void QueueLateUpdate();
	void DequeueLateUpdate();

	void QueueFixedUpdate();
	void DequeueFixedUpdate();

	// Start runs once when the behaviour is created.
	[[nodiscard]] virtual bool Start();

	// Update runs every frame.
	[[nodiscard]] virtual bool Update(TimeUtils &time, const Input &input);
	
	// ParallelUpdate runs after update and exeutes in parallel with all other behaviours, so one must ensure thread safety between behaviours.
	[[nodiscard]] virtual bool ParallelUpdate(const TimeUtils &time, const Input &input);
	
	// Like Update, but later.
	[[nodiscard]] virtual bool LateUpdate(TimeUtils &time, const Input &input);

	// FixedUpdate runs every physics update (20hz by default).
	[[nodiscard]] virtual bool FixedUpdate(float deltaTimeUtils, const Input &input);

	// Render runs for all objects queued for rendering before they are rendered.
	[[nodiscard]] virtual bool BeforeRender();

	// Render runs when objects are being queued for rendering.
	[[nodiscard]] virtual bool Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo);


#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] virtual bool RenderUI();
#endif

	// BindBuffers runs before drawcalls pertaining to the Entity are performed.
	[[nodiscard]] virtual bool BindBuffers(ID3D11DeviceContext *context);

	// OnEnable runs immediately after the behaviour is enabled.
	virtual void OnEnable();

	// OnEnable runs immediately after the behaviour is disabled.
	virtual void OnDisable();

	// OnDirty runs when the Entity's transform is modified.
	virtual void OnDirty();

	// Serializes the behaviour to a string.
	[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

	// Deserializes the behaviour from a string.
	[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene);

	virtual void PostDeserialize();

	[[nodiscard]] virtual bool OnHover();
	[[nodiscard]] virtual bool OffHover();
	[[nodiscard]] virtual bool OnSelect();
	[[nodiscard]] virtual bool OnDebugSelect();

public:
	Behaviour() = default;
	virtual ~Behaviour();

	[[nodiscard]] bool Initialize(Entity *entity, const std::string &behaviourName = "");
	[[nodiscard]] bool IsInitialized() const;
	[[nodiscard]] bool IsDestroyed() const;

	void SetSerialization(bool state);
	[[nodiscard]] bool IsSerializable() const;

	[[nodiscard]] Entity *GetEntity() const;
	[[nodiscard]] Transform *GetTransform() const;
	[[nodiscard]] Scene *GetScene() const;
	[[nodiscard]] Game *GetGame() const;

	void SetName(const std::string &name);
	[[nodiscard]] const std::string &GetName() const;

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

	[[nodiscard]] bool IsEnabled() const;
	[[nodiscard]] bool IsEnabledSelf() const;
	void InheritEnabled(bool state);
	void SetEnabled(bool state);
	void SetDirty();

	[[nodiscard]] bool InitialSerialize(json::Document::AllocatorType &docAlloc, json::Value &obj);
	[[nodiscard]] bool InitialDeserialize(const json::Value &obj, Scene *scene);
	void InitialPostDeserialize();

	TESTABLE()
};