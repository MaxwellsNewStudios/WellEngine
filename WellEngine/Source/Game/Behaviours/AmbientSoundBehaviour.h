#pragma once
#include "Behaviour.h"
#include "Behaviours/SoundBehaviour.h"

class [[register_behaviour]] AmbientSoundBehaviour : public Behaviour
{
private:
	SoundBehaviour *_soundBehaviour = nullptr;
	
	std::string _fileName = "";
	float _volume = 0.5f;
	dx::SOUND_EFFECT_INSTANCE_FLAGS _soundEffectFlag = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters;
	bool _loop = false;
	float _distanceScaler = 75.0f;
	float _reverbScaler = 1.0f;

	float _timer = 0.0f;

	float _delayMin = 2.0f;
	float _delayMax = 10.0f;

protected:
	[[nodiscard]] bool Start() override;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

public:
	AmbientSoundBehaviour() = default;
	AmbientSoundBehaviour(std::string fileName,
		dx::SOUND_EFFECT_INSTANCE_FLAGS flags = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters,
		bool loop = false, float volume = 0.5f, float distanceScaler = 75.0f, float reverbScaler = 1.0f,
		float minimumDelay = 2.0f, float maximumDelay = 10.0f);

	~AmbientSoundBehaviour() = default;

	void TriggerSound();

	[[nodiscard]] SoundBehaviour *GetSoundBehaviour() const;
};