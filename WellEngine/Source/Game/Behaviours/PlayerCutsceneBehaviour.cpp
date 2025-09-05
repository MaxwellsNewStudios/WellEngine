#include "stdafx.h"
#include "Behaviours/PlayerCutsceneBehaviour.h"
#include "Scenes/Scene.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Timing/TimelineSequence.h"
#include "Behaviours/MonsterBehaviour.h"
#include "Behaviours/PlayerViewBehaviour.h"
#include "Game.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

std::string StateToString(PlayerCutsceneState state)
{
    switch (state)
    {
    case START:             return "Start";
    case WAITING:           return "Waiting";
    case MOVE:              return "Move";
    case ROTATE:            return "Rotate";
    case RECHARGE:          return "Recharge";
    case MONSTER_SPAWN:     return "Monster Spawn";
    case FAINT:             return "Faint";
    case DONE:              return "Done";
    case PAUSE:             return "Pause";
    default:                return "NULL State";
    }
}

void PlayerCutsceneBehaviour::SetState(PlayerCutsceneState newState)
{
    if (_state == newState) return; // Don't chage

    _lastState = _state;
    _state = newState;
}

bool PlayerCutsceneBehaviour::Start()
{
    if (_name.empty())
        _name = "PlayerCutsceneBehaviour"; // For categorization in ImGui.

    _state = START;
    _lastState = START;

    _flashlightEnt = GetScene()->GetSceneHolder()->GetEntityByName("Flashlight");
	if (!_flashlightEnt->GetBehaviourByType<FlashlightBehaviour>(_flashlight))
	{
		ErrMsg("Failed to get FlashlighBehaviour");
		return false;
	}

	if (!GetEntity()->GetBehaviourByType<PlayerMovementBehaviour>(_playerMover))
	{
		ErrMsg("Failed to get PlayerMovementBehaviour");
		return false;
	}

    _cameraItem = new CameraItemBehaviour();

    if (!_cameraItem->Initialize(GetEntity()))
    {
        ErrMsg("Failed to create CameraItemBehaviour!");
        return false;
    }

	_dragSound = new SoundBehaviour("DragBody", (dx::SOUND_EFFECT_INSTANCE_FLAGS)(0x3), false, 50.0f, 0.25f);
	if (!_dragSound->Initialize(GetEntity()))
	{
		ErrMsg("Failed to initialize drag sound behaviour!");
		return false;
	}

    Entity *compass = GetScene()->GetSceneHolder()->GetEntityByName("Compass");
    if (compass)
        compass->Disable();

	_dragSound->SetVolume(0.4f);
	_dragSound->SetEnabled(false);
    _dragSound->SetSerialization(false);

    _caveMusic = new SoundBehaviour("The Cave", dx::SoundEffectInstance_ReverbUseFilters, false, 50.0f, 0.25f);
    if (!_caveMusic->Initialize(GetEntity()))
    {
        ErrMsg("Failed to initialize tave music sound behaviour!");
        return false;
    }

	_caveMusic->SetVolume(0.1f);
	_caveMusic->SetEnabled(false);
    _caveMusic->SetSerialization(false);

    _faintSound = new SoundBehaviour("BodyCollapse", dx::SoundEffectInstance_ReverbUseFilters, false, 50.0f, 0.25f);
    if (!_faintSound->Initialize(GetEntity()))
    {
        ErrMsg("Failed to initialize tave music sound behaviour!");
        return false;
    }

	_faintSound->SetVolume(0.2f);
	_faintSound->SetEnabled(false);
    _faintSound->SetSerialization(false);

    GetEntity()->SetSerialization(false);
    //GetEntity()->Disable();

    QueueUpdate();

    return true;
}

bool PlayerCutsceneBehaviour::Update(TimeUtils &time, const Input &input)
{
    _currentTimer.Update(time);

    Scene *scene = GetScene();
    Graphics *graphics = scene->GetGraphics();

    TimelineManager *tm = scene->GetTimelineManager();
    Transform *t = GetTransform();

    MonsterBehaviour *monster = scene->GetMonster();

    CameraBehaviour *animCamera = scene->GetAnimationCamera();

    if (input.GetKey(KeyCode::Space) == KeyState::Held) // Skip cutscene
    {
        _state = DONE;
        _currentTimer.MakeDone();
        _playerMover->Unlock();
    }

    switch (_state)
    {
    case START:
    {
        //_flashlight->DrainBattery();
        _flashlight->ToggleFlashlight(true);
        _playerMover->SetEnabled(false);
        _flashlight->SetBattry(80.0f);

        _currentTimer = CutsceneTimer(0.5f);
        _currentPoint = 1;

        SetState(WAITING);

        _caveMusic->SetEnabled(true);
        _caveMusic->Play();

        for (int i = 0; i < POINT_COUNT; i++)
            _movePoints[i].y -= 1.5f;

        GetTransform()->SetPosition(_movePoints[0], World);
        GetTransform()->SetRotation(_rotationPoints[0], World);

        _playerMover->Lock();

        //SetState(MONSTER_SPAWN);
        break;
    }

    case WAITING:
        if (_currentTimer.IsDone())
        {
            if (_lastState == START)
            {
				TimelineSequence seq;
				seq.SetEasing(Func_Linear);
				seq.SetType(SEQUENCE);
                float time = 0;

				for (int i = 0; i < 4; i++)
				{
					seq.AddWaypoint({ {_movePoints[i]}, _rotationPoints[i], t->GetScale(), time});
                    time += _speedPoints[i];
				}

				tm->AddSequence("FirstMove", seq);

				tm->RunSequence("FirstMove", GetTransform());
                _currentTimer = CutsceneTimer(7.4f);
				SetState(MOVE);
            }
        }
        break;

    case MOVE:
        if (_currentTimer.IsDone() && _currentTimer.IsActive())
        {
            _currentTimer.Disable();
            _flashlight->SetBattry(30);
        }

        if (tm->GetSequence("FirstMove").GetStatus() == SequenceStatus::FINISHED)
        {
            _flashlight->DrainBattery();
            _currentTimer = CutsceneTimer(6.4f);
            tm->GetSequence("FirstMove").Stop();
            SetState(START_RECHARGE);
        }
        break;

    case START_RECHARGE:
        _flashlight->Charge(time);
        if (_currentTimer.IsDone())
        {
			TimelineSequence seq;
			seq.SetEasing(Func_Linear);
			seq.SetType(SEQUENCE);
			float time = 0;

			for (int i = 3; i < POINT_COUNT-1; i++)
			{
				seq.AddWaypoint({ {_movePoints[i]}, _rotationPoints[i], t->GetScale(), time});
				time += _speedPoints[i];
			}

            _flashlight->ToggleFlashlight(true);

			tm->AddSequence("SecondMove", seq);

			tm->RunSequence("SecondMove", GetTransform());
			SetState(RECHARGE);
        }
        break;

    case RECHARGE:
        if (tm->GetSequence("SecondMove").GetStatus() == SequenceStatus::FINISHED)
        {
            _currentTimer = CutsceneTimer(0.4f);
            tm->GetSequence("SecondMove").Stop();
            SetState(MONSTER_SPAWN);
        }
        break;

    case MONSTER_SPAWN:
    {
        if (_currentTimer.IsDone())
        {
			graphics->SetDistortionOrigin(GetTransform()->GetPosition(World));
		    _currentTimer = CutsceneTimer(4.5f);

		    SetState(MONSTER_TEASING);
        }
        break;
    }

    case MONSTER_TEASING:
    {
		const float distortionAmp = 5;
		graphics->SetDistortionStrength(_currentTimer.GetTime()*distortionAmp);

		if (_currentTimer.IsDone())
		{
			SetState(FAINT);
			_currentTimer.Disable();
		}
        break;
    }

    case FAINT:
        graphics->BeginScreenFade(0.45f);

        _faintSound->SetEnabled(true);
        _faintSound->Play();

        if (_flashlightEnt)
        {
            _flashlight->ToggleFlashlight(false);
            _flashlightEnt->Disable();
        }

        if (graphics->GetScreenFadeAmount() <= 0.0f)
            if (!_currentTimer.IsActive())
                _currentTimer = CutsceneTimer(1.5f);

        if (_currentTimer.IsDone())
        {
            _currentTimer = CutsceneTimer(4.5f);
            _faintSound->SetEnabled(false);
            SetState(PICTURE);
        }

        break;

    case PICTURE:
        if (_currentTimer.IsDone())
        {
            _cameraItem->TakePicture(0.05f);
            _currentTimer = CutsceneTimer(0.1f);
            graphics->SetScreenFadeManual(0.7f);
            SetState(FLASH);
        }
        break;

    case FLASH:
        if (_currentTimer.IsDone())
        {
            graphics->SetScreenFadeManual(1);
            _currentTimer = CutsceneTimer(2.5f);
            SetState(DRAGGING);
        }
        break;

    case DRAGGING:
		_dragSound->SetEnabled(true);
		_dragSound->Play();

        if (_currentTimer.IsDone())
        {
            _currentTimer = CutsceneTimer(4.5f);
            SetState(DONE);
        }

        break;

    case DONE:
        if (_currentTimer.IsDone())
        {
            graphics->BeginScreenFade(-2.0f);
            _playerMover->Unlock();

            if (!GetScene()->GetGame()->SetScene("Cave"))
            {
                ErrMsg("Failed to set game scene");
                return false;
            }
        }
        break;

    case PAUSE:
    default:
        break;
    }

    return true;
}

#ifdef USE_IMGUI
bool PlayerCutsceneBehaviour::RenderUI()
{
    ImGui::Text((std::string("State: ") + StateToString(_state)).c_str());

    if (ImGui::Button("Restart"))
        SetState(START);
    ImGui::SameLine();
    if (ImGui::Button("Pause"))
        SetState(PAUSE);
    if (ImGui::Button("Skip"))
        SetState(DONE);
    return true;
}
#endif
