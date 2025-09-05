#include "stdafx.h"
#include "Behaviours/TrackerBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#ifdef DEBUG_BUILD
#include "Behaviours/DebugPlayerBehaviour.h"
#endif

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool TrackerBehaviour::Start()
{
	if (_name.empty())
		_name = "TrackerBehaviour"; // For categorization in ImGui.

	QueueParallelUpdate();

	return true;
}

bool TrackerBehaviour::ParallelUpdate(const TimeUtils &time, const Input &input)
{
	Entity *trackedEntity;
	if (!_trackedEntity.TryGet(trackedEntity))
		return true;

	// Get the tracked entity's transform.
	Transform &thisTrans = *GetTransform();
	Transform &trackTrans = *trackedEntity->GetTransform();

	dx::XMFLOAT3 trackPos = trackTrans.GetPosition(World);
	dx::XMFLOAT3 thisPos = thisTrans.GetPosition(World);

	dx::XMVECTOR lookPosVec = Load(trackPos) + Load(_offset);
	dx::XMVECTOR upVec = Load(thisTrans.GetUp(World));

	dx::XMVECTOR lookDir = XMVectorSubtract(lookPosVec, Load(thisPos));

	if (XMVector3Equal(lookDir, dx::g_XMZero))
	{
		// If the look direction is zero, do not update the transform.
		return true;
	}

	// Normalize the look direction.
	lookDir = XMVector3Normalize(lookDir);

	// lookDir is the new forward direction, now calculate the up and right vectors.
	dx::XMVECTOR right = XMVector3Cross(lookDir, upVec);

	if (XMVector3Equal(right, dx::g_XMZero))
	{
		// If the right vector is zero, it means the up vector is aligned with the look direction.
		// In this case, we can use the default up vector (not foolproof, but good enough).
		right = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}

	upVec = XMVector3Cross(right, lookDir);

	dx::XMFLOAT3A newFwd, newUp;
	Store(newFwd, lookDir);
	Store(newUp, upVec);

	thisTrans.SetLookDir(newFwd, newUp, World);

	return true;
}

#ifdef USE_IMGUI
bool TrackerBehaviour::RenderUI()
{
	// Drag & Drop target for tracking entities.
	{
		ImGui::Text("Tracking:"); 
		ImGui::SameLine();

		std::string trackingName = "None";
		Entity *trackedEntity;
		if (_trackedEntity.TryGet(trackedEntity))
			trackingName = trackedEntity->GetName();

		if (ImGui::Button(trackingName.c_str()))
		{
			if (trackedEntity)
				GetScene()->GetDebugPlayer()->Select(trackedEntity);
		}

		ImGui::EntityDragDropTarget([this](ImGui::EntityPayload &entPayload) {
			Scene *scene = GetScene();
			SceneHolder *sceneHolder = scene->GetSceneHolder();

			if (entPayload.sceneUID != scene->GetUID())
			{
				DbgMsg("Dragging from another scene not supported.");
				return false;
			}
			
			if (entPayload.entID == GetEntity()->GetID())
			{
				DbgMsg("Tracker cannot track itself.");
				return false;
			}
			
			if (Entity *currEnt = _trackedEntity.Get()) // We can skip if same entity as currently tracked.
			{
				if (currEnt->GetID() == entPayload.entID)
					return false;
			}

			_trackedEntity = sceneHolder->GetEntityByID(entPayload.entID);
			return true;
		});

		// Add tooltip for the tracking button.
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("Drag & Drop field");
			ImGui::EndTooltip();
		}
	
		if (_trackedEntity.IsValid())
		{
			ImGui::SameLine();
			if (ImGui::Button("X"))
				_trackedEntity = nullptr;
		}
	}

	ImGui::Dummy(ImVec2(0, 3));

	ImGui::DragFloat3("Offset", reinterpret_cast<float*>(&_offset));

	return true;
}
#endif


Entity *TrackerBehaviour::GetTrackedEntity() const
{
	return _trackedEntity.Get();
}
const dx::XMFLOAT3 &TrackerBehaviour::GetOffset()
{
	return _offset;
}

void TrackerBehaviour::SetTrackedEntity(Entity *ent)
{
	_trackedEntity = ent;
}
void TrackerBehaviour::GetOffset(dx::XMFLOAT3 offset)
{
	_offset = offset;
}

bool TrackerBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	Entity *ent = _trackedEntity.Get();
	obj.AddMember("Track", ent ? ent->GetID() : CONTENT_NULL, docAlloc);
	obj.AddMember("Offset", SerializerUtils::SerializeVec(_offset, docAlloc), docAlloc);

	return true;
}
bool TrackerBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Track"))
		_tempTrackedEntityID = obj["Track"].GetUint();

	if (obj.HasMember("Offset"))
		SerializerUtils::DeserializeVec(_offset, obj["Offset"]);

	return true;
}
void TrackerBehaviour::PostDeserialize()
{
	if (_tempTrackedEntityID == CONTENT_NULL)
		return;

	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();
	_trackedEntity = sceneHolder->GetEntityByDeserializedID(_tempTrackedEntityID);
	_tempTrackedEntityID = CONTENT_NULL;
}
