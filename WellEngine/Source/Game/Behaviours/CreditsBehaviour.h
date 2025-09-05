#pragma once
#include "Behaviour.h"
#include "Behaviours/MeshBehaviour.h"
#include "Game.h"
#include "Behaviours/SoundBehaviour.h"

class [[register_behaviour]] CreditsBehaviour : public Behaviour
{
private:
	struct Slide
	{
		UINT textureID;
		float showLength;
	};

	float _elapsed = 0.0f;

	const float _showTime = 15.0f;
	const float _fadeTime = 1.0f;
	UINT _screenIndex = 0;

	std::vector<Slide> _slides;
	MeshBehaviour *_creditsMesh = nullptr;
	Game *_game = nullptr;
	SoundBehaviour *_song = nullptr;

	[[nodiscard]] bool ChangeScreenTexture();

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	void OnEnable() override;

public:
	CreditsBehaviour() = default;
	~CreditsBehaviour() = default;

};

