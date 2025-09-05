#pragma once
#include "Behaviour.h"
#include "Behaviours/SoundBehaviour.h"
#include "Behaviours/SpotLightBehaviour.h"

/*
* F to toggle flashlight
*/

class [[register_behaviour]] FlashlightBehaviour final : public Behaviour
{
private:
	bool _version = false;

	bool _isOn = false;

	const float _batteryMax = 100.0f;
	float _battery = _batteryMax;
	bool _isDead = false;
	bool _isCharging = false;
	bool _isFlickering = false;
	
	bool _batteryHighTreshHold = false;
	bool _batteryMediumTreshHold = false;
	bool _batteryLowTreshHold = false;

	const float _batteryChargeRate = 10.0f; // Per second
	const float _batteryDrainRate = 1.0f; // Per second

	const float _flickerThreshold = 30.0f;		// Battery percentage at which flashlight starts flickering
	const float _flickerChance = 0.2f;			// Maximum chance to flicker per fixed update
	const float _flickerChanceExponent = 1.75f; // Exponent for calculating the flicker chance based on battery level
	float _flickerTimer = 0.0f;
	float _flickerVolume = 0.0f;

	const float _lightStrengthThreshold = 50.0f; // Battery percentage at which light intensity starts to decrease
	const float _lightFalloffExponent = 1.5f; // Exponent for calculating the intensity's rate of change
	const float _lightStrengthMax = 25.0f; // Intensity of the light wíth chargeat or above _lightStrengthThreshold
	const float _lightStrengthMin = 10.0f; // Intensity of the light immediately before the battery dies

	float _holdTime = 0.0f; // The amount of time the flashlight button has been held to determine if toggle or charge the flashlight
	const float _threshold = 0.3f; // _holdTime < _threshold -> toggle. _holdTime > _threshold -> charge

	const KeyCode _key = KeyCode::F;

	SpotLightBehaviour *_lightBehaviour = nullptr;
	SoundBehaviour *_onSound = nullptr;
	SoundBehaviour *_flickerSound = nullptr;
	SoundBehaviour *_toggleSound = nullptr;
	Transform *_rechargeLever = nullptr;

	void TurnOffIndicator(const std::string &name, bool &off);

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	// FixedUpdate runs every physics update (20hz by default).
	[[nodiscard]] bool FixedUpdate(float deltaTime, const Input &input) override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

public:
	FlashlightBehaviour() = default;
	~FlashlightBehaviour() = default;

	float GetMaxBattery() const;
	float GetBattery() const;
	void DrainBattery();
	void SetBattry(float battery);

	void Charge(TimeUtils &time);

	void SetRechargeLever(Transform *lever);
	[[nodiscard]] SpotLightBehaviour *GetLight() const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	void PostDeserialize() override;

	bool IsOn() const;
	void ToggleFlashlight(bool state);
};
