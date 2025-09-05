#include "stdafx.h"
#include "Behaviours/AmbientSoundBehaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

AmbientSoundBehaviour::AmbientSoundBehaviour(std::string fileName, dx::SOUND_EFFECT_INSTANCE_FLAGS flags, bool loop, float volume,
	float distanceScaler, float reverbScaler, float minimumDelay, float maximumDelay)
{
	_fileName = std::move(fileName);
	_soundEffectFlag = dx::SoundEffectInstance_Use3D | dx::SoundEffectInstance_ReverbUseFilters;
	_loop = loop;
	_volume = volume;
	_distanceScaler = distanceScaler;
	_reverbScaler = reverbScaler;
	_delayMin = minimumDelay;
	_delayMax = maximumDelay;
}

bool AmbientSoundBehaviour::Start()
{
    if (_name.empty())
		_name = "AmbientSoundBehaviour";

	_soundBehaviour = new SoundBehaviour(_fileName, _soundEffectFlag, _loop, _distanceScaler, _reverbScaler);
	if (!_soundBehaviour->Initialize(GetEntity()))
	{
		Warn("Failed to Initialize sound behaviour!");
		return true;
	}

	_soundBehaviour->SetVolume(_volume);
	_soundBehaviour->SetSerialization(false);
	_soundBehaviour->SetEmitterPosition(GetEntity()->GetTransform()->GetPosition(World));

	_timer = RandomFloat(_delayMin, _delayMax);

#ifdef DEBUG_BUILD
	Content *content = GetScene()->GetContent();

	Material mat;
	mat.textureID = content->GetTextureID("SoundEmitter");
	mat.ambientID = content->GetTextureID("White");

	auto billboardMeshBehaviour = new BillboardMeshBehaviour(mat, 0.0f, 0.0f, 0.5f, true, false, false, false, true);
	if (!billboardMeshBehaviour->Initialize(GetEntity()))
		Warn("Failed to Initialize billboard mesh behaviour!");
#endif

	QueueUpdate();

    return true;
}

bool AmbientSoundBehaviour::Update(TimeUtils & time, const Input& input)
{
	dx::SoundState soundState = _soundBehaviour->GetSoundState();

	if (soundState != dx::SoundState::PLAYING)
	{
		_timer -= time.GetDeltaTime();

		if (_timer <= 0.0f)
		{
			_soundBehaviour->Play();
			_timer += RandomFloat(_delayMin, _delayMax);
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool AmbientSoundBehaviour::RenderUI()
{
	ImGui::Text("Play countdown: %.1f", _timer);
	if (ImGui::Button("Play", ImVec2(50, 20)))
		TriggerSound();

	const float pointOne = 0.1f;
	float vol = _volume;
	ImGui::InputScalar("Volume", ImGuiDataType_Float, &vol, &pointOne);
	if (vol != _volume)
	{
		_volume = round(vol * 1000) / 1000;
		_soundBehaviour->SetVolume(_volume);
	}

	ImGui::Checkbox("Loop", &_loop);

	ImGui::DragFloat("Min Delay", &_delayMin, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Max Delay", &_delayMax, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Distance scaler", &_distanceScaler, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Reverb scaler", &_reverbScaler, 0.01f);
	ImGuiUtils::LockMouseOnActive();

	_delayMin = _delayMin < 0.0f ? 0.0f : round(_delayMin * 1000) / 1000;
	_delayMax = _delayMax < _delayMin ? _delayMin : round(_delayMax * 1000) / 1000;
	_distanceScaler = _distanceScaler < 0.0f ? 0.0f : round(_distanceScaler * 1000) / 1000;
	_reverbScaler = _reverbScaler < 0.0f ? 0.0f : round(_reverbScaler * 1000) / 1000;

	return true;
}
#endif

bool AmbientSoundBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	json::Value fileNameStr(json::kStringType);
	fileNameStr.SetString(_fileName.c_str(), docAlloc);
	obj.AddMember("File Name", fileNameStr, docAlloc);
	obj.AddMember("Sound Effect Flag", (uint32_t)_soundEffectFlag, docAlloc); // TODO: Shorten name
	obj.AddMember("Volume", _volume, docAlloc);
	obj.AddMember("Loop", _loop, docAlloc);
	obj.AddMember("Distance Scaler", _distanceScaler, docAlloc);
	obj.AddMember("Reverb Scaler", _reverbScaler, docAlloc);
	obj.AddMember("Delay Min", _delayMin, docAlloc);
	obj.AddMember("Delay Max", _delayMax, docAlloc);
	return true;
}
bool AmbientSoundBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_fileName			= obj["File Name"].GetString();
	_soundEffectFlag	= (dx::SOUND_EFFECT_INSTANCE_FLAGS)(obj["Sound Effect Flag"].GetUint()); // TODO: Shorten name
	_volume				= obj["Volume"].GetFloat();
	_loop				= obj["Loop"].GetBool();
	_distanceScaler		= obj["Distance Scaler"].GetFloat();
	_reverbScaler		= obj["Reverb Scaler"].GetFloat();
	_delayMin			= obj["Delay Min"].GetFloat();
	_delayMax			= obj["Delay Max"].GetFloat();

	return true;
}


void AmbientSoundBehaviour::TriggerSound()
{
	dx::SoundState soundState = _soundBehaviour->GetSoundState();

	if (soundState != dx::SoundState::PLAYING)
	{
		_soundBehaviour->Play();
		_timer = RandomFloat(_delayMin, _delayMax);
	}
}

SoundBehaviour *AmbientSoundBehaviour::GetSoundBehaviour() const
{
	return _soundBehaviour;
}
