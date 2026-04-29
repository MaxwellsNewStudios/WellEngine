#include "stdafx.h"
#include "B_SoundSource.h"
#include "Game/Scene/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


 B_SoundSource::B_SoundSource(std::string fileName, dx::SOUND_EFFECT_INSTANCE_FLAGS flags, 
	bool loop, float distanceScaler, float reverbScaler)
{
	_fileName = std::move(fileName);
	_soundEffectFlag = flags;
	_loop = loop;
	_distanceScaler = distanceScaler;
	_reverbScaler = reverbScaler;
}

bool B_SoundSource::Start()
{
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

bool B_SoundSource::Update(TimeUtils &time, const Input &input)
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
bool B_SoundSource::RenderUI()
{
	Scene *scene = GetScene();
	Entity *soundSource = GetEntity();
	dx::XMFLOAT3A emitterPos = soundSource->GetTransform()->GetPosition(World);

	Transform *viewCamTransform = scene->GetMainCamera()->GetTransform();
	dx::XMFLOAT3A listenerPos = viewCamTransform->GetPosition(World);

	// Sound File
	{
		static std::string soundName = _fileName;
		bool reinitialize = false;
		bool foundFile = false;

		float inputWidth = ImGui::GetContentRegionAvail().x - 32.0f; // Leave space for Browse button
		ImGui::SetNextItemWidth(inputWidth);
		if (ImGui::InputText("##SoundName", &soundName))
		{
			std::string fileName = WE_DFE(WE_D_ASSET_SOUND, soundName, "wav");
			struct stat buffer;
			reinitialize = foundFile = stat(fileName.c_str(), &buffer) == 0;
		}

		ImGui::SetItemTooltip(std::format("Name of the sound file you want to use, located in {}.", WE_D_ASSET_SOUND).c_str());

		ImGui::SameLine();
		if (ImGuiUtils::ButtonWithFont(ICON_LC_FOLDER_SEARCH "##Browse", FONT_ICON_FILE_NAME_LC, 14.0f))
		{
			const char *filterPatterns[] = { "*.wav" };
			const char *selectedFiles = tinyfd_openFileDialog(
				"Open Sound File",
				WE_DF(WE_D_ASSET_SOUND, "").c_str(),
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

					std::string fileName = WE_DFE(WE_D_ASSET_SOUND, soundName, "wav");
					struct stat buffer;
					reinitialize = foundFile = stat(fileName.c_str(), &buffer) == 0;
				}
			}
		}

		ImGui::SetItemTooltip("Open File Browser");

		if (foundFile)
		{
			// Reinitialize sound source with new file name
			_fileName = soundName;
			if (!Start())
				ErrMsg("Failed to reinitialize sound!")
		}
	}
	ImGui::Dummy({ 0.0f, 1.0f });

	// Playback
	{
		if (ImGui::Button("Play"))
			Play();
		ImGui::SameLine(0.0f, 8.0f);

		if (ImGui::Button("Pause"))
			Pause();
		ImGui::SameLine(0.0f, 8.0f);

		if (ImGui::Button("Reset"))
			ResetSound();
		ImGui::SameLine(0.0f, 8.0f);

		ImGui::Checkbox("Loop", &_loop);
		ImGui::Dummy({ 0.0f, 1.0f });

		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), std::format("Duration: {} / {}", _duration, _length).c_str());
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

void B_SoundSource::OnEnable()
{
	_play = true;
}
void B_SoundSource::OnDisable()
{
	_play = false;

	dx::SoundState soundState = _soundSource.GetSoundState();
	if (soundState == dx::SoundState::PLAYING)
		Pause();
}

bool B_SoundSource::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("File Name",			SerializerUtils::SerializeString(_fileName, docAlloc), docAlloc);
	obj.AddMember("Flag",				(uint32_t)_soundEffectFlag, docAlloc);
	obj.AddMember("Volume",				_volume, docAlloc);
	obj.AddMember("Loop",				_loop, docAlloc);
	obj.AddMember("Distance Scaler",	_distanceScaler, docAlloc);
	obj.AddMember("Reverb Scaler",		_reverbScaler, docAlloc);

	return true;
}
bool B_SoundSource::Deserialize(const json::Value &obj, Scene *scene)
{
	_fileName			= obj["File Name"].GetString();

	if (obj.HasMember("Flag"))
		_soundEffectFlag = (dx::SOUND_EFFECT_INSTANCE_FLAGS)(obj["Flag"].GetUint());
	else if (obj.HasMember("Sound Effect Flag")) // For backward compatibility
		_soundEffectFlag = (dx::SOUND_EFFECT_INSTANCE_FLAGS)(obj["Sound Effect Flag"].GetUint());

	_volume				= obj["Volume"].GetFloat();
	_loop				= obj["Loop"].GetBool();
	_distanceScaler		= obj["Distance Scaler"].GetFloat();
	_reverbScaler		= obj["Reverb Scaler"].GetFloat();

	return true;
}


void B_SoundSource::Play()
{
	if (!_isValid)
		return;

	if (!_play)
		return;

	UpdatePosition();
	_soundSource.PlayAudio();
}
void B_SoundSource::Pause()
{
	if (!_isValid)
		return;

	_soundSource.PauseAudio();
}
void B_SoundSource::ResetSound()
{
	if (!_isValid)
		return;

	_soundSource.ResetSound();
	_duration = 0.0f;
	SetVolume(_volume);
}

float B_SoundSource::GetSoundLength() const
{
	return _length;
}
dx::SoundState B_SoundSource::GetSoundState()
{
	return _soundSource.GetSoundState();
}

dx::XMFLOAT3 B_SoundSource::GetListenerPosition() const
{
	return _listenerPos;
}
dx::XMFLOAT3 B_SoundSource::GetEmitterPosition() const
{
	return _emitterPos;
}

void B_SoundSource::SetListenerPosition(dx::XMFLOAT3 position)
{
	_listenerPos = position;
	_soundSource.SetListenerPosition(_listenerPos);
}
void B_SoundSource::SetEmitterPosition(dx::XMFLOAT3 position)
{
	_emitterPos = position;
	_soundSource.SetEmitterPosition(_emitterPos);
}

void B_SoundSource::SetVolume(float volume)
{
	_volume = volume;
	_soundSource.AdjustVolume(_volume);
}
void B_SoundSource::SetLoop(bool state)
{
	_loop = state;
}
void B_SoundSource::SetSoundEffectFlag(dx::SOUND_EFFECT_INSTANCE_FLAGS flag)
{
	_soundEffectFlag = flag;
}

void B_SoundSource::UpdatePosition()
{
	SetEmitterPosition(GetEntity()->GetTransform()->GetPosition(World));

	B_Camera *viewCamera = GetScene()->GetMainCamera();
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
