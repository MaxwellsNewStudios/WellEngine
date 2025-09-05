#include "stdafx.h"
#include "Behaviours/CameraItemBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool CameraItemBehaviour::Start()
{
	if (_name.empty())
		_name = "CameraItemBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();
	SceneHolder *sceneHolder = scene->GetSceneHolder();
	Content *content = scene->GetContent();

	// Create camera flash entity
	Entity *ent = nullptr;
	if (!scene->CreateEntity(&ent, "Camera Flash", { {0,0,0},{.1f,.1f,.1f},{0,0,0,1} }, false))
		Warn("Failed to create Camera Flash entity!");
	ent->SetParent(GetEntity());
	ent->GetTransform()->SetPosition({ 0.150064f, 0.217686f, 0.259081f }, Local);

	ProjectionInfo projInfo = ProjectionInfo(120.0f * DEG_TO_RAD, 1.0f, { 0.01f, 100.0f });
	_flashLightBehaviour = new SpotLightBehaviour(projInfo, { 200, 200, 200 }, 1.0f, 0.3f, 0.0f);
	if (!_flashLightBehaviour->Initialize(ent))
		Warn("Failed to bind light behaviour to camera flash!");

	ent->SetSerialization(false);
	_flashLightBehaviour->SetSerialization(false);
	_flashLightBehaviour->SetEnabled(false);

	_flashSoundBehaviour = new SoundBehaviour("Camera Flash");
	if (!_flashSoundBehaviour->Initialize(GetEntity()))
		Warn("Failed to Initialize sound behaviour!");

	_flashSoundBehaviour->SetSerialization(false);

	QueueUpdate();

	return true;
}

bool CameraItemBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (_cooldownTimer > 0.0f)
		_cooldownTimer -= time.GetDeltaTime();

	if (!_firstFrameOfFlash && _flashTimer > 0.0f)
	{
		_flashTimer -= time.GetDeltaTime();
		if (_flashTimer <= 0.0f)
			_flashLightBehaviour->SetEnabled(false);
	}
	_firstFrameOfFlash = false;

	return true;
}

#ifdef USE_IMGUI
bool CameraItemBehaviour::RenderUI()
{
	if (ImGui::Button("Take Picture"))
		TakePicture(0.03f);
	return true;
}
#endif

bool CameraItemBehaviour::OnSelect()
{
	TakePicture(0.03f);
	return true;
}

void CameraItemBehaviour::TakePicture(float flashTime)
{
	if (_cooldownTimer > 0.0f)
		return;

	_flashTimer = flashTime;
	_cooldownTimer = 0.5f;
	_firstFrameOfFlash = true;
	_flashLightBehaviour->SetEnabled(true);
	_flashSoundBehaviour->Play();
}

bool CameraItemBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool CameraItemBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}
