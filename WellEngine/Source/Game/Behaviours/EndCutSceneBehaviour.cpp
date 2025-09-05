#include "stdafx.h"
#include "Behaviours/EndCutSceneBehaviour.h"
#include "Game.h"
#include "Scenes/Scene.h"
#include "Behaviours/CreditsBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool EndCutSceneBehaviour::Start()
{
	if (_name.empty())
		_name = "EndCutSceneBehaviour"; // For categorization in ImGui.

	_objectTransform = GetEntity()->GetTransform();
	_playerEntity = GetScene()->GetSceneHolder()->GetEntityByName("Player Entity");

	Content* content = GetScene()->GetContent();

	Entity* ent = GetEntity();
	
	InteractableBehaviour* interactableBehaviour;
	if (!ent->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour))
	{
		interactableBehaviour = new InteractableBehaviour();
		if (!interactableBehaviour->Initialize(ent))
		{
			ErrMsg("Failed to initialize interactable behaviour!");
			return false;
		}
	}
	interactableBehaviour->AddInteractionCallback(std::bind(&EndCutSceneBehaviour::CutScene, this));

	QueueUpdate();

	return true;
}

bool EndCutSceneBehaviour::Update(TimeUtils &time, const Input& input)
{
	if (_cutSceneFinished) 
		return true;

	TimelineManager* cutScene = GetScene()->GetTimelineManager();
	
	if (_startCutScene)
	{
		const float length = cutScene->GetSequence(_cutSceneName).GetLength();
		static float elapsed = 0.0f;
		const float timeToFade = length;
		if (elapsed >= length - timeToFade)
		{
			GetScene()->GetGame()->GetGraphics()->BeginScreenFade(timeToFade);
			_startCutScene = false;
		}
		else
			elapsed += time.GetDeltaTime();
	}

	if (cutScene->GetSequence(_cutSceneName).GetStatus() == SequenceStatus::FINISHED)
	{
		_cutSceneFinished = true; // Mark cutscene as finished

#ifndef EDIT_MODE
		// Set to credit scene
		Game* game = GetScene()->GetGame();
		//FogSettingsBuffer fogSettings;
		//fogSettings.thickness = 0.03f;
		//game->GetGraphics()->SetFogSettings(fogSettings);
		if (!game->SetScene("Credits"))
		{
			ErrMsg("Could not switch to credit scene from ending!");
		}

		CreditsBehaviour *credits = nullptr;
		GetScene()->GetGame()->GetScene("Credits")->GetSceneHolder()->GetEntityByName("Credits Manager")->GetBehaviourByType<CreditsBehaviour>(credits);
		credits->SetEnabled(true);
#endif
	}
	
	return true;
}

bool EndCutSceneBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool EndCutSceneBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

void EndCutSceneBehaviour::CutScene()
{
	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();

	if (!_playerEntity)
		_playerEntity = sceneHolder->GetEntityByName("Player Entity");

	if (!_playerEntity)
	{
		ErrMsg("Could not find player entity!");
		return;
	}

	TimelineManager* cutScene = GetScene()->GetTimelineManager();
	SequenceStatus status = cutScene->RunSequence(_cutSceneName, _playerEntity->GetTransform());
	_startCutScene = true;

	if (status != SequenceStatus::LERP_TO_START)
	{
		ErrMsg("Failed to run cutscene!");
		_cutSceneFinished = false;
		return;
	}

	MeshBehaviour *flashMesh = nullptr;
	sceneHolder->GetEntityByName("Flashlight Body Mesh")->GetBehaviourByType<MeshBehaviour>(flashMesh);
	flashMesh->SetOverlay(false);

	sceneHolder->GetEntityByName("Flashlight Lever Mesh")->GetBehaviourByType<MeshBehaviour>(flashMesh);
	flashMesh->SetOverlay(false);

	for (int i = 1; i <= 3; i++)
	{
		sceneHolder->GetEntityByName(std::format("Flashlight Indicator{} Mesh", i))->GetBehaviourByType<MeshBehaviour>(flashMesh);
		flashMesh->SetOverlay(false);
	}
}