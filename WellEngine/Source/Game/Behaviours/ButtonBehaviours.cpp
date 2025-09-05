#include "stdafx.h"
#include "Behaviours/ButtonBehaviours.h"
#include "Game.h"
#include "Scenes/Scene.h"
#include "Input/Input.h"
#include "Behaviours/CreditsBehaviour.h"
#include "Debug/DebugData.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

#define HOVER_EFFECT dx::XMFLOAT3A({ 0.7f / 0.7f, 0.2f / 0.7f, 0.2f / 0.7f })

bool PlayButtonBehaviour::Start()
{
	if (_name.empty())
		_name = "PlayButtonBehaviour"; // For categorization in ImGui.

	_t = GetEntity()->GetTransform();

	SoundBehaviour *song = new SoundBehaviour("LurksBelowThemeSong", dx::SoundEffectInstance_Default);
	if (!song->Initialize(GetEntity()))
	{
		Warn("Failed to initialize theme song!");
		return true; //-V773
	}
	song->SetVolume(0.1f);
	song->SetSerialization(false);
	_soundLength = song->GetSoundLength();
	_song = song;

	SoundBehaviour *buttonSound = new SoundBehaviour("footstep_concrete_000", dx::SoundEffectInstance_Default);
	if (!buttonSound->Initialize(GetEntity()))
	{
		Warn("Failed to initialize theme song!");
		return true;
	}
	buttonSound->SetVolume(0.3f);
	buttonSound->SetEnabled(false);
	_buttonSound = buttonSound;

	QueueUpdate();

	return true;
}

bool PlayButtonBehaviour::Update(TimeUtils &time, const Input &input)
{
	GetScene()->GetGraphics()->SetDistortionStrength(0.0f);

	if (_time >= _timeToStart && _reset)
	{
		_song->SetEnabled(true);
		_song->ResetSound();
		_song->Play();
		_reset = false;
	}
	
	if (BindingCollection::IsTriggered(InputBindings::InputAction::Pause))
	{
		_reset = true;
		_time = 0.0f;
		_song->SetEnabled(false);
	}

	if (_time >= _soundLength)
	{
		_time = 0.0f;
		_reset = true;
	}

	_time += time.GetDeltaTime();

	if (_isSwitching)
	{
		_timedSceneSwitch -= time.GetDeltaTime();
		if (_timedSceneSwitch <= 0.0f)
		{
			if (!SwitchScene())
				Warn("Failed to switch scene!");
		}
	}

	return true;
}

bool PlayButtonBehaviour::OnSelect()
{
	_buttonSound->SetEnabled(true);

	_isSwitching = true;
	_timedSceneSwitch = 0.5f;

	Game *game = GetScene()->GetGame();
	game->GetGraphics()->BeginScreenFade(_timedSceneSwitch);
	return true;
}
bool PlayButtonBehaviour::OnHover()
{
	_t->SetScale(HOVER_EFFECT);
	return true;
}

void PlayButtonBehaviour::PlayButtonSound() const
{
	_buttonSound->SetEnabled(true);
}
bool PlayButtonBehaviour::SwitchScene()
{
	_isSwitching = false;
	_timedSceneSwitch = 0.0f;

	Scene *scene = GetScene();
	Game *game = scene->GetGame();
#ifndef EDIT_MODE
	if (_playCutscene)
	{
		if (!game->SetScene("StartCutscene"))
			Warn("Failed to set scene!");
		_playCutscene = false;
	}
	else
#endif
	{
#ifdef DEBUG_BUILD
		if (!game->SetScene(DebugData::Get().activeScene))
#else
		if (!game->SetScene("Cave"))
#endif
			Warn("Failed to set scene!");
	}


	Window window = game->GetWindow();
	Input::Instance().ToggleLockCursor(window);

	game->GetGraphics()->BeginScreenFade(-0.5f);

	_reset = true;
	_time = 0.0f;
	_song->SetEnabled(false);

	return true;
}

#ifdef USE_IMGUI
bool PlayButtonBehaviour::RenderUI()
{
	std::string lengthText = "Length: " + std::to_string(_time) + "/" + std::to_string(_soundLength);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), lengthText.c_str());

	std::string durationText = "Duration: " + std::to_string(_time) + "/" + std::to_string(_timeToStart);
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), durationText.c_str());

	return true;
}
#endif

// ---------------------------------------------------------------------

bool SaveButtonBehaviour::Start()
{
	if (_name.empty())
		_name = "SaveButtonBehaviour"; // For categorization in ImGui.

	_t = GetEntity()->GetTransform();

	return true;
}

bool SaveButtonBehaviour::OnSelect()
{
	PlayButtonBehaviour *start = nullptr;
	GetScene()->GetSceneHolder()->GetEntityByName("StartButton")->GetBehaviourByType<PlayButtonBehaviour>(start);
	start->PlayButtonSound();

	Scene *scenes = GetScene()->GetGame()->GetScene("Cave");
	if (!scenes->Serialize(true))
		Warn("Failed to serialize scene!");

	return true;
}
bool SaveButtonBehaviour::OnHover()
{
	_t->SetScale(HOVER_EFFECT);
	return true;
}

// ---------------------------------------------------------------------

bool NewSaveButtonBehaviour::Start()
{
	if (_name.empty())
		_name = "NewSaveButtonBehaviour"; // For categorization in ImGui.

	_t = GetEntity()->GetTransform();

	std::vector<SoundBehaviour *> sounds;
	if (GetScene()->GetSceneHolder()->GetEntityByName("StartButton")->GetBehavioursByType<SoundBehaviour>(sounds))
		_length = sounds[1]->GetSoundLength();

	QueueUpdate();

	return true;
}

bool NewSaveButtonBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (_reset)
	{
		if (_time >= _length)
		{
			Scene *scene = GetScene()->GetGame()->GetScene("Cave");
			std::string path = PATH_FILE_EXT(ASSET_PATH_SAVES, scene->GetName(), ASSET_EXT_SAVE);

			try 
			{
				if (!std::filesystem::remove(path))
					Warn("File not found!");
			}
			catch (const std::filesystem::filesystem_error &err) 
			{
				ErrMsgF("Filesystem error: {}", err.what());
				return false;
			}

			Scene *currentScene = GetScene();
			scene->ResetScene();

			if (!scene->InitializeCave("Cave", currentScene->GetDevice(), currentScene->GetContext(), 
				currentScene->GetGame(), currentScene->GetContent(), currentScene->GetGraphics(), 10.0f)) // TODO: Causes double sound
			{
				Warn("Failed to reset game scene!");
			}

			_reset = false;
			_time = 0.0f;
		}
		else
		{
			_time += time.GetDeltaTime();
		}
	}
	return true;
}

bool NewSaveButtonBehaviour::OnSelect()
{
	PlayButtonBehaviour *start = nullptr;
	GetScene()->GetSceneHolder()->GetEntityByName("StartButton")->GetBehaviourByType<PlayButtonBehaviour>(start);
	start->PlayButtonSound();

	_reset = true;

	return true;
}
bool NewSaveButtonBehaviour::OnHover()
{
	_t->SetScale(HOVER_EFFECT);
	return true;
}

// ---------------------------------------------------------------------

bool CreditsButtonBehaviour::Start()
{
	if (_name.empty())
		_name = "CreditsButtonBehaviour"; // For categorization in ImGui.

#ifndef EDIT_MODE
	_t = GetEntity()->GetTransform();

	QueueUpdate();
#endif

	return true;
}

bool CreditsButtonBehaviour::Update(TimeUtils &time, const Input &input)
{
#ifndef EDIT_MODE
	static float elapsed = 0.0f;
	if (_selected)
		elapsed += time.GetDeltaTime();

	if (elapsed >= 1.0f)
	{
		if (!GetScene()->GetGame()->SetScene("Credits"))
			Warn("Failed to set credits scene!");

		CreditsBehaviour *credits = nullptr;
		GetScene()->GetGame()->GetScene("Credits")->GetSceneHolder()->GetEntityByName("Credits Manager")->GetBehaviourByType<CreditsBehaviour>(credits);
		credits->SetEnabled(true);
		_selected = false;
		elapsed = 0.0f;
	}
#endif

	return true;
}

bool CreditsButtonBehaviour::OnSelect()
{
#ifndef EDIT_MODE
	_selected = true;
	GetScene()->GetGraphics()->BeginScreenFade(0.5f);
#endif
	return true;
}
bool CreditsButtonBehaviour::OnHover()
{
#ifndef EDIT_MODE
	_t->SetScale(HOVER_EFFECT);
#endif
	return true;
}

// ---------------------------------------------------------------------

bool ExitButtonBehaviour::Start()
{
	if (_name.empty())
		_name = "ExitButtonBehaviour"; // For categorization in ImGui.

	_t = GetEntity()->GetTransform();

	return true;
}

bool ExitButtonBehaviour::OnSelect()
{
	GetScene()->GetGame()->Exit();
	return true;
}
bool ExitButtonBehaviour::OnHover()
{
	_t->SetScale(HOVER_EFFECT);
	return true;
}
