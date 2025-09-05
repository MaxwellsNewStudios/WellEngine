#pragma once
#include "Behaviour.h"
#include "Behaviours/MeshBehaviour.h"

class [[register_behaviour]] BillboardMeshBehaviour : public Behaviour
{
private:
	MeshBehaviour *_meshBehaviour = nullptr;
	Material _material;

	bool _transparent = true;
	bool _castShadows = false;
	bool _overlay = false;
	bool _gizmo = false;

	bool _keepUpright = true;
	float _rotation = 0.0f;
	float _normalOffset = 0.0f;
	float _scale = 1.0f;

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void OnEnable() override;
	void OnDisable() override;

	//[[nodiscard]] bool OnDebugSelect() override;

public:
	BillboardMeshBehaviour() = default;
	~BillboardMeshBehaviour();

	BillboardMeshBehaviour(
		const Material &material, float rotation, float normalOffset, float size, 
		bool keepUpright, bool isTransparent, bool castShadows, bool isOverlay, bool isGizmo = false);

	void SetSize(float size);
	void SetRotation(float rotation);
	MeshBehaviour* GetMeshBehaviour() const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
};