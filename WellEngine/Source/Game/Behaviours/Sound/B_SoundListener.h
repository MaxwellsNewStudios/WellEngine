#pragma once

#include "Game/Behaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] B_SoundListener : public Behaviour
	{
	public:
		const std::string &GetName() const override { return "SoundListener"; }

	private:

	protected:
		[[nodiscard]] bool Start() override;

		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	public:
		B_SoundListener() = default;
		~B_SoundListener() = default;

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
}
