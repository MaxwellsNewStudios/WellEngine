#pragma once
#include "Behaviour.h"

class [[register_behaviour]] RestrictedViewBehaviour final : public Behaviour
{
private:
	dx::XMFLOAT3A _startOrientation = { 0.0f, 0.0f, 0.0f };
	dx::XMFLOAT3A _viewDirection = { 0.0f, 0.0f, 1.0f };
	dx::XMFLOAT3A _upDirection = { 0.0f, 1.0f, 0.0f };

	float _allowedDotOffset = 1.0f;
	float _offsetInDeg = 90.0f;
	float _prevOffset = 1.0f;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input& input) override;

public:
	RestrictedViewBehaviour() = default;
	~RestrictedViewBehaviour() = default;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void SetViewDirection(const dx::XMFLOAT3 &orientation, const dx::XMFLOAT3 &viewDirection, const dx::XMFLOAT3 &upDirection);
	void SetAllowedOffset(float offsetDegrees);
};