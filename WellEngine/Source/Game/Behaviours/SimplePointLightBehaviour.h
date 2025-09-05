#pragma once

#include <array>
#include "Behaviour.h"
#include "Behaviours/CameraCubeBehaviour.h"

class PointLightCollection;

struct SimplePointLightBufferData
{
	dx::XMFLOAT3 position = { };
	float falloff = 0.0f;

	dx::XMFLOAT3 color = { };
	float fogStrength = 1.0f;
};

class [[register_behaviour]] SimplePointLightBehaviour final : public Behaviour
{
private:
	dx::XMFLOAT3 _color = { 1.0f, 1.0f, 1.0f };
	float _falloff = 1.0f;
	float _fogStrength = 1.0f;

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

public:
	SimplePointLightBehaviour() = default;
	SimplePointLightBehaviour(dx::XMFLOAT3 color, float falloff, float fogStrength) : _color(color), _falloff(falloff), _fogStrength(fogStrength) {}
	~SimplePointLightBehaviour();

	[[nodiscard]] SimplePointLightBufferData GetLightBufferData() const;
	void SetLightBufferData(dx::XMFLOAT3 color, float falloff, float fogStrength);

	[[nodiscard]] bool ContainsPoint(const dx::XMFLOAT3A &point) const;
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingFrustum &tile) const;
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingOrientedBox &tile) const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

};
