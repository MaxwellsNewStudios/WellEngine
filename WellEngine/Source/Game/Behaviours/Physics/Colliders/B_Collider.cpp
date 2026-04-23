#include "stdafx.h"
#include "B_Collider.h"
#include "Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace JPH;


B_Collider::B_Collider()
{ }
B_Collider::B_Collider(EMotionType motionType, ObjectLayer layer, float friction, float gravityFactor, float restitution) :
	_motionType(motionType), _layer(layer), _friction(friction), _gravityFactor(gravityFactor), _restitution(restitution)
{ }

B_Collider::~B_Collider()
{
	// If quitting, just return
	if (GetScene()->IsDestroyed())
		return;

	// if body is valid, remove it from the physics system
	if (!_bodyID.IsInvalid())
		DestroyBody();
}

bool B_Collider::Start()
{
	// Apply default properties
	Transform *transform = GetTransform();
	_lastPosLerpGoal = _posLerpGoal = transform->GetPosition(World);
	_lastRotLerpGoal = _rotLerpGoal = transform->GetRotation(World);
	_lerpTime = 0.0f;

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetFriction(_bodyID, _friction);
	bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	bodyInterface.SetRestitution(_bodyID, _restitution);
	bodyInterface.SetIsSensor(_bodyID, IsSensor());

	QueueUpdate();
	QueueLateUpdate();
	QueuePhysicsUpdate();

	return true;
}

bool B_Collider::Update(TimeUtils &time, const Input &input)
{
#ifdef USE_IMGUI
	if (_debugDrawInterpolation)
	{
		// Draw green line from last lerp goal pos to curr pos
		// Draw yellow line from curr pos to goal pos
		// Draw red line from curr pos to actual physics body pos

		const BodyInterface &bodyInterface = GetBodyInterface();
		DebugDrawer &drawer = DebugDrawer::Instance();

		dx::XMFLOAT3A currPos;
		dx::XMFLOAT4A _;
		CalcLerp(currPos, _);

		dx::XMFLOAT3A bodyPos;
		CalcBodyLocation(bodyPos, _);


		drawer.DrawLine(_lastPosLerpGoal, currPos, 0.1f, {0.0f, 1.0f, 0.0f, 0.5f}, false);
		drawer.DrawLine(currPos, _posLerpGoal, 0.1f, { 1.0f, 1.0f, 0.0f, 0.5f }, false);
		drawer.DrawLine(currPos, bodyPos, 0.05f, { 1.0f, 0.0f, 0.0f, 0.5f }, false);
	}
#endif

	return true;
}
bool B_Collider::LateUpdate(TimeUtils &time, const Input &input)
{
	// Resample interpolation goal immediately after physics update.
	if (_resampleGoal)
	{
		Transform *transform = GetTransform();
		transform->SetPosition(_posLerpGoal, World);
		transform->SetRotation(_rotLerpGoal, World);

		_lastPosLerpGoal = _posLerpGoal;
		_lastRotLerpGoal = _rotLerpGoal;

		CalcBodyLocation(_posLerpGoal, _rotLerpGoal);
		_lerpTime = time.GetPhysDeltaTime();

		_resampleGoal = false;
	}

	BodyInterface &bodyInterface = GetBodyInterface();
	bool skipInterpolation = false;

	if (bodyInterface.GetMotionType(_bodyID) == EMotionType::Static)
		skipInterpolation = true;
	else if (!bodyInterface.IsActive(_bodyID))
		skipInterpolation = true;

	if (!skipInterpolation)
	{
		// Interpolate towards the current lerp goal to ensure smooth movement even if physics updates are infrequent.

		dx::XMFLOAT3A newPos;
		dx::XMFLOAT4A newRot;
		CalcLerp(newPos, newRot);

		Transform *transform = GetTransform();
		transform->SetPosition(newPos, World);
		transform->SetRotation(newRot, World);

		_lerpTime = MAX(_lerpTime - time.GetDeltaTime(), 0.0f);
	}

	return true;
}
bool B_Collider::PhysicsUpdate(float deltaTime)
{
	BodyInterface &bodyInterface = GetBodyInterface();

	if (bodyInterface.GetMotionType(_bodyID) == EMotionType::Static)
		return true;
	else if (!bodyInterface.IsActive(_bodyID))
		return true;

	_resampleGoal = true;

	return true;
}

void B_Collider::OnEnable()
{
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddBody(_bodyID, EActivation::Activate);
}
void B_Collider::OnDisable()
{
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.RemoveBody(_bodyID);
}

bool B_Collider::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
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
bool B_Collider::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("MotionType"))
		_motionType = (EMotionType)obj["MotionType"].GetUint();

	if (obj.HasMember("Layer"))
		_layer = (ObjectLayer)obj["Layer"].GetUint();

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
void B_Collider::PostDeserialize()
{
	BodyInterface &bodyInterface = GetBodyInterface();

	SetMotionType(_motionType);
	SetLayer(_layer);

	bodyInterface.SetFriction(_bodyID, _friction);
	bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	bodyInterface.SetRestitution(_bodyID, _restitution);

	RecalculatePhysicsBody();
	SyncPhysics();
}

void B_Collider::DestroyBody()
{
	ZoneScopedC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.RemoveBody(_bodyID);
	bodyInterface.DestroyBody(_bodyID);
	_bodyID = BodyID(BodyID::cInvalidBodyID);
}

void B_Collider::CalcLerp(dx::XMFLOAT3A &pos, dx::XMFLOAT4A &rot) const
{
	float t = _lerpTime / TimeUtils::GetPhysDeltaTime();
	Store(pos, dx::XMVectorLerp(Load(_posLerpGoal), Load(_lastPosLerpGoal), t));
	Store(rot, dx::XMQuaternionSlerp(Load(_rotLerpGoal), Load(_lastRotLerpGoal), t));
}

void B_Collider::OnEditTransformRec()
{
	Transform *transform = GetTransform();
	dx::XMFLOAT3A currPos = transform->GetPosition(World);
	dx::XMFLOAT4A currRot = transform->GetRotation(World);
	dx::XMFLOAT3A currScale = transform->GetScale();

	constexpr float eps = 1e-3f;
	if (!EstEqual(currScale.x, _lastEntScale.x, eps) || !EstEqual(currScale.y, _lastEntScale.y, eps) || !EstEqual(currScale.z, _lastEntScale.z, eps))
	{
		// Entity scale has changed, recalculate physics body.
		RecalculatePhysicsBody();
		_lastEntScale = currScale;
	}

	// Entity transform has changed, apply it to the physics body.
	SyncPhysics();

	_lastPosLerpGoal = _posLerpGoal = currPos;
	_lastRotLerpGoal = _rotLerpGoal = currRot;
}

const BodyInterface &B_Collider::GetBodyInterface() const
{ 
	return GetScene()->GetPhysicsInstance()->GetBodyInterface(); 
}
BodyInterface &B_Collider::GetBodyInterface() 
{ 
	return GetScene()->GetPhysicsInstance()->GetBodyInterface(); 
}
float B_Collider::GetMass() const
{
	const BodyInterface &bodyInterface = GetBodyInterface();
	MassProperties massProps = bodyInterface.GetShape(_bodyID)->GetMassProperties();
	return massProps.mMass;
}
JPH::EBodyType B_Collider::GetBodyType() const
{
	const BodyInterface &bodyInterface = GetBodyInterface();
	return bodyInterface.GetBodyType(_bodyID);
}
dx::XMFLOAT3 B_Collider::GetCenterOfMass() const
{
	const BodyInterface &bodyInterface = GetBodyInterface();
	Vec3 jCOM = bodyInterface.GetCenterOfMassPosition(_bodyID);

	dx::XMFLOAT3 dxCOM;
	std::memcpy(&dxCOM, jCOM.mF32, sizeof(dx::XMFLOAT3));

	return dxCOM;
}
dx::XMFLOAT3 B_Collider::GetLinearVelocity() const
{
	ZoneScopedXC(RandomUniqueColor());

	const BodyInterface &bodyInterface = GetBodyInterface();
	Vec3 jLinVel = bodyInterface.GetLinearVelocity(_bodyID);

	dx::XMFLOAT3 dxLinVel;
	std::memcpy(&dxLinVel, jLinVel.mF32, sizeof(dx::XMFLOAT3));

	return dxLinVel;
}
dx::XMFLOAT3 B_Collider::GetAngularVelocity() const
{
	ZoneScopedXC(RandomUniqueColor());

	const BodyInterface &bodyInterface = GetBodyInterface();
	Vec3 jAngVel = bodyInterface.GetAngularVelocity(_bodyID);

	dx::XMFLOAT3 dxAngVel;
	std::memcpy(&dxAngVel, jAngVel.mF32, sizeof(dx::XMFLOAT3));

	return dxAngVel;
}
dx::XMFLOAT3 B_Collider::GetPointVelocity(const dx::XMFLOAT3 &point) const
{
	ZoneScopedXC(RandomUniqueColor());

	Vec3 jPoint(point.x, point.y, point.z);

	const BodyInterface &bodyInterface = GetBodyInterface();
	Vec3 jPointVel = bodyInterface.GetPointVelocity(_bodyID, jPoint);

	dx::XMFLOAT3 dxPointVel;
	std::memcpy(&dxPointVel, jPointVel.mF32, sizeof(dx::XMFLOAT3));

	return dxPointVel;
}
bool B_Collider::IsBodyActive() const
{
	const BodyInterface &bodyInterface = GetBodyInterface();
	return bodyInterface.IsActive(_bodyID);
}
bool B_Collider::IsSensor() const
{
	return GetLayer() == Layers::SENSOR;
}

void B_Collider::SetBodyActive(bool active)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	if (active)
		bodyInterface.ActivateBody(_bodyID);
	else
		bodyInterface.DeactivateBody(_bodyID);
}
void B_Collider::SetMotionType(EMotionType motionType)
{
	_motionType = motionType;

	// Update body motion type in physics system
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetMotionType(_bodyID, GetMotionType(), EActivation::Activate);
}
void B_Collider::SetLayer(ObjectLayer layer)
{
	_layer = layer;

	// Update body layer in physics system
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetObjectLayer(_bodyID, GetLayer());
	bodyInterface.SetIsSensor(_bodyID, IsSensor());
}
void B_Collider::SetFriction(float friction)
{
	_friction = friction;

	// Update body friction in physics system
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetFriction(_bodyID, GetFriction());
}
void B_Collider::SetGravityFactor(float gravityFactor)
{
	_gravityFactor = gravityFactor;

	// Update body gravity factor in physics system
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetGravityFactor(_bodyID, GetGravityFactor());
}
void B_Collider::SetRestitution(float restitution)
{
	_restitution = restitution;

	// Update body restitution in physics system
	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetRestitution(_bodyID, GetRestitution());
}
void B_Collider::SetPosition(const dx::XMFLOAT3 &position)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetPosition(_bodyID, Vec3(position.x, position.y, position.z), EActivation::Activate);
}
void B_Collider::SetRotation(const dx::XMFLOAT4 &rotation)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetRotation(_bodyID, Quat(rotation.x, rotation.y, rotation.z, rotation.w), EActivation::Activate);
}
void B_Collider::SetLinearVelocity(const dx::XMFLOAT3 &velocity)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetLinearVelocity(_bodyID, Vec3(velocity.x, velocity.y, velocity.z));
}
void B_Collider::SetAngularVelocity(const dx::XMFLOAT3 &velocity)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetAngularVelocity(_bodyID, Vec3(velocity.x, velocity.y, velocity.z));
}
void B_Collider::SetPosRotVel(const dx::XMFLOAT3 &position, const dx::XMFLOAT4 &rotation, const dx::XMFLOAT3 &linVel, const dx::XMFLOAT3 &angVel)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.SetPositionRotationAndVelocity(_bodyID, 
		Vec3(position.x, position.y, position.z), 
		Quat(rotation.x, rotation.y, rotation.z, rotation.w), 
		Vec3(linVel.x, linVel.y, linVel.z), 
		Vec3(angVel.x, angVel.y, angVel.z)
	);
}

void B_Collider::AddLinearVelocity(const dx::XMFLOAT3 &velocity)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddLinearVelocity(_bodyID, Vec3(velocity.x, velocity.y, velocity.z));
}
void B_Collider::AddAngularVelocity(const dx::XMFLOAT3 &velocity)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	// Jolt doesn't have a method to just add angular velocity.
	bodyInterface.AddLinearAndAngularVelocity(_bodyID, Vec3(0, 0, 0), Vec3(velocity.x, velocity.y, velocity.z));
}
void B_Collider::AddForce(const dx::XMFLOAT3 &force)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddForce(_bodyID, Vec3(force.x, force.y, force.z));
}
void B_Collider::AddForce(const dx::XMFLOAT3 &force, const dx::XMFLOAT3 &point)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddForce(_bodyID, Vec3(force.x, force.y, force.z), Vec3(point.x, point.y, point.z));
}
void B_Collider::AddTorque(const dx::XMFLOAT3 &torque)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddTorque(_bodyID, Vec3(torque.x, torque.y, torque.z));
}
void B_Collider::AddImpulse(const dx::XMFLOAT3 &impulse)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddImpulse(_bodyID, Vec3(impulse.x, impulse.y, impulse.z));
}
void B_Collider::AddImpulse(const dx::XMFLOAT3 &impulse, const dx::XMFLOAT3 &point)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddImpulse(_bodyID, Vec3(impulse.x, impulse.y, impulse.z), Vec3(point.x, point.y, point.z));
}
void B_Collider::AddAngularImpulse(const dx::XMFLOAT3 &impulse)
{
	ZoneScopedXC(RandomUniqueColor());

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.AddAngularImpulse(_bodyID, Vec3(impulse.x, impulse.y, impulse.z));
}
void B_Collider::MoveKinematic(const dx::XMFLOAT3 &position, const dx::XMFLOAT4 &rotation, float time)
{
	ZoneScopedXC(RandomUniqueColor());

	Vec3 jPos(position.x, position.y, position.z);
	Quat jRot(rotation.x, rotation.y, rotation.z, rotation.w);

	BodyInterface &bodyInterface = GetBodyInterface();
	bodyInterface.MoveKinematic(_bodyID, jPos, jRot, time);
}

#ifdef USE_IMGUI
bool B_Collider::RenderUI()
{
	// Info

	if (ImGui::TreeNode("Info"))
	{
		dx::XMFLOAT3 com = GetCenterOfMass();
		dx::XMFLOAT3 linVel = GetLinearVelocity();
		dx::XMFLOAT3 angVel = GetAngularVelocity();

		float startPos = ImGui::GetCursorPosX();
		float padding = 16.0f;

		static float labelWidth = 120.0f;
		float prevLabelWidth = startPos + labelWidth;
		labelWidth = 0.0f;

		ImGui::Text("Body ID");
		labelWidth = MAX(labelWidth, ImGui::GetItemRectSize().x);
		ImGui::SameLine(prevLabelWidth, padding);
		ImGui::Text("%d", GetBodyID().GetIndex());

		ImGui::Text("Mass (kg)");
		labelWidth = MAX(labelWidth, ImGui::GetItemRectSize().x);
		ImGui::SameLine(prevLabelWidth, padding);
		ImGui::Text("%.4f", GetMass());

		ImGui::Text("Lin Vel");
		labelWidth = MAX(labelWidth, ImGui::GetItemRectSize().x);
		ImGui::SameLine(prevLabelWidth, padding);
		ImGui::Text("(%.3f, %.3f, %.3f)", linVel.x, linVel.y, linVel.z);

		ImGui::Text("Ang Vel");
		labelWidth = MAX(labelWidth, ImGui::GetItemRectSize().x);
		ImGui::SameLine(prevLabelWidth, padding);
		ImGui::Text("(%.3f, %.3f, %.3f)", angVel.x, angVel.y, angVel.z);

		ImGui::Separator();
		ImGui::TreePop();
	}

	ImGui::Checkbox("Debug Draw", &_debugDraw);
	ImGui::Checkbox("Draw Interpolation", &_debugDrawInterpolation);

	// Motion type
	const char *motionTypes[] = { "Static", "Kinematic", "Dynamic" };
	int motionTypeIndex = (int)GetMotionType();
	if (ImGui::Combo("Motion Type", &motionTypeIndex, motionTypes, IM_ARRAYSIZE(motionTypes)))
	{
		SetMotionType((EMotionType)motionTypeIndex);
	}

	// Layer
	const char *layers[] = { "Non-Moving", "Moving", "Sensor" };
	int layerIndex = (int)GetLayer();
	if (ImGui::Combo("Layer", &layerIndex, layers, IM_ARRAYSIZE(layers)))
	{
		SetLayer((ObjectLayer)layerIndex);
	}

	// Friction
	if (ImGui::DragFloat("Friction", &_friction, 0.01f))
	{
		SetFriction(_friction);
	}
	ImGuiUtils::LockMouseOnActive();

	// Gravity factor
	if (ImGui::DragFloat("Gravity Factor", &_gravityFactor, 0.01f))
	{
		BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetGravityFactor(_bodyID, _gravityFactor);
	}
	ImGuiUtils::LockMouseOnActive();

	// Restitution
	if (ImGui::DragFloat("Restitution", &_restitution, 0.01f))
	{
		BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetRestitution(_bodyID, _restitution);
	}
	ImGuiUtils::LockMouseOnActive();

	return true;
}
#endif
