#pragma once
#include "Behaviour.h"
#include "Behaviours/SoundBehaviour.h"
#include "Behaviours/MonsterBehaviour.h"

class [[register_behaviour]] MonsterHintBehaviour final : public Behaviour
{
private:
	std::vector<SoundBehaviour*> _sounds;
	float _timer = 0.0f;
	float _minDistance = 100.0f;

	float _minDelay = 100.0f;
	float _maxDelay = 200.0f;
	MonsterBehaviour* _monsterBehaviour = nullptr;

#ifdef USE_IMGUI
	bool _drawEmitters = false;
#endif

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input& input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

public:
	MonsterHintBehaviour() = default;
	~MonsterHintBehaviour() = default;
};