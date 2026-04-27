#pragma once

#include "Game/Behaviours/Behaviour.h"
#include "B_SoundSource.h"

namespace WellEngine
{
	class [[register_behaviour]] B_SoundSourceAmbient : public Behaviour
	{
	public:
		std::string_view GetName() const override { return "SoundSourceAmbient"; }
		std::string_view GetScriptPath() const override { return __FILE__; }

	private:
		B_SoundSource *_soundBehaviour = nullptr;
	
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
		B_SoundSourceAmbient() = default;
		B_SoundSourceAmbient(std::string fileName,
			dx::SOUND_EFFECT_INSTANCE_FLAGS flags = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters,
			bool loop = false, float volume = 0.5f, float distanceScaler = 75.0f, float reverbScaler = 1.0f,
			float minimumDelay = 2.0f, float maximumDelay = 10.0f);

		~B_SoundSourceAmbient() = default;

		void TriggerSound();

		[[nodiscard]] B_SoundSource *GetB_SoundSource() const;
	};
}
