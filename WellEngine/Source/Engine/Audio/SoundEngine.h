#pragma once
#include <string>
#include <memory>
#include <iostream>
#include "Audio.h"

class SoundEngine
{
private:
	bool _initialized = false;
	std::unique_ptr<dx::AudioEngine> _soundEngine = nullptr;

public:
	SoundEngine() = default;
	~SoundEngine() = default;
	bool Initialize(dx::AUDIO_ENGINE_FLAGS flags, dx::AUDIO_ENGINE_REVERB reverb, float gameVolume);
	bool IsInitialized();

	bool Update();

	dx::AudioEngine *GetAudioEngine();

	void Suspend();
	void Resume();

	void SetVolume(float volume);

	void ResetSoundEngine();

	TESTABLE()
};

