#pragma once

#include "Behaviour.h"
#include "Transform.h"
#include "Behaviours/CameraBehaviour.h"
#include "Rendering/RenderQueuer.h"
#include "Collision/Raycast.h"
#include "rapidjson/document.h"

namespace dx = DirectX;
namespace json = rapidjson;

class BillboardMeshBehaviour;

class [[register_behaviour]] DebugPlayerBehaviour final : public Behaviour, public IRefTarget<DebugPlayerBehaviour>
{
#ifdef DEBUG_BUILD
private:
	Ref<CameraBehaviour> _mainCamera{};
	Ref<CameraBehaviour> _secondaryCamera{};
	Ref<CameraBehaviour> _currCameraPtr{};

	int _currCamera = -2;
	std::vector<Ref<Entity>> _currSelection;
	bool _rayCastFromMouse = false;

	bool _useMainCamera = true;
	bool _drawPointer = false;

	Ref<Entity> _cursorPositioningTarget{};
	bool _includePositioningTargetInTree = false;

	std::vector<std::pair<KeyCode, UINT>> _duplicateBinds = {};
	int _addDuplicateBindForEntity = -1;

	std::vector<BillboardMeshBehaviour *> _gizmoBillboards = {};

	[[nodiscard]] bool UpdateGlobalEntities(TimeUtils &time, const Input &input);

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
	[[nodiscard]] bool Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo) override;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void PostDeserialize() override;

public:
	DebugPlayerBehaviour() = default;
	~DebugPlayerBehaviour() = default;

	void SetCamera(CameraBehaviour *cam);

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

	void AssignDuplicateToKey(UINT id);
	bool IsAssigningDuplicateToKey(UINT id) const;
	bool IsValidDuplicateBind(KeyCode key) const;
	void AddDuplicateBind(KeyCode key, UINT id);
	void RemoveDuplicateBind(UINT id);
	bool HasDuplicateBind(UINT id) const;
	KeyCode GetDuplicateBind(UINT id);
	void ClearDuplicateBinds();

	void PositionWithCursor(Entity *ent);

	void AddGizmoBillboard(BillboardMeshBehaviour *gizmo);
	void RemoveGizmoBillboard(BillboardMeshBehaviour *gizmo);

	void UpdateGizmoBillboards();
#else
public:
	DebugPlayerBehaviour() = default;
#endif
};
