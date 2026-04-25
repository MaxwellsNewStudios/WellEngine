#pragma once

#include <string>
#include <memory>
#include <iostream>

#include "Audio.h"

namespace WellEngine
{
	class SoundEngine
	{
	private:
		bool _initialized = false;
		std::unique_ptr<DirectX::AudioEngine> _soundEngine = nullptr;

	public:
		SoundEngine() = default;
		~SoundEngine() = default;
		bool Initialize(DirectX::AUDIO_ENGINE_FLAGS flags, DirectX::AUDIO_ENGINE_REVERB reverb, float gameVolume);
		bool IsInitialized() const;

		bool Update();

		DirectX::AudioEngine *GetAudioEngine();

		void Suspend();
		void Resume();

		void SetVolume(float volume);

		void ResetSoundEngine();

		TESTABLE
	};
}
