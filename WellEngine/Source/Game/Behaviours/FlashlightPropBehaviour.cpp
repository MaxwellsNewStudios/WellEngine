#include "stdafx.h"
#include "Behaviours/FlashlightPropBehaviour.h"
#include "Scenes/Scene.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool FlashlightPropBehaviour::Start()
{
	if (_name.empty())
		_name = "FlashlightPropBehaviour"; // For categorization in ImGui.

	Entity *spotLight = nullptr;
	if (!GetScene()->CreateSpotLightEntity(&spotLight, "Spotlight Flashlight", { 500.0f, 495.0f, 480.0f }, 1.0f, 60.0f, false, 0.1f, 4, 0.5f))
	{
		ErrMsg("Failed to create spotlight entity to flashlight!");
		return false;
	}
	spotLight->SetParent(GetScene()->GetSceneHolder()->GetEntityByName("FlashlightProp"));
	//spotLight->GetTransform()->SetPosition({ 0.0f, 0.0f, 1.5f });
	spotLight->SetSerialization(false);

	spotLight->GetBehaviourByType<SpotLightBehaviour>(_lightBehaviour);

	QueueUpdate();
	QueueFixedUpdate();

	return true;
}

bool FlashlightPropBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (input.GetKey(KeyCode::F) == KeyState::Pressed)
	{
		_isOn = !_isOn;
	}
	return true;
}

bool FlashlightPropBehaviour::FixedUpdate(float deltaTime, const Input &input)
{
	float flickerChance = !_isOn ? 1.0f : 0.15f; // Chance to flicker per fixed update
	if (rand() % 1000 < flickerChance * 1000)
	{
		_lightBehaviour->SetEnabled(false);
	}
	else
	{
		_lightBehaviour->SetEnabled(true);
	}

	return true;
}
