/*
	NOTE:
		All non-abstract classes deriving from Behaviour should likely have the [[register_behaviour]] attribute.
		This exposes the behaviour to the behaviour factory. Leaving out the attribute means the behaviour will 
		not be serialized/deserialized, it will also be excluded from the "Add Behaviour" list in the editor.
	
		It makes sense to leave out the attribute for internal/hidden classes that are only created manually 
		in code, but remember to disable serialization for all instances of those classes.
*/

#pragma once

#include <d3d11.h>

#include "Engine/EngineSettings.h"
#include "Engine/Utils/ReferenceHelper.h"
#include "Engine/Timing/TimeUtils.h"
#include "Engine/Input/Input.h"
#include "Engine/Rendering/RendererInfo.h"
#include "Game/Transform.h"
#include "rapidjson/document.h"

namespace WellEngine
{
	namespace dx = DirectX;
	namespace json = rapidjson;

	// Forward declarations
	class Game;
	class Scene;
	class Entity;
	class B_Camera;
	class RenderQueuer;

	class Behaviour : public IRefTarget<Behaviour>
	{
	public:
		virtual std::string_view GetName() const = 0;
		virtual std::string_view GetScriptPath() const = 0;

	private:
		bool _isInitialized = false;
		bool _isDestroyed = false;
		bool _isEnabledSelf = true;
		bool _doSerialize = true;
	#ifdef USE_IMGUI
		int _uiOpen = 0; // -1 = uninitialized, 0 = close, 1 = open
		float _uiMaxSize = -1.0f;
		bool _uiMaximized = true;
		bool _uiSizeDirty = true;
		bool _uiIsResizing = false;
	#endif

		Entity *_entity = nullptr;

	protected:
		void QueueUpdate();
		void DequeueUpdate();

		void QueueParallelUpdate();
		void DequeueParallelUpdate();

		void QueueLateUpdate();
		void DequeueLateUpdate();

		void QueueFixedUpdate();
		void DequeueFixedUpdate();

		void QueuePhysicsUpdate();
		void DequeuePhysicsUpdate();

		// Start runs once when the behaviour is created.
		[[nodiscard]] virtual bool Start();

		// Update runs every frame.
		[[nodiscard]] virtual bool Update(TimeUtils &time, const Input &input);
	
		// ParallelUpdate runs after update and exeutes in parallel with all other behaviours, so one must ensure thread safety between behaviours.
		[[nodiscard]] virtual bool ParallelUpdate(const TimeUtils &time, const Input &input);
	
		// Like Update, but later.
		[[nodiscard]] virtual bool LateUpdate(TimeUtils &time, const Input &input);

		// FixedUpdate runs every fixed update (20hz by default).
		[[nodiscard]] virtual bool FixedUpdate(float deltaTime, const Input &input);

		// PhysicsUpdate runs every physics update (60hz by default).
		[[nodiscard]] virtual bool PhysicsUpdate(float deltaTime);

		// Render runs for all objects queued for rendering before they are rendered.
		[[nodiscard]] virtual bool BeforeRender();

		// Render runs when objects are being queued for rendering.
		[[nodiscard]] virtual bool Render(RenderQueuer &queuer, const RendererInfo &rendererInfo);

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

		// OnEditTransform runs when the Entity's transform is modified in the editor or in code.
		// NOTE: Triggered manually by calling Entity::SignalTransformEdited(). 
		// It is your responsibility to call after editing an entity.
		[[nodiscard]] virtual bool OnEditTransform();

		// OnEditTransformRec runs when the Entity's transform or any of its parents' transforms are modified in the editor or in code.
		// NOTE: Triggered manually by calling Entity::SignalTransformEdited(). 
		// It is your responsibility to call after editing an entity.
		[[nodiscard]] virtual bool OnEditTransformRec();

		// Serializes the behaviour to a string.
		[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

		// Deserializes the behaviour from a string.
		[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene);

		[[nodiscard]] virtual bool PostDeserialize();
		[[nodiscard]] virtual bool OnDebugSelect();

	public:
		Behaviour() = default;
		virtual ~Behaviour();

		[[nodiscard]] bool Initialize(Entity *entity);
		[[nodiscard]] bool IsInitialized() const;

		void Destroy();
		[[nodiscard]] bool IsDestroyed() const;
		void MarkDestroyed();

		void SetSerialization(bool state);
		[[nodiscard]] bool IsSerializable() const;

		[[nodiscard]] Entity *GetEntity() const;
		[[nodiscard]] Transform *GetTransform() const;
		[[nodiscard]] Scene *GetScene() const;
		[[nodiscard]] Game *GetGame() const;

		[[nodiscard]] bool IsEnabled() const;
		[[nodiscard]] bool IsEnabledSelf() const;
		void InheritEnabled(bool state);
		void SetEnabled(bool state);
		void SetDirty();

		[[nodiscard]] bool InitialUpdate(TimeUtils &time, const Input &input);
		[[nodiscard]] bool InitialParallelUpdate(const TimeUtils &time, const Input &input);
		[[nodiscard]] bool InitialLateUpdate(TimeUtils &time, const Input &input);
		[[nodiscard]] bool InitialFixedUpdate(float deltaTime, const Input &input);
		[[nodiscard]] bool InitialPhysicsUpdate(float deltaTime);

		[[nodiscard]] bool InitialBeforeRender();
		[[nodiscard]] bool InitialRender(RenderQueuer &queuer, const RendererInfo &rendererInfo);
		[[nodiscard]] bool InitialBindBuffers(ID3D11DeviceContext *context);

		[[nodiscard]] bool InitialSerialize(json::Document::AllocatorType &docAlloc, json::Value &obj);
		[[nodiscard]] bool InitialDeserialize(const json::Value &obj, Scene *scene);
		[[nodiscard]] bool InitialPostDeserialize();

		[[nodiscard]] bool InitialOnDebugSelect();
		[[nodiscard]] bool InitialOnEditTransform();
		[[nodiscard]] bool InitialOnEditTransformRec();

	#ifdef USE_IMGUI
		// Serialize behaviour to JSON and copy to clipboard
		void CopyToClipboard();

		[[nodiscard]] int PopUIOpenState();
		void SetUIOpen(bool state);

		[[nodiscard]] float GetUISize() const;
		void SetUISize(float maxSize);

		[[nodiscard]] bool GetUIMaximized() const;
		void SetUIMaximized(bool state);

		[[nodiscard]] bool IsResizingUI() const;
		void SetResizingUI(bool state);

		[[nodiscard]] bool IsUIDirty() const;
		void SetUIDirty(bool state);

		[[nodiscard]] bool InitialRenderUI();
	#endif

		TESTABLE
	};
}
