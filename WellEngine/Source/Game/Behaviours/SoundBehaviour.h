#pragma once
#include <string>
#include "Behaviour.h"
#include "Content/Content.h"
#include "Audio/SoundSource.h"

class [[register_behaviour]] SoundBehaviour : public Behaviour
{
private:
	SoundSource _soundSource;
	std::string _fileName = "";
	dx::SOUND_EFFECT_INSTANCE_FLAGS _soundEffectFlag = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters;

	dx::XMFLOAT3 _listenerPos = { 0.0f, 0.0f, 0.0f };
	dx::XMFLOAT3 _emitterPos = { 0.0f, 0.0f, 0.0f };

	float _volume = 1.0f;
	float _distanceScaler = 75.0f;
	float _reverbScaler = 1.0f;
	bool _loop = false;
	bool _play = true;
	bool _isValid = false;

	float _length = 0.0f;
	float _duration = 0.0f;

	void UpdatePosition();

protected:
	// Start runs once when the behaviour is created.
	[[nodiscard]] bool Start() override;

	// Update runs every frame.
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

	// OnEnable runs immediately after the behaviour is enabled.
	void OnEnable() override;

	// OnEnable runs immediately after the behaviour is disabled.
	void OnDisable() override;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

public:
	SoundBehaviour() = default;
	SoundBehaviour(std::string fileName, 
		dx::SOUND_EFFECT_INSTANCE_FLAGS flags = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters, 
		bool loop = false, float distanceScaler = 75.0f, float reverbScaler = 1.0f);
	~SoundBehaviour() = default;

	void Play();
	void Pause();
	void ResetSound();

	[[nodiscard]] float GetSoundLength() const;
	[[nodiscard]] dx::SoundState GetSoundState();

	[[nodiscard]] dx::XMFLOAT3 GetListenerPosition() const;
	[[nodiscard]] dx::XMFLOAT3 GetEmitterPosition() const;

	void SetListenerPosition(dx::XMFLOAT3 position);
	void SetEmitterPosition(dx::XMFLOAT3 position);

	void SetVolume(float volume);
	void SetLoop(bool state);
	void SetSoundEffectFlag(dx::SOUND_EFFECT_INSTANCE_FLAGS flag);
};

