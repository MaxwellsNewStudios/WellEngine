#pragma once

#include "Behaviour.h"
#include "Behaviours/CameraBehaviour.h"

struct SimpleSpotLightBufferData
{
	dx::XMFLOAT3 position = { };
	float angle = 0.0f;

	dx::XMFLOAT3 direction = { };
	float falloff = 0.0f;

	dx::XMFLOAT3 color = { };
	int orthographic = -1;

	float fogStrength = 1.0f;
	float padding[3]{};
};

class [[register_behaviour]] SimpleSpotLightBehaviour final : public Behaviour
{
private:
	dx::XMFLOAT3 _color = { 1.0f, 1.0f, 1.0f };
	float _falloff = 1.0f;
	float _angle = dx::XM_PI;
	bool _isOrtho = false;
	float _fogStrength = 1.0f;

	dx::BoundingFrustum _bounds = { };
	bool _recalculateBounds = true;

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

	// OnEnable runs immediately after the behaviour is enabled.
	void OnEnable() override;

	// OnEnable runs immediately after the behaviour is disabled.
	void OnDisable() override;

	void OnDirty() override;

public:
	SimpleSpotLightBehaviour() = default;
	SimpleSpotLightBehaviour(dx::XMFLOAT3 color, float angle, float falloff, bool isOrtho, float fogStrength) :
		_color(color), _angle(angle), _falloff(falloff), _isOrtho(isOrtho), _fogStrength(fogStrength) {};
	~SimpleSpotLightBehaviour();

	[[nodiscard]] SimpleSpotLightBufferData GetLightBufferData() const;
	void SetLightBufferData(dx::XMFLOAT3 color, float angle, float falloff, bool isOrtho, float fogStrength);

	[[nodiscard]] bool ContainsPoint(const dx::XMFLOAT3A &point);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingFrustum &tile);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingOrientedBox &tile);

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

};
