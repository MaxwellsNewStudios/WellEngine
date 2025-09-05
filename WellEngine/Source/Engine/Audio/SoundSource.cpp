#include "stdafx.h"
#include "Audio/SoundSource.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool SoundSource::Initialize(dx::AudioEngine *audEngine, 
	dx::SOUND_EFFECT_INSTANCE_FLAGS flags, std::string fileName, 
	float distanceScaler, float reverbScaler)
{
	fileName = PATH_FILE_EXT(ASSET_PATH_SOUNDS, fileName, "wav");
	struct stat buffer;   
	if (stat(fileName.c_str(), &buffer) != 0)
	{
		ErrMsg("Failed to load " + fileName + "! File does not exist.");
		return false;
	}

	std::wstring temp = std::wstring(fileName.begin(), fileName.end());
	_sound = std::make_unique<dx::SoundEffect>(audEngine, temp.c_str());
	_soundEffectFlag = flags;
	_effect = _sound->CreateInstance(flags);
	_emitter.ChannelCount = _sound->GetFormat()->nChannels;

	_listener.SetOrientation(dx::XMFLOAT3(0.0f, 0.0f, 1.0f), dx::XMFLOAT3(0.0f, 1.0f, 0.0f));

	_invDistanceScaler = 1.0f / distanceScaler;
	_reverbScaler = reverbScaler;

	// Default
	{
		_listenerCone.InnerAngle = 0;
		_listenerCone.OuterAngle = 0;

		_listenerCone.InnerVolume = _invDistanceScaler;
		_listenerCone.OuterVolume = _invDistanceScaler;

		_listenerCone.InnerLPF = 0.0f;
		_listenerCone.OuterLPF = 0.25f;

		_listenerCone.InnerReverb = 1.0f * _reverbScaler;
		_listenerCone.OuterReverb = 0.1f * _reverbScaler;
	}

	// Test
	/*{
		_listenerCone = AudioPresets::ListenerCone;

		_listenerCone.InnerVolume *= _invDistanceScaler;
		_listenerCone.OuterVolume *= _invDistanceScaler;

		_listenerCone.InnerReverb *= _reverbScaler;
		_listenerCone.OuterReverb *= _reverbScaler;
	}*/

	// DirectX example
	/*{
		_listenerCone.InnerAngle = AudioPresets::ListenerCone.InnerAngle;
		_listenerCone.OuterAngle = AudioPresets::ListenerCone.OuterAngle;

		_listenerCone.InnerVolume = AudioPresets::ListenerCone.InnerVolume * _invDistanceScaler;
		_listenerCone.OuterVolume = AudioPresets::ListenerCone.OuterVolume * _invDistanceScaler;

		_listenerCone.InnerLPF = AudioPresets::ListenerCone.InnerLPF;
		_listenerCone.OuterLPF = AudioPresets::ListenerCone.OuterLPF;

		_listenerCone.InnerReverb = AudioPresets::ListenerCone.InnerReverb * _reverbScaler;
		_listenerCone.OuterReverb = AudioPresets::ListenerCone.OuterReverb * _reverbScaler;
	}*/

	_listener.SetCone(_listenerCone);
	//_emitter.SetCone(_emitterCone);
	
	// Default
	{
		_emitter.CurveDistanceScaler = distanceScaler;
		_emitter.pVolumeCurve = const_cast<X3DAUDIO_DISTANCE_CURVE *>(&AudioPresets::CustomCurve);
		_emitter.pLFECurve = const_cast<X3DAUDIO_DISTANCE_CURVE *>(&AudioPresets::LFECurve);
		_emitter.pReverbCurve = const_cast<X3DAUDIO_DISTANCE_CURVE *>(&AudioPresets::ReverbCurve);
	}

	// DirectX example
	/*{
		_emitter.pCone = const_cast<X3DAUDIO_CONE*>(&AudioPresets::EmitterCone);
	}*/

	if (!_effect)
	{
		ErrMsg("Failed to load sound effect " + fileName + "!");
		return false;
	}

	return true;
}

void SoundSource::PlayAudio()
{
	_effect->Play(true);
	UpdateAudio();
}

void SoundSource::UpdateAudio()
{
	if (_soundEffectFlag == (dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters))
		_effect->Apply3D(_listener, _emitter, false);
}

void SoundSource::AdjustVolume(float newVolume)
{
	_effect->SetVolume(newVolume);
}

void SoundSource::PauseAudio()
{
	_effect->Pause();
}

void SoundSource::ResumeAudio()
{
	_effect->Resume();
}

size_t SoundSource::GetSoundLength()
{
	return _sound->GetSampleDurationMS();
}

dx::SoundState SoundSource::GetSoundState()
{
	return _effect->GetState();
}

void SoundSource::SetListenerPosition(dx::XMFLOAT3 position)
{
	_listener.SetPosition(position);
}

void SoundSource::SetListenerOrientation(dx::XMFLOAT3 forwardVec, dx::XMFLOAT3 upVec)
{
	_listener.SetOrientation(forwardVec, upVec);
}

void SoundSource::SetEmitterPosition(dx::XMFLOAT3 position)
{
	_emitter.SetPosition(position);
}

float SoundSource::GetDistanceScaler() const
{
	return 1.0f / _invDistanceScaler;
}
void SoundSource::SetDistanceScaler(float scaler)
{
	if (scaler < 0.01f)
		scaler = 0.01f; // Prevent division by zero

	float distanceChange = 1.0f / (_invDistanceScaler * scaler);
	_listenerCone.InnerVolume *= distanceChange;
	_listenerCone.OuterVolume *= distanceChange;

	_emitter.CurveDistanceScaler = std::clamp(scaler, FLT_MIN, FLT_MAX);
	_listenerCone.InnerVolume = std::clamp(_listenerCone.InnerVolume, 0.0f, 2.0f);
	_listenerCone.OuterVolume = std::clamp(_listenerCone.OuterVolume, 0.0f, 2.0f);

	_invDistanceScaler = 1.0f / scaler;
	_listener.SetCone(_listenerCone); // Reapply cone to update volumes
}

float SoundSource::GetReverbScaler() const
{
	return _reverbScaler;
}
void SoundSource::SetReverbScaler(float scaler)
{
	if (scaler < 0.0001f)
		scaler = 0.0001f; // Prevent division by zero

	float reverbChange = scaler / _reverbScaler;
	_listenerCone.InnerReverb *= reverbChange;
	_listenerCone.OuterReverb *= reverbChange;

	_listenerCone.InnerReverb = std::clamp(_listenerCone.InnerReverb, 0.0f, 2.0f);
	_listenerCone.OuterReverb = std::clamp(_listenerCone.OuterReverb, 0.0f, 2.0f);

	_reverbScaler = scaler;
	_listener.SetCone(_listenerCone); // Reapply cone to update reverb
}

const X3DAUDIO_CONE &SoundSource::GetListenerCone() const
{
	return _listenerCone;
}
void SoundSource::SetListenerCone(const X3DAUDIO_CONE &cone)
{
	_listenerCone = cone;
	_listener.SetCone(_listenerCone);
}

const X3DAUDIO_CONE *SoundSource::GetEmitterCone() const
{
	if (_emitterConeSet)
		return &_emitterCone;
	return nullptr;
}
void SoundSource::SetEmitterCone(X3DAUDIO_CONE *cone)
{
	if (cone == nullptr)
	{
		_emitterConeSet = false;
		_emitter.SetOmnidirectional();
		return;
	}

	_emitterConeSet = true;
	_emitterCone = *cone;
	_emitter.SetCone(_emitterCone);
}

void SoundSource::ResetSound()
{
	_effect.reset();
	_effect = _sound->CreateInstance(_soundEffectFlag);
}
