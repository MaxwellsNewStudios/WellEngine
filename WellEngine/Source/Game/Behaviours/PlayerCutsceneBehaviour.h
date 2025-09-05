#pragma once

#include "Behaviour.h"
#include "Behaviours/PlayerMovementBehaviour.h"
#include "Timing/TimelineSequence.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Behaviours/CameraItemBehaviour.h"

enum PlayerCutsceneState
{
	START,
	WAITING,
	MOVE,
	ROTATE,
	START_RECHARGE,
	RECHARGE,
	LOOK_OUT,
	MONSTER_SPAWN,
	MONSTER_TEASING,
	FAINT,
	PICTURE,
	FLASH,
	DRAGGING,
	DONE,
	PAUSE,

	STATE_COUNT
};

struct CutsceneTimer
{
	CutsceneTimer(): _currentTime(0), _duration(0), _active(true) {}
	CutsceneTimer(float duration): _currentTime(0), _duration(duration), _active(true) {}

	void Update(TimeUtils &time) { _currentTime += time.GetDeltaTime(); }
	bool IsDone() const { return _currentTime >= _duration && _active; }
	void MakeDone() { _currentTime = _duration; _active = true; }
	float GetTime() const { return _currentTime; }
	void Disable() { _active = false; }
	void Reset() { _active = true; _currentTime = 0; }
	bool IsActive() const { return _active; }

private:
	float _currentTime, _duration;
	bool _active;
};

constexpr UINT POINT_COUNT = 10;

class [[register_behaviour]] PlayerCutsceneBehaviour : public Behaviour
{
private:
	PlayerCutsceneState _state{}, _lastState{};
	CutsceneTimer _currentTimer;

	TimelineSequence _rotateSeq;

	PlayerMovementBehaviour *_playerMover = nullptr;

	Entity *_flashlightEnt = nullptr;
	FlashlightBehaviour *_flashlight = nullptr;

	CameraItemBehaviour *_cameraItem = nullptr;

	SoundBehaviour *_dragSound = nullptr;
	SoundBehaviour *_caveMusic = nullptr;
	SoundBehaviour *_faintSound = nullptr;

	const UINT _faintPoint = 12;

	UINT _currentPoint = 0;
	dx::XMFLOAT3A _movePoints[POINT_COUNT] {
		{ -136.427338f, -25.737167f,	-5.285335f	},		// Start 1.1
		{ -138.108871f, -24.476824f,	-3.482301f	},		// Start 1.2
		{ -140.385773f, -22.588501f,	0.007089f },		// Start 1.3
		{ -141.393509f, -20.339582f,	3.872515f },		// Start 1.4
		{ -141.888657f, -18.577436f,	6.642964f },		// Start 1.5
		{ -142.142624f, -17.236174f,	8.750660f	},		// Start 2.1
		{ -142.090622f, -16.181681f,	9.636235f	},		// Start 2.2
		{ -142.088989f, -15.017520f,	9.649009f },		// Start 2.3
		{ -142.088989f, -15.017520f,	9.649009f },		// Start 2.4
		{ -142.508743f, -15.105126f,	12.093872f },		// Start 2.5
	};

	dx::XMFLOAT4A _rotationPoints[POINT_COUNT]{
		{ 0.122810f,  0.379404f,  0.050889f, -0.915615f },		// Start 1.1
		{ 0.200494f,  0.320811f,  0.069683f, -0.923036f },		// Start 1.2
		{ 0.228724f,  0.142500f,  0.033866f, -0.962394f },		// Start 1.3
		{ 0.279949f,  0.078344f,  0.022929f, -0.956523f },		// Start 1.4
		{ 0.291298f,  0.038305f,  0.011674f, -0.955778f },		// Start 1.5
		{ 0.368788f, -0.016988f, -0.006742f, -0.929317f },		// Start 2.1
		{ 0.703553f, -0.039555f, -0.039284f, -0.708430f },		// Start 2.2
		{ 0.085993f,  0.014999f,  0.001294f, -0.996163f },		// Start 2.3
		{ 0.085993f,  0.014999f,  0.001294f, -0.996163f },		// Start 2.4
		{ 0.143663f,  0.380940f,  0.219322f,  0.886644f },		// Start 2.5
	};

	float _speedPoints[POINT_COUNT]{
		3.0f,		// Start 1.1
		5.0f,		// Start 1.2
		5.0f,		// Start 1.3
		3.0f,		// Start 1.4
		3.0f,		// Start 1.5

		3.0f,		// Start 2.1
		5.0f,		// Start 2.2
		5.0f,		// Start 2.3
		3.0f,		// Start 2.4
		3.0f,		// Start 2.5
	};

	float speedup = 1;

	void SetState(PlayerCutsceneState newState);

protected:
	[[nodiscard]] bool Start() override;

public:
	PlayerCutsceneBehaviour() = default;
	~PlayerCutsceneBehaviour() = default;

	[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif
};

