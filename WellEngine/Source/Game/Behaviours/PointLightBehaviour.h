#pragma once

#include <array>
#include "Behaviour.h"
#include "Behaviours/CameraCubeBehaviour.h"

class PointLightCollection;

struct PointLightBufferData
{
	dx::XMFLOAT3 position = { };
	float falloff = 0.0f;

	dx::XMFLOAT3 color = { };
	float fogStrength = 1.0f;

	float nearZ = 0.0f, farZ = 1.0f;

	float padding[2]{};
};

class [[register_behaviour]] PointLightBehaviour final : public Behaviour
{
private:
	CameraPlanes _initialCameraPlanes = { 0.1f, 1.0f };
	dx::XMFLOAT3 _color = { 1.0f, 1.0f, 1.0f };
	float _falloff = 1.0f;
	float _fogStrength = 1.0f;

	UINT _updateFrequency = 2;
	int _updateTimer = -1;

	CameraCubeBehaviour *_shadowCameraCube = nullptr;
	PointLightBufferData _lastLightBufferData = {};

	dx::BoundingBox _transformedBounds = { };

	bool _boundsDirty = true;

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void OnEnable() override;

	void OnDisable() override;


public:
	PointLightBehaviour() = default;
	PointLightBehaviour(CameraCubeBehaviour *cameraCube, dx::XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency = 3);
	PointLightBehaviour(CameraPlanes planes, dx::XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency = 3);
	~PointLightBehaviour();

	void SetUpdateTimer(UINT timer);
	[[nodiscard]] bool DoUpdate() const;
	[[nodiscard]] bool UpdateBuffers();

	[[nodiscard]] PointLightBufferData GetLightBufferData();
	void SetLightBufferData(dx::XMFLOAT3 color, float falloff, float fogStrength);

	[[nodiscard]] CameraCubeBehaviour *GetShadowCameraCube() const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	[[nodiscard]] bool ContainsPoint(const dx::XMFLOAT3A &point);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingFrustum &tile);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingOrientedBox &tile);
};
