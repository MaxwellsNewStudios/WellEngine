#include "stdafx.h"
#include "Behaviours/SoundBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


SoundBehaviour::SoundBehaviour(std::string fileName, dx::SOUND_EFFECT_INSTANCE_FLAGS flags, 
	bool loop, float distanceScaler, float reverbScaler)
{
	_fileName = std::move(fileName);
	_soundEffectFlag = flags;
	_loop = loop;
	_distanceScaler = distanceScaler;
	_reverbScaler = reverbScaler;
}

bool SoundBehaviour::Start()
{
	if (_name.empty())
		_name = "SoundBehaviour"; // For categorization in ImGui.

	if (_fileName.empty())
		return true;

	if (!_soundSource.Initialize(GetScene()->GetSoundEngine()->GetAudioEngine(), 
		_soundEffectFlag, _fileName, _distanceScaler, _reverbScaler))
	{
		ErrMsg("Failed to initialize sound source " + _fileName);
		return false;
	}

	_soundSource.SetListenerPosition(_listenerPos);
	_soundSource.SetEmitterPosition(_emitterPos);

	SetVolume(_volume);

	_length = _soundSource.GetSoundLength() / 1000.0f;
	_isValid = true;

	QueueUpdate();

	return true;
}

bool SoundBehaviour::Update(TimeUtils &time, const Input &input)
{
	dx::SoundState soundState = _soundSource.GetSoundState();

	if (soundState == dx::SoundState::PLAYING)
	{
		UpdatePosition();
		_duration += time.GetDeltaTime();
	}

	if (_duration >= _length) // Reached end of sound
	{
		if (_loop)
		{
			_duration -= _length;
		}
		else
		{
			ResetSound();
			Pause();
		}
	}

	return true;
}

#ifdef USE_IMGUI
bool SoundBehaviour::RenderUI()
{
	Scene *scene = GetScene();
	Entity *soundSource = GetEntity();
	dx::XMFLOAT3A emitterPos = soundSource->GetTransform()->GetPosition(World);

	Transform *viewCamTransform = scene->GetViewCamera()->GetTransform();
	dx::XMFLOAT3A listenerPos = viewCamTransform->GetPosition(World);

	// Playback
	{
		if (ImGui::Button("Play"))
			Play();
		ImGui::SameLine(); ImGui::Dummy({ 2.0f, 0.0f }); ImGui::SameLine();

		if (ImGui::Button("Pause"))
			Pause();
		ImGui::SameLine(); ImGui::Dummy({ 2.0f, 0.0f }); ImGui::SameLine();

		if (ImGui::Button("Reset"))
			ResetSound();
		ImGui::SameLine(); ImGui::Dummy({ 2.0f, 0.0f }); ImGui::SameLine();

		ImGui::Checkbox("Loop", &_loop);

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), std::format("Duration: {} / {}", _duration, _length).c_str());
	}
	ImGui::Separator();

	// Sound File
	{
		static std::string soundName = _fileName;
		bool reinitialize = false;
		bool foundFile = false;

		ImGui::Text("Sound Name:");
		ImGui::SameLine();
		if (ImGui::InputText("##SoundName", &soundName))
		{
			std::string fileName = PATH_FILE_EXT(ASSET_PATH_SOUNDS, soundName, "wav");
			struct stat buffer;
			reinitialize = foundFile = stat(fileName.c_str(), &buffer) == 0;
		}
		ImGui::SetItemTooltip(std::format("Name of the sound file you want to use, located in {}.", ASSET_PATH_SOUNDS).c_str());

		if (ImGui::Button("Browse"))
		{
			const char *filterPatterns[] = { "*.wav" };
			const char *selectedFiles = tinyfd_openFileDialog(
				"Open File",
				PATH_FILE(ASSET_PATH_SOUNDS, "").c_str(),
				1,
				filterPatterns,
				"Supported Files",
				0
			);

			if (selectedFiles)
			{
				std::string fileString = selectedFiles;

				std::vector<std::string> filePaths;
				std::stringstream ss(fileString);
				std::string filePath;
				while (std::getline(ss, filePath, '|'))
				{
					filePaths.emplace_back(filePath);
				}

				if (filePaths.size() > 0)
				{
					soundName = filePaths[0].substr(filePaths[0].find_last_of('\\') + 1);
					soundName = soundName.substr(0, soundName.find_last_of('.'));

					std::string fileName = PATH_FILE_EXT(ASSET_PATH_SOUNDS, soundName, "wav");
					struct stat buffer;
					reinitialize = foundFile = stat(fileName.c_str(), &buffer) == 0;
				}
			}
		}

		if (foundFile)
		{
			// Reinitialize sound source with new file name
			_fileName = soundName;
			if (!Start())
				ErrMsg("Failed to reinitialize sound!")
		}
	}
	ImGui::Separator();

	// Settings
	{
		ImGui::Text("Volume:"); ImGui::SameLine();
		if (ImGui::DragFloat("##AdjustVolume", &_volume, 0.1f, 0.000001f))
			_soundSource.AdjustVolume(_volume);
		ImGuiUtils::LockMouseOnActive();

		float distanceScaler = _soundSource.GetDistanceScaler();
		ImGui::Text("Distance:"); ImGui::SameLine();
		if (ImGui::DragFloat("##AdjustDistance", &distanceScaler, 0.25f, 0.000001f))
			_soundSource.SetDistanceScaler(distanceScaler);
		ImGuiUtils::LockMouseOnActive();

		float reverbScaler = _soundSource.GetReverbScaler();
		ImGui::Text("Reverb:"); ImGui::SameLine();
		if (ImGui::DragFloat("##AdjustReverb", &reverbScaler, 0.01f, 0.000001f))
			_soundSource.SetReverbScaler(reverbScaler);
		ImGuiUtils::LockMouseOnActive();

		if (ImGui::TreeNode("Set Listener Cone"))
		{
			static X3DAUDIO_CONE newListenerCone = AudioPresets::ListenerCone;
			static bool applyContinuously = false;

			if (ImGui::Button("Get Current##Listener"))
				newListenerCone = _soundSource.GetListenerCone();

			bool modified = false;
			modified |= ImGui::SliderFloat("Inner Angle##Listener", &newListenerCone.InnerAngle, 0.0f, newListenerCone.OuterAngle);
			modified |= ImGui::SliderFloat("Outer Angle##Listener", &newListenerCone.OuterAngle, newListenerCone.InnerAngle, X3DAUDIO_2PI);
			modified |= ImGui::SliderFloat("Inner Volume##Listener", &newListenerCone.InnerVolume, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Outer Volume##Listener", &newListenerCone.OuterVolume, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Inner LPF##Listener", &newListenerCone.InnerLPF, 0.0f, 1.0f);
			modified |= ImGui::SliderFloat("Outer LPF##Listener", &newListenerCone.OuterLPF, 0.0f, 1.0f);
			modified |= ImGui::SliderFloat("Inner Reverb##Listener", &newListenerCone.InnerReverb, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Outer Reverb##Listener", &newListenerCone.OuterReverb, 0.0f, 2.0f);

			ImGui::Separator();

			modified |= ImGui::Checkbox("Continuous##Listener", &applyContinuously);
			ImGui::SameLine();
			if (ImGui::Button("Apply##Listener") || (applyContinuously && modified))
			{
				_soundSource.SetListenerCone(newListenerCone);
				_soundSource.SetEmitterPosition(emitterPos); // Reapply emitter position to update cone
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Set Emitter Cone"))
		{
			static X3DAUDIO_CONE newEmitterCone = AudioPresets::EmitterCone;
			static bool applyContinuously = false;

			if (ImGui::Button("Get Current##Emitter"))
				newEmitterCone = _soundSource.GetEmitterCone() ? *_soundSource.GetEmitterCone() : newEmitterCone;

			bool modified = false;
			modified |= ImGui::SliderFloat("Inner Angle##Emitter", &newEmitterCone.InnerAngle, 0.0f, newEmitterCone.OuterAngle);
			modified |= ImGui::SliderFloat("Outer Angle##Emitter", &newEmitterCone.OuterAngle, newEmitterCone.InnerAngle, X3DAUDIO_2PI);
			modified |= ImGui::SliderFloat("Inner Volume##Emitter", &newEmitterCone.InnerVolume, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Outer Volume##Emitter", &newEmitterCone.OuterVolume, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Inner LPF##Emitter", &newEmitterCone.InnerLPF, 0.0f, 1.0f);
			modified |= ImGui::SliderFloat("Outer LPF##Emitter", &newEmitterCone.OuterLPF, 0.0f, 1.0f);
			modified |= ImGui::SliderFloat("Inner Reverb##Emitter", &newEmitterCone.InnerReverb, 0.0f, 2.0f);
			modified |= ImGui::SliderFloat("Outer Reverb##Emitter", &newEmitterCone.OuterReverb, 0.0f, 2.0f);

			ImGui::Separator();

			modified |= ImGui::Checkbox("Continuous##Emitter", &applyContinuously);
			ImGui::SameLine();
			if (ImGui::Button("Apply##Emitter") || (applyContinuously && modified))
			{
				_soundSource.SetEmitterCone(&newEmitterCone);
				_soundSource.SetEmitterPosition(emitterPos); // Reapply emitter position to update cone
			}
			ImGui::SameLine();
			if (ImGui::Button("Toggle##Emitter"))
			{
				_soundSource.SetEmitterCone(_soundSource.GetEmitterCone() ? nullptr : &newEmitterCone);
				_soundSource.SetEmitterPosition(emitterPos); // Reapply emitter position to update cone
			}

			ImGui::TreePop();
		}
	}
	ImGui::Separator();

	// Positional Info
	{
		ImGui::TextColored(ImVec4(1, 1, 1, 1),
			std::format("Listener Pos:  ({}, {}, {})", listenerPos.x, listenerPos.y, listenerPos.z).c_str()
		);

		ImGui::TextColored(ImVec4(1, 1, 1, 1),
			std::format("Emitter Pos:   ({}, {}, {})", emitterPos.x, emitterPos.y, emitterPos.z).c_str()
		);
	}

	return true;
}
#endif

void SoundBehaviour::OnEnable()
{
	_play = true;
}
void SoundBehaviour::OnDisable()
{
	_play = false;

	dx::SoundState soundState = _soundSource.GetSoundState();
	if (soundState == dx::SoundState::PLAYING)
		Pause();
}

bool SoundBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	json::Value fileNameStr(json::kStringType);
	fileNameStr.SetString(_fileName.c_str(), docAlloc);
	obj.AddMember("File Name", fileNameStr, docAlloc);
	obj.AddMember("Sound Effect Flag", (uint32_t)_soundEffectFlag, docAlloc); // TODO: Shorten name
	obj.AddMember("Volume", _volume, docAlloc);
	obj.AddMember("Loop", _loop, docAlloc);
	obj.AddMember("Play", _play, docAlloc);
	obj.AddMember("Distance Scaler", _distanceScaler, docAlloc);
	obj.AddMember("Reverb Scaler", _reverbScaler, docAlloc);

	return true;
}
bool SoundBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_fileName = obj["File Name"].GetString();
	_soundEffectFlag = (dx::SOUND_EFFECT_INSTANCE_FLAGS)(obj["Sound Effect Flag"].GetUint()); // TODO: Shorten name
	_volume = obj["Volume"].GetFloat();
	_loop = obj["Loop"].GetBool();
	_play = obj["Play"].GetBool();
	_distanceScaler = obj["Distance Scaler"].GetFloat();
	_reverbScaler = obj["Reverb Scaler"].GetFloat();

	return true;
}


void SoundBehaviour::Play()
{
	if (!_isValid)
		return;

	if (!_play)
		return;

	UpdatePosition();
	_soundSource.PlayAudio();
}
void SoundBehaviour::Pause()
{
	if (!_isValid)
		return;

	_soundSource.PauseAudio();
}
void SoundBehaviour::ResetSound()
{
	if (!_isValid)
		return;

	_soundSource.ResetSound();
	_duration = 0.0f;
	SetVolume(_volume);
}

float SoundBehaviour::GetSoundLength() const
{
	return _length;
}
dx::SoundState SoundBehaviour::GetSoundState()
{
	return _soundSource.GetSoundState();
}

dx::XMFLOAT3 SoundBehaviour::GetListenerPosition() const
{
	return _listenerPos;
}
dx::XMFLOAT3 SoundBehaviour::GetEmitterPosition() const
{
	return _emitterPos;
}

void SoundBehaviour::SetListenerPosition(dx::XMFLOAT3 position)
{
	_listenerPos = position;
	_soundSource.SetListenerPosition(_listenerPos);
}
void SoundBehaviour::SetEmitterPosition(dx::XMFLOAT3 position)
{
	_emitterPos = position;
	_soundSource.SetEmitterPosition(_emitterPos);
}

void SoundBehaviour::SetVolume(float volume)
{
	_volume = volume;
	_soundSource.AdjustVolume(_volume);
}
void SoundBehaviour::SetLoop(bool state)
{
	_loop = state;
}
void SoundBehaviour::SetSoundEffectFlag(dx::SOUND_EFFECT_INSTANCE_FLAGS flag)
{
	_soundEffectFlag = flag;
}

void SoundBehaviour::UpdatePosition()
{
	SetEmitterPosition(GetEntity()->GetTransform()->GetPosition(World));

	CameraBehaviour *viewCamera = GetScene()->GetViewCamera();
	if (viewCamera)
	{
		Transform *viewCamTransform = viewCamera->GetTransform();
		dx::XMFLOAT3A listenerPos = viewCamTransform->GetPosition(World);
		dx::XMFLOAT3A forwardVec = viewCamTransform->GetForward(World);
		dx::XMFLOAT3A upVec = viewCamTransform->GetUp(World);

		SetListenerPosition(listenerPos);
		_soundSource.SetListenerOrientation(forwardVec, upVec);
	}

	_soundSource.UpdateAudio();
}
