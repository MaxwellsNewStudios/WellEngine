#include "stdafx.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Scenes/Scene.h"
#include "Entity.h"
#include "Content/Content.h"
#include "Behaviours/MonsterBehaviour.h"
#include "Collision/Intersections.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool FlashlightBehaviour::Start()
{
	if (_name.empty())
		_name = "FlashlightBehaviour"; // For categorization in ImGui.

	Content *content = GetScene()->GetContent();

	Entity *flashlightEntity = GetEntity();
	UINT meshID = content->GetMeshID("FlashlightBody");
	Material mat{};
	mat.textureID = content->GetTextureID("FlashlightBody");
	mat.normalID = content->GetTextureID("FlashlightBody_Normal");
	mat.specularID = content->GetTextureID("FlashlightBody_Specular");
	mat.glossinessID = content->GetTextureID("FlashlightBody_Glossiness");
	mat.ambientID = content->GetTextureID("Ambient");
	mat.vsID = content->GetShaderID("VS_Geometry");

	Entity *flashlightBodyMesh = nullptr;
	if (!GetScene()->CreateMeshEntity(&flashlightBodyMesh, "Flashlight Body Mesh", meshID, mat, false, false, true))
	{
		ErrMsg("Failed to initialize flashlight");
		return false;
	}
	flashlightBodyMesh->SetSerialization(false);
	flashlightBodyMesh->SetParent(flashlightEntity);
	flashlightBodyMesh->GetTransform()->SetScale({ 0.1f, 0.1f, 0.1f });

	meshID = content->GetMeshID("FlashlightLever");
	mat.textureID = content->GetTextureID("FlashlightLever");
	mat.normalID = content->GetTextureID("FlashlightLever_Normal");
	mat.specularID = content->GetTextureID("FlashlightLever_Specular");
	mat.glossinessID = content->GetTextureID("FlashlightLever_Glossiness");

	Entity *flashlightLeverMesh = nullptr;
	if (!GetScene()->CreateMeshEntity(&flashlightLeverMesh, "Flashlight Lever Mesh", meshID, mat, false, false, true))
	{
		ErrMsg("Failed to initialize flashlight lever");
		return false;
	}
	flashlightLeverMesh->SetSerialization(false);
	flashlightLeverMesh->SetParent(flashlightBodyMesh);

	SetRechargeLever(flashlightLeverMesh->GetTransform());

	meshID = content->GetMeshID("LightButton1");
	mat.textureID = content->GetTextureID("Blue");
	mat.ambientID = content->GetTextureID("Blue");

	Entity *flashlightIndicatorMesh = nullptr;
	if (!GetScene()->CreateMeshEntity(&flashlightIndicatorMesh, "Flashlight Indicator1 Mesh", meshID, mat, false, false, true))
	{
		ErrMsg("Failed to initialize flashlight Indicator");
		return false;
	}
	
	flashlightIndicatorMesh->SetSerialization(false);
	flashlightIndicatorMesh->SetParent(flashlightBodyMesh);
	flashlightIndicatorMesh->GetTransform()->SetPosition({ 0.1f, 0.0f, 0.0f });
	flashlightIndicatorMesh->GetTransform()->SetScale({ 1.0f, -1.0f, 1.0f });

	
	if (!GetScene()->CreateMeshEntity(&flashlightIndicatorMesh, "Flashlight Indicator2 Mesh", meshID, mat, false, false, true))
	{
		ErrMsg("Failed to initialize flashlight Indicator");
		return false;
	}
	
	flashlightIndicatorMesh->SetSerialization(false);
	flashlightIndicatorMesh->SetParent(flashlightBodyMesh);
	flashlightIndicatorMesh->GetTransform()->SetPosition({ 0.08f, 0.0f, -0.3f });
	flashlightIndicatorMesh->GetTransform()->SetScale({ 1.0f, -1.0f, 1.0f });

	
	if (!GetScene()->CreateMeshEntity(&flashlightIndicatorMesh, "Flashlight Indicator3 Mesh", meshID, mat, false, false, true))
	{
		ErrMsg("Failed to initialize flashlight Indicator");
		return false;
	}
	flashlightIndicatorMesh->SetSerialization(false);
	flashlightIndicatorMesh->SetParent(flashlightBodyMesh);
	flashlightIndicatorMesh->GetTransform()->SetPosition({ 0.06f, 0.0f, -0.6f });
	flashlightIndicatorMesh->GetTransform()->SetScale({ 1.0f, -1.0f, 1.0f });


	Entity *spotLight = nullptr;
	if (!GetScene()->CreateSpotLightEntity(&spotLight, "Flashlight Spotlight", 
		{ 1.0f, 0.95f, 0.85f }, 1.35f, 65.0f, false, 0.1f, 0, 0.5f))
	{
		ErrMsg("Failed to create spotlight entity to flashlight!");
		return false;
	}
	spotLight->SetSerialization(false);
	spotLight->SetParent(flashlightEntity);

	spotLight->GetBehaviourByType<SpotLightBehaviour>(_lightBehaviour);
	_lightBehaviour->SetEnabled(false);
	_lightBehaviour->SetIntensity(_lightStrengthMax);
	_lightBehaviour->SetEnabled(_isOn);


	_onSound = new SoundBehaviour("100hz_mono");
	if (!_onSound->Initialize(flashlightEntity))
	{
		ErrMsg("Failed to initialize flashlight behaviour!");
		return false;
	}
	_onSound->SetSerialization(false);
	_onSound->SetVolume(0.25f);
	
	_flickerSound = new SoundBehaviour("LightFlicker", (SOUND_EFFECT_INSTANCE_FLAGS)3U, true, 10.0f, 0.2f);
	if (!_flickerSound->Initialize(flashlightEntity))
	{
		ErrMsg("Failed to initialize flashlight flicker sound!");
		return false;
	}
	_flickerSound->SetSerialization(false);
	_flickerSound->SetVolume(0.0f);
	_flickerSound->Play();
	
	_toggleSound = new SoundBehaviour("ButtonPress", (SOUND_EFFECT_INSTANCE_FLAGS)3U, false, 10.0f, 0.35f);
	if (!_toggleSound->Initialize(flashlightEntity))
	{
		ErrMsg("Failed to initialize flashlight flicker sound!");
		return false;
	}
	_toggleSound->SetSerialization(false);
	_toggleSound->SetVolume(1.0f);

	QueueUpdate();
	QueueFixedUpdate();

	return true;
}

bool FlashlightBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (input.GetKey(_key) == KeyState::Held && _holdTime <= 0.55f)
	{
		_holdTime += time.GetDeltaTime();
	}

	if (_holdTime > 0.5f || _isCharging) // The flashlight is charging
	{
		_isCharging = true;
		Charge(time);

	}

	if (_battery > 0.0f)
	{
		// Adding ambience to índicators

		if (_battery <= 66.0f && !_batteryHighTreshHold) {
			TurnOffIndicator("Flashlight Indicator1 Mesh", _batteryHighTreshHold);
		}
		if (_battery >= 66.0f && _batteryHighTreshHold) {
			TurnOffIndicator("Flashlight Indicator1 Mesh", _batteryHighTreshHold);
		}
		if (_battery <= 33.0f && !_batteryMediumTreshHold) {

			TurnOffIndicator("Flashlight Indicator2 Mesh", _batteryMediumTreshHold);
		}
		if (_battery >= 33.0f && _batteryMediumTreshHold) {
			TurnOffIndicator("Flashlight Indicator2 Mesh", _batteryMediumTreshHold);
		}
		if (_battery <= 0.1f && !_batteryLowTreshHold) {
			TurnOffIndicator("Flashlight Indicator3 Mesh", _batteryLowTreshHold);
		}
		if (_battery >= 0.1f && _batteryLowTreshHold) {
			TurnOffIndicator("Flashlight Indicator3 Mesh", _batteryLowTreshHold);
		}

		if (input.GetKey(_key) == KeyState::Released && _holdTime < 0.5f)
		{
			ToggleFlashlight(!_isOn);
			_onSound->Pause();
			_isCharging = false;
		}

		MonsterBehaviour *monster = GetScene()->GetMonster();
		if (!monster)
			return true;

		XMFLOAT3 monsterPos = monster ? To3(monster->GetTransform()->GetPosition(World)) : XMFLOAT3(0,0,0);
		XMFLOAT3 flashlightPos = GetTransform()->GetPosition(World);
		float monsterDistSqr = XMVectorGetX(XMVector3LengthSq(Load(monsterPos) - Load(flashlightPos)));
		constexpr float monsterDrainDist = 20.0f;

		if (_isOn)
		{
			XMFLOAT3 fwd = GetTransform()->GetForward(World);

			XMFLOAT3 p, n;
			float depth;
			bool hit = false;

			const Collisions::Terrain *t = GetScene()->GetTerrain();

			if (t)
			{
				Collisions::Ray rayCollider = Collisions::Ray(flashlightPos, fwd, 20.0f);
				hit = Collisions::TerrainRayIntersection(*t, rayCollider, n, p, depth);
			}

			if (hit)
			{
				depth = (20.0f - depth);
			}
			else
			{
				depth = 20.0f;
			}

			Store(p, Load(flashlightPos) + Load(fwd) * depth);

			// Alert monster of enabled flashlight
			if (monster)
			{
				monster->Alert(1.5f * time.GetDeltaTime(), To3(flashlightPos), 7.5f, true, true);
				monster->Alert(1.0f * time.GetDeltaTime(), To3(p), 15.0f * (depth / 20.0f), true, true);
			}
		}

		if ((_isOn && !_isCharging) || monsterDistSqr <= monsterDrainDist*monsterDrainDist)
		{
			if (monsterDistSqr <= monsterDrainDist * monsterDrainDist)
			{
				float monsterDrain = 3.0f * std::clamp(
					1.0f - std::pow(std::sqrtf(monsterDistSqr) / monsterDrainDist, 4.0f), 
					0.0f, 1.0f
				);

				_battery -= monsterDrain * time.GetDeltaTime();
			}

			if (_isOn && !_isCharging)
				_battery -= _batteryDrainRate * time.GetDeltaTime();

			if (_battery < 0.0f)
			{
				_isDead = true;
				_isFlickering = false;
				_flickerTimer = 0.0f;
				_battery = 0.0f;
				ToggleFlashlight(false);
			}
		}

		if (_battery < _lightStrengthThreshold)
		{
			float t = 1.0f - pow(1.0f - (_battery / _lightStrengthThreshold), _lightFalloffExponent);

			float intensity = std::clamp(
				_lightStrengthMin + (_lightStrengthMax - _lightStrengthMin) * t, 
				_lightStrengthMin, 
				_lightStrengthMax
			);

			_lightBehaviour->SetIntensity(intensity);
		}
		else
		{
			_lightBehaviour->SetIntensity(_lightStrengthMax);
		}

		if (_isOn && _isFlickering)
		{
			if (!_isCharging)
			{
				if (_flickerTimer > 0.0f)
				{
					_flickerTimer -= time.GetDeltaTime();

					if (_flickerTimer <= 0.0f)
					{
						_lightBehaviour->SetEnabled(true);
						_flickerTimer = 0.0f;
						_isFlickering = false;
					}
				}
			}
		}
	}

	if (input.GetKey(_key) == KeyState::Released)
	{
		_holdTime = 0.0f;
	}

	_flickerSound->SetVolume(_flickerVolume);
	//_flickerSound->Play();

	//_toggleSound->Play();

	return true;
}

bool FlashlightBehaviour::FixedUpdate(float deltaTime, const Input &input)
{
	MonsterBehaviour *monster = GetScene()->GetMonster();
	if (!monster)
		return true;

	XMFLOAT3 monsterPos = monster ? To3(monster->GetTransform()->GetPosition(World)) : XMFLOAT3(0, 0, 0);
	XMFLOAT3 flashlightPos = GetEntity()->GetTransform()->GetPosition(World);
	float monsterDistSqr = XMVectorGetX(XMVector3LengthSq(Load(monsterPos) - Load(flashlightPos)));
	constexpr float monsterFlickerDist = 25.0f;

	if (_isOn && !_isCharging && _battery > 0.0f && (				// If the flashlight is on and not charging
		(_battery < _flickerThreshold) ||							// And the battery is low
		(monsterDistSqr <= monsterFlickerDist*monsterFlickerDist)))	// Or the monster is close
	{
		float monsterFlicker = std::clamp(0.2f * (1.0f - std::pow(monsterDistSqr / (monsterFlickerDist * monsterFlickerDist), 3.0f)), 0.0f, 1.0f);		
		float chance = monsterFlicker * 0.75f;
		if (_battery < _flickerThreshold) 
			chance += _flickerChance * pow(1.0f - _battery / _flickerThreshold, _flickerChanceExponent);

		_flickerVolume = pow(chance, 0.9f);
		if (rand() % 10000 < chance * 10000)
		{
			_lightBehaviour->SetEnabled(false);
			_flickerTimer = 0.5f * chance * (float)(rand() % 10000) / 10000.0f;
			_isFlickering = true;
		}
	}
	else
	{
		_flickerVolume = 0.0f;
	}

	return true;
}

#ifdef USE_IMGUI
bool FlashlightBehaviour::RenderUI()
{
	ImGui::Text(("Battery: " + std::to_string(_battery)).c_str());
	ImGui::Text(("HoldTime: " + std::to_string(_holdTime)).c_str());
	ImGui::Text(("Enabled: " + std::to_string(_isOn)).c_str());
	ImGui::Text(("Charging: " + std::to_string(_isCharging)).c_str());
	ImGui::Text(("Dead: " + std::to_string(_isDead)).c_str());

	return true;
}
#endif

float FlashlightBehaviour::GetMaxBattery() const
{
	return _batteryMax;
}

float FlashlightBehaviour::GetBattery() const
{
	return _battery;
}

void FlashlightBehaviour::DrainBattery()
{
	_battery = 0;

	TurnOffIndicator("Flashlight Indicator1 Mesh", _batteryHighTreshHold);
	TurnOffIndicator("Flashlight Indicator2 Mesh", _batteryMediumTreshHold);
	TurnOffIndicator("Flashlight Indicator3 Mesh", _batteryLowTreshHold);
}

void FlashlightBehaviour::SetBattry(float battery)
{
	_battery = std::clamp<float>(battery, 0, _batteryMax);
}

void FlashlightBehaviour::Charge(TimeUtils &time)
{
	_battery += _batteryChargeRate * time.GetDeltaTime();

	if (_isOn)
		ToggleFlashlight(false);

	_isDead = false;
	if (_battery < _batteryMax - 0.5f) // To make the sound not get spammed when holding charge for too long
		_onSound->Play();

	if (_battery > _batteryMax) // Battery is fully charged
	{
		_isCharging = false;
		_battery = _batteryMax;
		_onSound->Pause();
	}
	else
	{
		_rechargeLever->Rotate({ time.GetDeltaTime() * 12.0f , 0.0f, 0.0f }, Local);
	}
}

void FlashlightBehaviour::SetRechargeLever(Transform *lever)
{
	_rechargeLever = lever;
}

SpotLightBehaviour *FlashlightBehaviour::GetLight() const
{
	return _lightBehaviour;
}

bool FlashlightBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Battery", _battery, docAlloc);
	obj.AddMember("On", _isOn, docAlloc);
	obj.AddMember("Dead", _isDead, docAlloc);
	obj.AddMember("Charging", _isCharging, docAlloc);

	return true;
}

bool FlashlightBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_battery	= obj["Battery"].GetFloat();
	_isOn		= obj["On"].GetBool();
	_isDead		= obj["Dead"].GetBool();
	_isCharging	= obj["Charging"].GetBool();

	return true;
}

void FlashlightBehaviour::PostDeserialize()
{
	std::vector<Entity *> children{};
	GetEntity()->GetChildrenRecursive(children);

	for (Entity *entity : children)
	{
		std::string name = entity->GetName();
		if (name == "Flashlight Lever Mesh")
		{
			_rechargeLever = entity->GetTransform();
		}
	}
}

bool FlashlightBehaviour::IsOn() const
{
	return _isOn;
}

void FlashlightBehaviour::ToggleFlashlight(bool state)
{
	_isOn = state;
	_lightBehaviour->SetEnabled(_isOn);

	_toggleSound->ResetSound();
	_toggleSound->Play();
}

void FlashlightBehaviour::TurnOffIndicator(const std::string &name, bool &off)
{
	Entity* flashlightEntity = GetEntity();
	const std::vector<Entity*> *bodyChild = flashlightEntity->GetChildren();
	const std::vector<Entity*> *Children = bodyChild->at(0)->GetChildren();

	for (auto child : *Children) 
	{
		if (child->GetName() == name) 
		{
			MeshBehaviour* mesh = nullptr;
			if (!child->GetBehaviourByType(mesh))
				continue;

			Content* content = flashlightEntity->GetScene()->GetContent();
			Material mat = Material(*mesh->GetMaterial());
			mat.ambientID = off ? content->GetTextureID("Blue") : CONTENT_NULL;
			
			if (!mesh->SetMaterial(&mat))
			{
				ErrMsg("Failed to set material for flashlight indicator");
			}

			break;
		}
	}

	off = !off;
}
