#include "stdafx.h"
#include "Behaviour.h"
#include "BehaviourRegistry.h"
#include "Behaviours/Rendering/Camera/CameraBehaviour.h"
#include "Source/Engine/Rendering/RenderQueuer.h"
#include "Scenes/Scene.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

Behaviour::~Behaviour()
{
	_isDestroyed = true;

	if (_isInitialized)
	{
		DequeueUpdate();
		DequeueParallelUpdate();
		DequeueLateUpdate();
		DequeueFixedUpdate();
	}
}

bool Behaviour::Initialize(Entity *entity, const std::string &behaviourName)
{
	ZoneScopedC(RandomUniqueColor());

	if (_isInitialized)
	{
		Warn("Behaviour is already initialized!");
		return true;
	}
	_isDestroyed = false;

	if (!entity)
	{
		Warn("Entity is null!");
		return true;
	}

	_entity = entity;
	entity->AddBehaviour(this);

	if (behaviourName.empty())
	{
		_name = behaviourName;
	}

	if (!Start())
	{
		ErrMsg("Failed to initialize behaviour!");
		return false;
	}

	if (_isDestroyed)
		return true;

#ifdef DEBUG_BUILD
	if (_name.empty())
		Warn("Behaviour name is empty! Did you forget to assign a name in Start?");
#endif

	ZoneTextX(_name.c_str(), _name.size());

	_isInitialized = true;
	return true;
}
bool Behaviour::IsInitialized() const
{
	return _isInitialized;
}

void Behaviour::Destroy()
{
	ZoneScopedC(RandomUniqueColor());
	std::string zoneName = std::format("Destroy '{}'", GetName());
	ZoneTextX(zoneName.c_str(), zoneName.size());

	if (_isDestroyed)
		return;

	if (!_entity)
		return;

	MarkDestroyed();
	_entity->RemoveBehaviour(this);
}
bool Behaviour::IsDestroyed() const
{
	return _isDestroyed;
}
void Behaviour::MarkDestroyed()
{
	_isDestroyed = true;
}

void Behaviour::QueueUpdate()
{
	GetScene()->AddUpdateCallback(this);
}
void Behaviour::DequeueUpdate()
{
	GetScene()->RemoveUpdateCallback(this);
}
void Behaviour::QueueParallelUpdate()
{
	GetScene()->AddParallelUpdateCallback(this);
}
void Behaviour::DequeueParallelUpdate()
{
	GetScene()->RemoveParallelUpdateCallback(this);
}
void Behaviour::QueueLateUpdate()
{
	GetScene()->AddLateUpdateCallback(this);
}
void Behaviour::DequeueLateUpdate()
{
	GetScene()->RemoveLateUpdateCallback(this);
}
void Behaviour::QueueFixedUpdate()
{
	GetScene()->AddFixedUpdateCallback(this);
}
void Behaviour::DequeueFixedUpdate()
{
	GetScene()->RemoveFixedUpdateCallback(this);
}

void Behaviour::SetSerialization(bool state)
{
	_doSerialize = state;
}
bool Behaviour::IsSerializable() const
{
	return _doSerialize;
}

Entity *Behaviour::GetEntity() const
{
	return _entity;
}
Transform *Behaviour::GetTransform() const
{
	if (!_entity)
		return nullptr;

	return _entity->GetTransform();
}
Scene *Behaviour::GetScene() const
{
	if (!_entity)
		return nullptr;

	return _entity->GetScene();
}
Game *Behaviour::GetGame() const
{
	if (!_entity)
		return nullptr;

	return _entity->GetGame();
}

void Behaviour::SetName(const std::string &name)
{
	_name = name;
}
const std::string &Behaviour::GetName() const
{
	return _name;
}

bool Behaviour::IsEnabled() const
{
	return _entity->IsEnabled() && _isEnabledSelf;
}
bool Behaviour::IsEnabledSelf() const
{
	return _isEnabledSelf;
}
void Behaviour::InheritEnabled(bool state)
{
	if (!_isEnabledSelf)
		return;

	if (state)
		OnEnable();
	else
		OnDisable();
}
void Behaviour::SetEnabled(bool state)
{
	if (_isEnabledSelf == state)
		return;

	_isEnabledSelf = state;

	if (_entity)
	{
		if (!_entity->IsEnabled())
			return;
	}

	if (state)
		OnEnable();
	else
		OnDisable();
}

void Behaviour::SetDirty()
{
	OnDirty();
}

bool Behaviour::InitialUpdate(TimeUtils &time, const Input &input)
{
	if (!IsEnabled())
		return true;

	if (_entity->IsRemoved())
		return true;

	if (!_isInitialized)
	{
		Warn("Behaviour is not initialized!");
		return true;
	}

	ZoneScopedXC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	return Update(time, input);
}
bool Behaviour::InitialParallelUpdate(const TimeUtils &time, const Input &input)
{
	if (!IsEnabled())
		return true;

	if (_entity->IsRemoved())
		return true;

	if (!_isInitialized)
	{
#pragma omp critical
		{
			Warn("Behaviour is not initialized!");
		}
		return true;
	}

	ZoneScopedXC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	return ParallelUpdate(time, input);
}
bool Behaviour::InitialLateUpdate(TimeUtils &time, const Input &input)
{
	if (!IsEnabled())
		return true;

	if (_entity->IsRemoved())
		return true;

#ifdef DEBUG_BUILD
	if (!_isInitialized)
	{
		Warn("Behaviour is not initialized!");
		return true;
	}
#endif

	ZoneScopedXC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	return LateUpdate(time, input);
}
bool Behaviour::InitialFixedUpdate(float deltaTime, const Input &input)
{
	if (!IsEnabled())
		return true;

	if (_entity->IsRemoved())
		return true;

#ifdef DEBUG_BUILD
	if (!_isInitialized)
	{
		Warn("Behaviour is not initialized!");
		return true;
	}
#endif

	ZoneScopedXC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	return FixedUpdate(deltaTime, input);
}
bool Behaviour::InitialBeforeRender()
{
	ZoneScopedXC(RandomUniqueColor());
	const std::string &name = GetName();
	ZoneTextX(name.c_str(), name.size());

	return BeforeRender();
}
bool Behaviour::InitialRender(const RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
#ifdef DEBUG_BUILD
	if (!_isInitialized)
	{
		Warn("Behaviour is not initialized!");
		return true;
	}
#endif

	if (!GetEntity())
		return true;

	if (GetEntity()->IsRemoved())
		return true;

	if (!IsEnabled())
		return true;

	return Render(queuer, rendererInfo);
}
#ifdef USE_IMGUI
int Behaviour::PopUIOpenState()
{
	int state = _uiOpen;
	_uiOpen = -1;
	return state;
}
void Behaviour::SetUIOpen(bool state)
{
	_uiOpen = state ? 1 : 0;
}
float Behaviour::GetUISize() const
{
	return _uiMaxSize;
}
void Behaviour::SetUISize(float maxSize)
{
	_uiMaxSize = maxSize;
}
bool Behaviour::GetUIMaximized() const
{
	return _uiMaximized;
}
void Behaviour::SetUIMaximized(bool state)
{
	_uiMaximized = state;
}
bool Behaviour::IsResizingUI() const
{
	return _uiIsResizing;
}
void Behaviour::SetResizingUI(bool state)
{
	_uiIsResizing = state;
}
bool Behaviour::IsUIDirty() const
{
	return _uiSizeDirty;
}
void Behaviour::SetUIDirty(bool state)
{
	_uiSizeDirty = state;
}

bool Behaviour::InitialRenderUI()
{
	ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Yellow);
	ImGuiUtils::BeginFont(FONT_ICON_FILE_NAME_FAS, 18.0f);
	float codeButtonWidth = ImGui::CalcTextSize(ICON_FA_FILE_CODE).x + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;
	if (ImGui::Button(ICON_FA_FILE_CODE, ImVec2(codeButtonWidth, 25.0f)))
	{
		// Get path from Behaviour Registry
		const std::string &behCategory = BehaviourRegistry::GetCategories().at(_name);
		std::string scriptPath = std::format(BEHAVIOURS_PATH "/{}{}.cpp", behCategory, _name);

		// Replace all / with \ for Windows
		std::replace(scriptPath.begin(), scriptPath.end(), '/', '\\');

		DbgMsgF("Opening '{}'", scriptPath);

		// Open script with default program
		ShellExecuteA(nullptr, "open", scriptPath.c_str(), nullptr, nullptr, SW_SHOW);
	}
	ImGuiUtils::EndFont();
	ImGuiUtils::EndButtonStyle();
	ImGui::SetItemTooltip("Open Script in Default Editor");

	ImGui::SameLine(0.0f, 10.0f);
	ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
	if (ImGui::Button("Delete", { 60.0f, 25.0f }))
	{
		_entity->RemoveBehaviour(this);
		ImGuiUtils::EndButtonStyle();
		return true;
	}
	ImGuiUtils::EndButtonStyle();

	ImGui::SameLine(0.0f, 10.0f);
	bool behEnabled = IsEnabledSelf();
	if (ImGui::Checkbox("Active##behActive", &behEnabled))
	{
		if (behEnabled)
			SetEnabled(true);
		else
			SetEnabled(false);
	}

	ImGui::SameLine(0.0f, 10.0f);
	ImGui::Checkbox("Serialized##behSerialize", &_doSerialize);

	ImGui::SameLine();
	std::string refText = std::format("References: {}", GetRefs().size());
	float refTextWidth = ImGui::CalcTextSize(refText.c_str()).x + 4.0f;
	float availWidth = ImGui::GetContentRegionAvail().x;
	ImGui::NewLine();
	float refTextoffset = max(2.0f, availWidth - refTextWidth);

	ImGui::SameLine(0.0f, refTextoffset);
	ImGui::Text(refText.c_str());

	ImGui::Dummy({ 0.0f, 0.0f });
	ImGui::Separator();
	ImGui::Dummy({ 0.0f, 2.0f });

	return RenderUI();
}
#endif
bool Behaviour::InitialBindBuffers(ID3D11DeviceContext *context)
{
	if (!IsEnabled())
		return true;

#ifdef DEBUG_BUILD
	if (!_isInitialized)
	{
		Warn("Behaviour is not initialized!");
		return true;
	}
#endif

	ZoneScopedXC(RandomUniqueColor());

	return BindBuffers(context);
}

bool Behaviour::InitialOnHover()
{
	if (!IsEnabled())
		return true;

	return OnHover();
}
bool Behaviour::InitialOffHover()
{
	if (!IsEnabled())
		return true;

	return OffHover();
}
bool Behaviour::InitialOnSelect()
{
	if (!IsEnabled())
		return true;

	return OnSelect();
}
bool Behaviour::InitialOnDebugSelect()
{
	return OnDebugSelect();
}

bool Behaviour::InitialSerialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(_name.c_str(), _name.size());

	if (!_doSerialize)
		return true;

	json::Value nameStr(json::kStringType);
	nameStr.SetString(_name.c_str(), docAlloc);
	obj.AddMember("Name", nameStr, docAlloc);

	json::Value attributes(json::kObjectType);
	if (!Serialize(docAlloc, attributes))
		Warn("Failed to serialize behaviour!");

	obj.AddMember("Attributes", attributes, docAlloc);

	return true;
}
bool Behaviour::InitialDeserialize(const json::Value &obj, Scene *scene)
{
	ZoneScopedXC(RandomUniqueColor());

	if (!Deserialize(obj, scene))
	{
		Warn("Failed to deserialize behaviour!");
		return true;
	}

	scene->AddPostDeserializeCallback(this);

	return true;
}
void Behaviour::InitialPostDeserialize()
{
	PostDeserialize();
}


bool Behaviour::Start() { return true; }
bool Behaviour::Update(TimeUtils &time, const Input &input) { return true; }
bool Behaviour::ParallelUpdate(const TimeUtils &time, const Input &input) { return true; }
bool Behaviour::LateUpdate(TimeUtils &time, const Input &input) { return true; }
bool Behaviour::FixedUpdate(float deltaTime, const Input &input) { return true; }
bool Behaviour::BeforeRender()
{
	return true;
}
bool Behaviour::Render(const RenderQueuer &queuer, const RendererInfo &rendererInfo) { return true; }
#ifdef USE_IMGUI
bool Behaviour::RenderUI() { return true; }
#endif
bool Behaviour::BindBuffers(ID3D11DeviceContext *context) { return true; }

void Behaviour::OnEnable() { }
void Behaviour::OnDisable() { }
void Behaviour::OnDirty() { }
bool Behaviour::OnHover() { return true; }
bool Behaviour::OffHover() { return true; }
bool Behaviour::OnSelect() { return true; }
bool Behaviour::OnDebugSelect() { return true; }

bool Behaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) { return true; }
bool Behaviour::Deserialize(const json::Value &obj, Scene *scene) { return true; }
void Behaviour::PostDeserialize() { }
