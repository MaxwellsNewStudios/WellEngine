#include "stdafx.h"
#include "Behaviour.h"
#include "BehaviourRegistry.h"
#include "Rendering/Camera/B_Camera.h"
#include "Engine/Rendering/RenderQueuer.h"
#include "Game/Scene/Scene.h"
#include "Game/Entity.h"

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
		DequeuePhysicsUpdate();
	}
}

bool Behaviour::Initialize(Entity *entity)
{
	ZoneScopedC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

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

	if (!Start())
	{
		ErrMsg("Failed to initialize behaviour!");
		return false;
	}

	if (_isDestroyed)
		return true;

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
void Behaviour::QueuePhysicsUpdate()
{
	GetScene()->AddPhysicsUpdateCallback(this);
}
void Behaviour::DequeuePhysicsUpdate()
{
	GetScene()->RemovePhysicsUpdateCallback(this);
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
	ZoneTextX(GetName().data(), GetName().size());

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
	ZoneTextX(GetName().data(), GetName().size());

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
	ZoneTextX(GetName().data(), GetName().size());

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
	ZoneTextX(GetName().data(), GetName().size());

	return FixedUpdate(deltaTime, input);
}
bool Behaviour::InitialPhysicsUpdate(float deltaTime)
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
	ZoneTextX(GetName().data(), GetName().size());

	return PhysicsUpdate(deltaTime);
}
bool Behaviour::InitialBeforeRender()
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	return BeforeRender();
}
bool Behaviour::InitialRender(RenderQueuer &queuer, const RendererInfo &rendererInfo)
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

	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	return Render(queuer, rendererInfo);
}
#ifdef USE_IMGUI
void Behaviour::CopyToClipboard()
{
	std::string behJSON = "[[BEHAVIOUR_JSON]] ";
	{
		json::Document doc;
		json::Value behObj(json::kObjectType);

		if (!InitialSerialize(doc.GetAllocator(), behObj))
		{
			ErrMsg("Failed to serialize behaviour!");
			return;
		}

		json::StringBuffer buffer;
		json::Writer<json::StringBuffer> writer(buffer);
		behObj.Accept(writer);
		behJSON += buffer.GetString();
	}
	ImGui::SetClipboardText(behJSON.c_str());
}
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
	ImGui::Checkbox("Serialized##behSerialize", &_doSerialize);

	ImGui::SameLine();
	std::string refText = std::format("Refs: {}", GetRefs().size());
	float refTextWidth = ImGui::CalcTextSize(refText.c_str()).x + 4.0f;
	float availWidth = ImGui::GetContentRegionAvail().x;
	ImGui::NewLine();
	float refTextoffset = MAX(2.0f, availWidth - refTextWidth);

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

bool Behaviour::InitialOnDebugSelect()
{
	return OnDebugSelect();
}
bool Behaviour::InitialOnEditTransform()
{
	return OnEditTransform();
}
bool Behaviour::InitialOnEditTransformRec()
{
	return OnEditTransformRec();
}

bool Behaviour::InitialSerialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	ZoneScopedXC(RandomUniqueColor());
	ZoneTextX(GetName().data(), GetName().size());

	if (!_doSerialize)
		return true;

	json::Value nameStr(json::kStringType);
	nameStr.SetString(GetName().data(), docAlloc);
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
bool Behaviour::InitialPostDeserialize()
{
	return PostDeserialize();
}


bool Behaviour::Start() { return true; }
bool Behaviour::Update(TimeUtils &time, const Input &input) { return true; }
bool Behaviour::ParallelUpdate(const TimeUtils &time, const Input &input) { return true; }
bool Behaviour::LateUpdate(TimeUtils &time, const Input &input) { return true; }
bool Behaviour::FixedUpdate(float deltaTime, const Input &input) { return true; }
bool Behaviour::PhysicsUpdate(float deltaTime) { return true; }
bool Behaviour::BeforeRender()
{
	return true;
}
bool Behaviour::Render(RenderQueuer &queuer, const RendererInfo &rendererInfo) { return true; }
#ifdef USE_IMGUI
bool Behaviour::RenderUI() { return true; }
#endif
bool Behaviour::BindBuffers(ID3D11DeviceContext *context) { return true; }

void Behaviour::OnEnable() { }
void Behaviour::OnDisable() { }
void Behaviour::OnDirty() { }
bool Behaviour::OnEditTransform() { return true; }
bool Behaviour::OnEditTransformRec() { return true; }
bool Behaviour::OnDebugSelect() { return true; }

bool Behaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) { return true; }
bool Behaviour::Deserialize(const json::Value &obj, Scene *scene) { return true; }
bool Behaviour::PostDeserialize() { return true; }

