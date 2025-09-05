 #pragma once

#include "Behaviour.h"
#include "Behaviours/CameraBehaviour.h"

struct SpotLightBufferData
{
	dx::XMFLOAT4X4 vpMatrix = { };

	dx::XMFLOAT3 position = { };
	float angle = 0.0f;

	dx::XMFLOAT3 direction = { };
	float falloff = 0.0f;

	dx::XMFLOAT3 color = { };
	int orthographic = -1;

	float fogStrength = 1.0f;
	float padding[3]{};
};

class [[register_behaviour]] SpotLightBehaviour final : public Behaviour
{
private:
	ProjectionInfo _initialProjInfo = { };
	dx::XMFLOAT3 _color = { 1.0f, 1.0f, 1.0f };
	float _falloff = 1.0f;
	float _fogStrength = 1.0f;
	bool _ortho = false;

	UINT _updateFrequency = 2;
	int _updateTimer = 1;

	CameraBehaviour *_shadowCamera = nullptr;
	SpotLightBufferData _lastLightBufferData = { };

	dx::BoundingFrustum _transformedBounds = { };
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
	SpotLightBehaviour() = default;
	SpotLightBehaviour(CameraBehaviour *camera, dx::XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency = 2);
	SpotLightBehaviour(ProjectionInfo projInfo, dx::XMFLOAT3 color, float falloff, float fogStrength, bool isOrtho = false, UINT updateFrequency = 2);
	~SpotLightBehaviour();

	void SetUpdateTimer(UINT timer);
	[[nodiscard]] bool DoUpdate() const;
	[[nodiscard]] bool UpdateBuffers();

	[[nodiscard]] SpotLightBufferData GetLightBufferData();
	void SetLightBufferData(dx::XMFLOAT3 color, float falloff, float fogStrength);
	void SetIntensity(float intensity);

	[[nodiscard]] CameraBehaviour *GetShadowCamera() const;
	
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	
	[[nodiscard]] bool ContainsPoint(const dx::XMFLOAT3A &point);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingFrustum &tile);
	[[nodiscard]] bool IntersectsLightTile(const dx::BoundingOrientedBox &tile);
};
