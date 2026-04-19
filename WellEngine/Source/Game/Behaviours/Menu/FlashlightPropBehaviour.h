#pragma once

#include "Game/Behaviour.h"
#include "../Rendering/Lighting/SpotLightBehaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] FlashlightPropBehaviour : public Behaviour
	{
	private:
		bool _isOn = true;

		SpotLightBehaviour *_lightBehaviour = nullptr;

	protected:
		[[nodiscard]] bool Start() override;
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
		[[nodiscard]] bool FixedUpdate(float deltaTime, const Input &input) override;

	public:
		FlashlightPropBehaviour() = default;
		~FlashlightPropBehaviour() = default;
	};
}
