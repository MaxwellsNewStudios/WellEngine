#pragma once

#include "Behaviour.h"
#include "Behaviours/SoundBehaviour.h"

// Abstract class intended as a base class for UI buttons (ex: play, exit, options...)
class UIButtonBehaviour : public Behaviour
{
public:
	UIButtonBehaviour() = default;
	virtual ~UIButtonBehaviour() = default;
};


class [[register_behaviour]] PlayButtonBehaviour : public UIButtonBehaviour
{
private:
	Transform *_t = nullptr;

	SoundBehaviour *_song = nullptr;
	SoundBehaviour *_buttonSound = nullptr;
	const float _timeToStart = 10.0f; // Seconds
	float _time = _timeToStart;
	float _soundLength = 0.0f;
	bool _reset = true;

	bool _isSwitching = false;
	float _timedSceneSwitch = 0.0f;

	bool _playCutscene = true;

	[[nodiscard]] bool SwitchScene();

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input);
	[[nodiscard]] bool OnSelect() override;
	[[nodiscard]] bool OnHover() override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI();
#endif

public:
	PlayButtonBehaviour() = default;
	~PlayButtonBehaviour() = default;

	void PlayButtonSound() const;
};

// ---------------------------------------------------------------------

class [[register_behaviour]] SaveButtonBehaviour : public UIButtonBehaviour
{
private:
	Transform *_t = nullptr;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool OnSelect() override;
	[[nodiscard]] bool OnHover() override;

public:
	SaveButtonBehaviour() = default;
	~SaveButtonBehaviour() = default;
};

// ---------------------------------------------------------------------

class [[register_behaviour]] NewSaveButtonBehaviour : public UIButtonBehaviour
{
private:
	Transform *_t = nullptr;

	bool _reset = false;
	float _time = 0.0f;
	float _length = 0.0f;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] bool OnSelect() override;
	[[nodiscard]] bool OnHover() override;

public:
	NewSaveButtonBehaviour() = default;
	~NewSaveButtonBehaviour() = default;
};

// ---------------------------------------------------------------------

class [[register_behaviour]] CreditsButtonBehaviour : public UIButtonBehaviour
{
private:
	Transform *_t = nullptr;

	bool _selected = false;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
	[[nodiscard]] bool OnSelect() override;
	[[nodiscard]] bool OnHover() override;

public:
	CreditsButtonBehaviour() = default;
	~CreditsButtonBehaviour() = default;
};

// ---------------------------------------------------------------------

class [[register_behaviour]] ExitButtonBehaviour : public UIButtonBehaviour
{
private:
	Transform *_t = nullptr;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool OnSelect() override;
	[[nodiscard]] bool OnHover() override;

public:
	ExitButtonBehaviour() = default;
	~ExitButtonBehaviour() = default;
};
