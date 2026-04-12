#include "stdafx.h"
#include "JoltColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltColliderBehaviour::JoltColliderBehaviour()
{ }

JoltColliderBehaviour::JoltColliderBehaviour(JPH::EMotionType motionType, JPH::ObjectLayer layer, float friction, float gravityFactor, float restitution) :
	_motionType(motionType), _layer(layer), _friction(friction), _gravityFactor(gravityFactor), _restitution(restitution)
{ }

JoltColliderBehaviour::~JoltColliderBehaviour()
{
	// If quitting, just return
	if (GetScene()->IsDestroyed())
		return;

	// if body is valid, remove it from the physics system
	if (!_bodyID.IsInvalid())
		DestroyBody();
}

bool JoltColliderBehaviour::Start()
{
	// Apply default properties
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetFriction(_bodyID, _friction);
	bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	bodyInterface.SetRestitution(_bodyID, _restitution);

	QueueUpdate();
	QueueLateUpdate();

	return true;
}


bool JoltColliderBehaviour::Update(TimeUtils &time, const Input &input)
{
	return true;
}

bool JoltColliderBehaviour::LateUpdate(TimeUtils &time, const Input &input)
{
	// If entity transform has moved or rotated since the last frame, apply the new transform to the physics body.
	// Otherwise, apply the current physics body transform to the entity to keep them in sync.

	Transform *transform = GetEntity()->GetTransform();

	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);
	dx::XMFLOAT3A currScale = transform->GetScale();

	if (currScale.x != _lastEntScale.x || currScale.y != _lastEntScale.y || currScale.z != _lastEntScale.z)
	{
		// Entity scale has changed, recalculate physics body.
		RecalculatePhysicsBody();

		_lastEntScale = currScale;
	}

	if (currPos.x != _lastEntPos.x || currPos.y != _lastEntPos.y || currPos.z != _lastEntPos.z ||
		currRot.x != _lastEntRot.x || currRot.y != _lastEntRot.y || currRot.z != _lastEntRot.z || currRot.w != _lastEntRot.w)
	{
		// Entity transform has changed, apply it to the physics body.
		SyncPhysics();

		_lastEntPos = currPos;
		_lastEntRot = currRot;
	}
	else
	{
		// Entity transform has not changed, apply physics body transform to entity.
		// This can be skipped if the body is not dynamic or is inactive.

		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bool doSkip = false;

		if (bodyInterface.GetMotionType(_bodyID) != JPH::EMotionType::Dynamic)
			doSkip = true;
		else if (!bodyInterface.IsActive(_bodyID))
			doSkip = true;

		if (!doSkip)
		{
			SyncTransform();

			currPos = transform->GetPosition(World);
			currRot = transform->GetRotation(World);

			_lastEntPos = currPos;
			_lastEntRot = currRot;
		}
	}

	return true;
}


bool JoltColliderBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("MotionType", (UINT)_motionType, docAlloc);
	obj.AddMember("Layer", (UINT)_layer, docAlloc);
	obj.AddMember("Friction", _friction, docAlloc);
	obj.AddMember("GravityFactor", _gravityFactor, docAlloc);
	obj.AddMember("Restitution", _restitution, docAlloc);
#ifdef USE_IMGUI
	obj.AddMember("DebugDraw", _debugDraw, docAlloc);
#endif
	return true;
}
bool JoltColliderBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("MotionType"))
		_motionType = (JPH::EMotionType)obj["MotionType"].GetUint();

	if (obj.HasMember("Layer"))
		_layer = (JPH::ObjectLayer)obj["Layer"].GetUint();

	if (obj.HasMember("Friction"))
		_friction = obj["Friction"].GetFloat();

	if (obj.HasMember("GravityFactor"))
		_gravityFactor = obj["GravityFactor"].GetFloat();

	if (obj.HasMember("Restitution"))
		_restitution = obj["Restitution"].GetFloat();

#ifdef USE_IMGUI
	if (obj.HasMember("DebugDraw"))
		_debugDraw = obj["DebugDraw"].GetBool();
#endif

	return true;
}
void JoltColliderBehaviour::PostDeserialize()
{
	JPH::BodyInterface &bodyInterface = GetBodyInterface();

	SetMotionType(_motionType);
	SetLayer(_layer);

	bodyInterface.SetFriction(_bodyID, _friction);
	bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	bodyInterface.SetRestitution(_bodyID, _restitution);

	RecalculatePhysicsBody();
	SyncPhysics();
}


[[nodiscard]] JPH::BodyInterface &JoltColliderBehaviour::GetBodyInterface() 
{ 
	return GetScene()->GetPhysicsInstance()->GetBodyInterface(); 
}

void JoltColliderBehaviour::DestroyBody()
{
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.RemoveBody(_bodyID);
	bodyInterface.DestroyBody(_bodyID);
	_bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
}

void JoltColliderBehaviour::SetMotionType(JPH::EMotionType motionType)
{
	_motionType = motionType;

	// Update body motion type in physics system
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetMotionType(_bodyID, GetMotionType(), JPH::EActivation::Activate);
}

void JoltColliderBehaviour::SetLayer(JPH::ObjectLayer layer)
{
	_layer = layer;

	// Update body layer in physics system
	JPH::BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetObjectLayer(_bodyID, GetLayer());
}


#ifdef USE_IMGUI
bool JoltColliderBehaviour::RenderUI()
{
	ImGui::Checkbox("Debug Draw", &_debugDraw);

	// Motion type
	const char *motionTypes[] = { "Static", "Kinematic", "Dynamic" };
	int motionTypeIndex = (int)GetMotionType();
	if (ImGui::Combo("Motion Type", &motionTypeIndex, motionTypes, IM_ARRAYSIZE(motionTypes)))
	{
		SetMotionType((JPH::EMotionType)motionTypeIndex);
	}

	// Layer
	const char *layers[] = { "Non Moving", "Moving" };
	int layerIndex = (int)GetLayer();
	if (ImGui::Combo("Layer", &layerIndex, layers, IM_ARRAYSIZE(layers)))
	{
		SetLayer((JPH::ObjectLayer)layerIndex);
	}

	// Friction
	if (ImGui::SliderFloat("Friction", &_friction, 0.0f, 1.0f))
	{
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetFriction(_bodyID, _friction);
	}

	// Gravity factor
	if (ImGui::DragFloat("Gravity Factor", &_gravityFactor, 0.01f))
	{
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	}
	ImGuiUtils::LockMouseOnActive();

	// Restitution
	if (ImGui::DragFloat("Restitution", &_restitution, 0.01f))
	{
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetRestitution(_bodyID, _restitution);
	}

	return true;
}
#endif
