#include "stdafx.h"
#include "Behaviours/CreditsBehaviour.h"
#include "Scenes/Scene.h"
#include "Game.h"
#include "Behaviours/SoundBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool CreditsBehaviour::ChangeScreenTexture()
{
	_game->GetGraphics()->BeginScreenFade(-_fadeTime);

	Material mat = Material(*_creditsMesh->GetMaterial());
	mat.textureID = _slides[_screenIndex].textureID;
	if (!_creditsMesh->SetMaterial(&mat))
	{
		ErrMsg("Failed to set credits material!");
		return false;
	}

	if (_slides[_screenIndex].textureID == GetScene()->GetContent()->GetTextureID("Credit_Special_Texture"))
	{
		GetScene()->GetSceneHolder()->GetEntityByName("Maxwell")->Enable();
	}

	_screenIndex++;

	return true;
}

bool CreditsBehaviour::Start()
{
	if (_name.empty())
		_name = "CreditsBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();

	Entity* ent = scene->GetSceneHolder()->GetEntityByName("Credits Mesh");
	if (!ent->GetBehaviourByType<MeshBehaviour>(_creditsMesh))
		return false;

	_game = scene->GetGame();

	// Create main camera
	{
		ProjectionInfo projInfo = ProjectionInfo(65.0f * DEG_TO_RAD, 16.0f / 9.0f, { 0.2f, 100.0f });
		CameraBehaviour *camera = new CameraBehaviour(projInfo);

		if (!camera->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind MainCamera behaviour!");
			return false;
		}

		camera->SetSerialization(false);

		scene->SetViewCamera(camera);
	}

	Content *content = scene->GetContent();
	// 2.64
	Slide slide{};
	slide.textureID = content->GetTextureID("Credit_Logo_Texture");
	slide.showLength = 12.0f; // 12.6
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_Devs_Texture");
	slide.showLength = 12.0f; // 25.2
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_Assets_Texture");
	slide.showLength = 17.0f; // 42.8
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_Testing_Texture");
	slide.showLength = 12.0f; // 55.4
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_BTH_Texture");
	slide.showLength = 10.0f; // 66.5
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_Story1_Texture");
	slide.showLength = 17.0f; // 83.7
	_slides.emplace_back(slide);

	slide.textureID = content->GetTextureID("Credit_Special_Texture");
	slide.showLength = 12.0f; // 96
	_slides.emplace_back(slide);

	SoundBehaviour *song = new SoundBehaviour("Credits", (dx::SOUND_EFFECT_INSTANCE_FLAGS)0U);
	if (!song->Initialize(GetEntity()))
	{
		ErrMsg("Failed to initialize sound!");
		return false;
	}
	song->SetSerialization(false);
	song->SetVolume(0.3f);
	song->SetEnabled(false);
	_song = song;

	QueueUpdate();

	return true;
}

bool CreditsBehaviour::Update(TimeUtils &time, const Input &input)
{
	_elapsed += time.GetDeltaTime();
	static bool hasFadedOut = false;

	if (BindingCollection::IsTriggered(InputBindings::InputAction::Pause))
	{
		_screenIndex = 0;
		_elapsed = 0.0f;
		hasFadedOut = false;
		_game->GetGraphics()->BeginScreenFade(0.5f);
		GetScene()->GetSceneHolder()->GetEntityByName("Maxwell")->Disable();

		std::vector<SoundBehaviour *> sounds;
		GetScene()->GetGame()->GetScene("MainMenu")->GetSceneHolder()->GetEntityByName("StartButton")->GetBehavioursByType<SoundBehaviour>(sounds);
		sounds[0]->ResetSound();
		sounds[0]->Play();

		SetEnabled(false);
		return true;
	}

	float showTime = _slides[_screenIndex == _slides.size() ? _slides.size() - 1 : _screenIndex].showLength;
	if (!hasFadedOut && _elapsed >= showTime && _elapsed <= showTime + _fadeTime) // Fade out
	{
		_game->GetGraphics()->BeginScreenFade(_fadeTime);
		hasFadedOut = true;
	}
	else if (hasFadedOut && _elapsed >= showTime + _fadeTime) // Switch credit screen texture / scene
	{
		if (_screenIndex >= _slides.size()) // Credit scene finished
		{
			Scene *currentScene = GetScene();
			currentScene->GetSceneHolder()->GetEntityByName("Maxwell")->Disable();

			if (!_game->SetScene("MainMenu"))
			{
				ErrMsg("Failed to set scene!");
				return false;
			}

			// Reset game

			Scene *gameScene = currentScene->GetGame()->GetScene("Cave");
			std::string path = PATH_FILE_EXT(ASSET_PATH_SAVES, gameScene->GetName(), ASSET_EXT_SAVE);

			try
			{
				if (!std::filesystem::remove(path))
					Warn("File not found!");


				if (!std::filesystem::remove(path))
					Warn("File not found!");
			}
			catch (const std::filesystem::filesystem_error &err)
			{
				ErrMsgF("Filesystem error: {}", err.what());
				return false;
			}

			gameScene->ResetScene();
			if (!gameScene->InitializeCave("Cave", currentScene->GetDevice(), currentScene->GetContext(), currentScene->GetGame(), currentScene->GetContent(),
				currentScene->GetGraphics(), 10.0f))
			{
				ErrMsg("Failed to reset game scene!");
				return false;
			}

			std::vector<SoundBehaviour *> sounds;
			currentScene->GetGame()->GetScene("MainMenu")->GetSceneHolder()->GetEntityByName("StartButton")->GetBehavioursByType<SoundBehaviour>(sounds);
			sounds[0]->ResetSound();
			sounds[0]->Play();

			_game->GetGraphics()->BeginScreenFade(-_fadeTime);
			SetEnabled(false);
			_screenIndex = 0;
		}
		else if (!ChangeScreenTexture()) // Next slide
		{
			return false;
		}
		hasFadedOut = false;
		_elapsed = 0.0f;
	}

	return true;
}

void CreditsBehaviour::OnEnable()
{
	_song->SetEnabled(true);
	if (ChangeScreenTexture())
		return;
}
