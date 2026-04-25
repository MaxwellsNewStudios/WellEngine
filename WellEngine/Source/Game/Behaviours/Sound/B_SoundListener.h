#pragma once

#include "Game/Behaviours/Behaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] B_SoundListener : public Behaviour, public IRefTarget<B_SoundListener>
	{
	public:
		std::string_view GetName() const override { return "SoundListener"; }

	private:

	protected:
		[[nodiscard]] bool Start() override;

	public:
		B_SoundListener() = default;
		~B_SoundListener() = default;
	};
}
