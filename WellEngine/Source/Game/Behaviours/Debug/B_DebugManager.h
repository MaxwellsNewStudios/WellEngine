#pragma once

#include "Game/Transform.h"
#include "Game/Behaviours/Behaviour.h"
#include "Game/Behaviours/Rendering/Camera/B_Camera.h"
#include "Engine/Rendering/RenderQueuer.h"
#include "Engine/Collision/Raycast.h"
#include "rapidjson/document.h"

namespace WellEngine
{
	namespace dx = DirectX;
	namespace json = rapidjson;

	class B_MeshBillboard;

	enum class MouseMovementMode
	{
		None,
		OrbitPan,
		FlyCam
	};

	class [[register_behaviour]] B_DebugManager final : public Behaviour, public IRefTarget<B_DebugManager>
	{
	public:
		std::string_view GetName() const override { return "DebugManager"; }
		std::string_view GetScriptPath() const override { return __FILE__; }

	#ifdef DEBUG_BUILD
	private:
		Ref<B_Camera> _mainCamera{};
		Ref<B_Camera> _secondaryCamera{};
		Ref<B_Camera> _currCameraPtr{};

		int _currCamera = -2;
		std::vector<Ref<Entity>> _currSelection;
		bool _rayCastFromMouse = false;

		bool _useMainCamera = true;
		bool _drawPointer = false;

		Ref<Entity> _cursorPositioningTarget;
		bool _includePositioningTargetInTree = false;

		std::vector<B_MeshBillboard *> _gizmoBillboards = {};

		[[nodiscard]] bool HandleCameraMovement(TimeUtils &time, const Input &input);

		// out contains entity and distance to entity from camera, pos is the coordinates for the ray hit
		[[nodiscard]] bool RayCastFromCamera(RaycastOut &out);	// Casts a ray from _camera in the direction of _camera
		[[nodiscard]] bool RayCastFromCamera(RaycastOut &out, dx::XMFLOAT3A &pos, dx::XMFLOAT3A &dir);
		[[nodiscard]] bool RayCastFromMouse(RaycastOut &out, const Input &input);	// Casts a ray from mouse position on nearplane along the z-axis
		[[nodiscard]] bool RayCastFromMouse(RaycastOut &out, dx::XMFLOAT3A &pos, dx::XMFLOAT3A &dir, const Input &input);

	protected:
		// Start runs once when the behaviour is created.
		[[nodiscard]] bool Start() override;

		// Update runs every frame.
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

		// Render runs every frame when objects are being queued for rendering.
		[[nodiscard]] bool Render(RenderQueuer &queuer, const RendererInfo &rendererInfo) override;

		// Serializes the behaviour to a string.
		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		// Deserializes the behaviour from a string.
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

		[[nodiscard]] bool PostDeserialize() override;

	public:
		B_DebugManager() = default;
		~B_DebugManager() = default;

		void SetCamera(B_Camera *cam);

		[[nodiscard]] bool IsSelected(Entity *ent, UINT *index = nullptr, bool includeBillboard = false) const;

		void Select(UINT id, bool additive = false);
		void Select(Entity *ent, bool additive = false);
		void Select(Entity **ents, UINT count, bool additive = false);

		void Deselect(UINT id);
		void Deselect(Entity *ent);
		void Deselect(Entity **ents, UINT count);
		void DeselectIndex(UINT index);

		void ClearSelection();

		[[nodiscard]] const UINT GetSelectionSize() const;
		[[nodiscard]] Entity *GetPrimarySelection() const;
		[[nodiscard]] const std::vector<Ref<Entity>> &GetSelection();
		void GetParentSelection(std::vector<Ref<Entity>> &selectedParents);

		void SetEditSpace(ReferenceSpace space);
		[[nodiscard]] ReferenceSpace GetEditSpace() const;

		void SetEditType(TransformationType type);
		[[nodiscard]] TransformationType GetEditType() const;

		void SetEditOriginMode(TransformOriginMode mode);
		[[nodiscard]] TransformOriginMode GetEditOriginMode() const;

		Entity *DuplicateEntity(Entity *entity);
		void PositionWithCursor(Entity *ent);

		void AddGizmoBillboard(B_MeshBillboard *gizmo);
		void RemoveGizmoBillboard(B_MeshBillboard *gizmo);

		void UpdateGizmoBillboards();
	#else
	public:
		B_DebugManager() = default;
	#endif
	};

}
