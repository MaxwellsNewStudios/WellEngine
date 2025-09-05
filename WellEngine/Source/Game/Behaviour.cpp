#include "stdafx.h"
#include "Behaviour.h"
#include "Behaviours/CameraBehaviour.h"
#include "Rendering/RenderQueuer.h"
#include "Scenes/Scene.h"
#include "Entity.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

Behaviour::~Behaviour()
{
	_isDestroyed = true;
	DequeueUpdate();
	DequeueParallelUpdate();
	DequeueLateUpdate();
	DequeueFixedUpdate();
}

bool Behaviour::Initialize(Entity *entity, const std::string &behaviourName)
{
	ZoneScopedXC(RandomUniqueColor());

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

#ifdef TRACY_DETAILED
	std::string zoneName = std::format("Behaviour Initialize '{}'", GetName());
	ZoneTextX(zoneName.c_str(), zoneName.size());
#endif

#ifdef DEBUG_BUILD
	if (_name.empty())
		Warn("Behaviour name is empty! Did you forget to assign a name in Start?");
#endif

	_isInitialized = true;
	return true;
}

bool Behaviour::IsInitialized() const
{
	return _isInitialized;
}
bool Behaviour::IsDestroyed() const
{
	return _isDestroyed;
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
bool Behaviour::InitialRenderUI()
{
	ImGuiUtils::BeginButtonStyle(ImGuiUtils::StyleType::Red);
	if (ImGui::Button("Delete", { 60, 20 }))
	{
		_entity->RemoveBehaviour(this);
		ImGuiUtils::EndButtonStyle();
		return true;
	}
	ImGuiUtils::EndButtonStyle();
	
	bool behEnabled = IsEnabledSelf();
	if (ImGui::Checkbox("Active##behActive", &behEnabled))
	{
		if (behEnabled)
			SetEnabled(true);
		else
			SetEnabled(false);
	}

	ImGui::Checkbox("Serialize##behSerialize", &_doSerialize);

	ImGui::Text("References: %d", GetRefs().size());

	ImGui::Separator();

	ImGui::Dummy({0, 6});
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
