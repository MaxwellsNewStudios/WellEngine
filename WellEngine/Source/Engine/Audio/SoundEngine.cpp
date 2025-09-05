#include "stdafx.h"
#include "Audio/SoundEngine.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool SoundEngine::Initialize(dx::AUDIO_ENGINE_FLAGS flags, dx::AUDIO_ENGINE_REVERB reverb, float gameVolume)
{
#ifdef DEBUG_BUILD
	flags |= dx::AudioEngine_Debug;
#endif
	_soundEngine = std::make_unique<dx::AudioEngine>(flags);
	_soundEngine->SetReverb(reverb);

	if (!_soundEngine)
	{
		ErrMsg("Failed to initialize audio engine");
		return false;
	}

	SetVolume(gameVolume);
	_initialized = true;

	return true;
}

bool SoundEngine::IsInitialized()
{
	return _initialized;
}

bool SoundEngine::Update()
{
	if (!_soundEngine->Update())
	{
		if (_soundEngine->IsCriticalError())
		{
			// No audio device is active
			ErrMsg("No audio device is active!");
			return false;
		}

		// Failed to update
		ErrMsg("Failed to update audio!");
		return false;
	}
	return true;
}

dx::AudioEngine *SoundEngine::GetAudioEngine()
{
	return _soundEngine.get();
}

void SoundEngine::Suspend()
{
	_soundEngine->Suspend();
}

void SoundEngine::Resume()
{
	_soundEngine->Resume();
}

void SoundEngine::SetVolume(float volume)
{
	_soundEngine->SetMasterVolume(volume);
}

void SoundEngine::ResetSoundEngine()
{
	_initialized = false;
	_soundEngine = nullptr;
}
