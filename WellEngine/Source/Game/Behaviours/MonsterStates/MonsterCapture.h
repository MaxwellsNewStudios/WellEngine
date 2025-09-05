#pragma once

#include "MonsterState.h"

class MonsterCapture : public MonsterState
{
public:
	MonsterCapture() = default;
	~MonsterCapture() = default;

protected:
	bool OnEnter() override;
	bool OnUpdate(TimeUtils &time) override;
	bool OnExit() override;

private:
	Transform *_playerTransform = nullptr;

	dx::XMFLOAT3A _playerPos{};
	dx::XMFLOAT4A _playerRotation{};
};
