#pragma once

#include <array>
#include "Game/Behaviour.h"
#include "../Camera/CameraCubeBehaviour.h"

class PointLightCollection;

struct PointLightBufferData
{
	dx::XMFLOAT3 position = { };
	float falloff = 0.0f;

	dx::XMFLOAT3 color = { };
	float fogStrength = 1.0f;
	float shadowStrength = 1.0f;

	float nearZ = 0.0f, farZ = 1.0f;

	float padding[1]{};
};

class [[register_behaviour]] PointLightBehaviour final : public Behaviour
{
private:
	CameraPlanes _initialCameraPlanes = { 0.1f, 1.0f };
	dx::XMFLOAT3 _color = { 1.0f, 1.0f, 1.0f };
	float _falloff = 1.0f;
	float _fogStrength = 1.0f;
	float _shadowStrength = 1.0f;

	bool _updateShadows = true;
	UINT _updateFrequency = 3;
	int _updateTimer = 0;

	CameraCubeBehaviour *_shadowCameraCube = nullptr;
	PointLightBufferData _lastLightBufferData = {};

	dx::BoundingBox _transformedBounds = { };

	bool _boundsDirty = true;

#ifdef DEBUG_BUILD
	Ref<Behaviour> _billboardMeshBehaviour;
#endif

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	void OnEnable() override;
	void OnDisable() override;

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;


public:
	PointLightBehaviour() = default;
	PointLightBehaviour(CameraCubeBehaviour *cameraCube, dx::XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency = 3);
	PointLightBehaviour(CameraPlanes planes, dx::XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency = 3);
	~PointLightBehaviour();

	[[nodiscard]] UINT GetUpdateFrequency() const;
	void SetUpdateFrequency(UINT frequency);
	[[nodiscard]] int GetUpdateTimer() const;
	void SetUpdateTimer(int timer);

	void ForceUpdate();
	void MarkUpdated();
	[[nodiscard]] bool DoUpdate() const;
	[[nodiscard]] bool UpdateBuffers();

	[[nodiscard]] PointLightBufferData GetLightBufferData();
	void SetLightBufferData(dx::XMFLOAT3 color, float falloff, float fogStrength, float shadowStrength);

	void SetIntensity(float intensity);
	void SetColor(dx::XMFLOAT3 color);
	void SetFalloff(float falloff);
	void SetFogStrength(float fogStrength);
	void SetShadowStrength(float shadowStrength);

	[[nodiscard]] CameraCubeBehaviour *GetShadowCameraCube() const;

	[[nodiscard]] bool ContainsPoint(const dx::XMFLOAT3A &point);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingFrustum &tile);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingOrientedBox &tile);
};
