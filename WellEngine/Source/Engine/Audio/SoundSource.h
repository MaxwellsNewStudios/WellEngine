#pragma once
#include <Audio.h>

namespace AudioPresets
{
	constexpr X3DAUDIO_CONE ListenerCone = { 
		X3DAUDIO_PI / 2, X3DAUDIO_PI, 
		1.0f, 0.708f, 
		0.0f, 0.25f, 
		0.708f, 1.0f 
	};
	/*constexpr X3DAUDIO_CONE ListenerCone = {
		X3DAUDIO_PI * 5.0f / 6.0f, X3DAUDIO_PI * 11.0f / 6.0f, 
		1.0f, 0.75f, 
		0.0f, 0.25f, 
		0.708f, 1.0f
	};*/
	constexpr X3DAUDIO_CONE EmitterCone = {
		0.0f, 0.0f, 
		0.0f, 1.0f, 
		0.0f, 1.0f, 
		0.0f, 1.0f
	};

	constexpr X3DAUDIO_DISTANCE_CURVE_POINT CustomCurvePoints[11] = {
		{ 0.0f,		1.0f		},
		{ 0.1f,		0.625f		},
		{ 0.2f,		0.4081632f	},
		{ 0.3f,		0.2734375f	},
		{ 0.4f,		0.1851852f	},
		{ 0.5f,		0.125f		},
		{ 0.6f,		0.0826446f	},
		{ 0.7f,		0.0520833f	},
		{ 0.8f,		0.0295858f	},
		{ 0.9f,		0.0127551f	},
		{ 1.0f,		0.0f		}
	};
	constexpr X3DAUDIO_DISTANCE_CURVE CustomCurve = {
		const_cast<X3DAUDIO_DISTANCE_CURVE_POINT *>(&CustomCurvePoints[0]),
		sizeof(CustomCurvePoints) / sizeof(X3DAUDIO_DISTANCE_CURVE_POINT)
	};

	constexpr X3DAUDIO_DISTANCE_CURVE_POINT CustomInverseCurvePoints[11] = {
		{ 0.0f,		0.0f		},
		{ 0.1f,		0.0127551f	},
		{ 0.2f,		0.0295858f	},
		{ 0.3f,		0.0520833f	},
		{ 0.4f,		0.0826446f	},
		{ 0.5f,		0.125f		},
		{ 0.6f,		0.1851852f	},
		{ 0.7f,		0.2734375f	},
		{ 0.8f,		0.4081632f	},
		{ 0.9f,		0.625f		},
		{ 1.0f,		1.0f		}
	};
	constexpr X3DAUDIO_DISTANCE_CURVE CustomInverseCurve = {
		const_cast<X3DAUDIO_DISTANCE_CURVE_POINT *>(&CustomInverseCurvePoints[0]),
		sizeof(CustomInverseCurvePoints) / sizeof(X3DAUDIO_DISTANCE_CURVE_POINT)
	};

	constexpr X3DAUDIO_DISTANCE_CURVE_POINT LinearInverseCurvePoints[11] = {
		{ 0.0f,		0.0f		},
		{ 1.0f,		1.0f		}
	};
	constexpr X3DAUDIO_DISTANCE_CURVE LinearInverseCurve = {
		const_cast<X3DAUDIO_DISTANCE_CURVE_POINT *>(&LinearInverseCurvePoints[0]),
		sizeof(LinearInverseCurvePoints) / sizeof(X3DAUDIO_DISTANCE_CURVE_POINT)
	}; 

	constexpr X3DAUDIO_DISTANCE_CURVE_POINT LFECurvePoints[3] = {
		{ 0.0f, 1.0f }, { 0.25f, 0.0f}, { 1.0f, 0.0f }
	};
	constexpr X3DAUDIO_DISTANCE_CURVE LFECurve = {
		const_cast<X3DAUDIO_DISTANCE_CURVE_POINT*>(&LFECurvePoints[0]), 3
	};

	constexpr X3DAUDIO_DISTANCE_CURVE_POINT ReverbCurvePoints[3] = {
		{ 0.0f, 0.5f}, { 0.75f, 1.0f }, { 1.0f, 0.0f }
	};
	constexpr X3DAUDIO_DISTANCE_CURVE ReverbCurve = {
		const_cast<X3DAUDIO_DISTANCE_CURVE_POINT*>(&ReverbCurvePoints[0]), 3
	};
}

class SoundSource
{
private:
	std::unique_ptr<dx::SoundEffect> _sound;
	std::unique_ptr<dx::SoundEffectInstance> _effect;

	dx::AudioListener _listener;
	dx::AudioEmitter _emitter;

	X3DAUDIO_CONE _listenerCone = AudioPresets::ListenerCone;
	X3DAUDIO_CONE _emitterCone = AudioPresets::EmitterCone;

	bool _emitterConeSet = false;
	float _invDistanceScaler = 1.0f / 75.0f;
	float _reverbScaler = 1.0f;

	dx::SOUND_EFFECT_INSTANCE_FLAGS _soundEffectFlag = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters;

public:
	SoundSource() = default;
	~SoundSource() = default;
	bool Initialize(dx::AudioEngine *audEngine, 
		dx::SOUND_EFFECT_INSTANCE_FLAGS flags, std::string fileName, 
		float distanceScaler = 75.0f, float reverbScaler = 1.0f);

	void PlayAudio();
	void UpdateAudio();
	void AdjustVolume(float newVolume);
	void PauseAudio();
	void ResumeAudio();

	size_t GetSoundLength(); // Sound lenght in milliseconds
	dx::SoundState GetSoundState();

	void SetListenerPosition(dx::XMFLOAT3 position);
	void SetListenerOrientation(dx::XMFLOAT3 forwardVec, dx::XMFLOAT3 upVec);
	void SetEmitterPosition(dx::XMFLOAT3 position);

	float GetDistanceScaler() const;
	void SetDistanceScaler(float scaler);
	float GetReverbScaler() const;
	void SetReverbScaler(float scaler);

	const X3DAUDIO_CONE &GetListenerCone() const;
	void SetListenerCone(const X3DAUDIO_CONE &cone);

	const X3DAUDIO_CONE *GetEmitterCone() const;
	void SetEmitterCone(X3DAUDIO_CONE *cone);

	void ResetSound();

	TESTABLE()
};
