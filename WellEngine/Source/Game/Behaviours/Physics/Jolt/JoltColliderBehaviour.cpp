#include "stdafx.h"
#include "JoltColliderBehaviour.h"
#include "Source/Game/Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


JoltColliderBehaviour::JoltColliderBehaviour(JPH::EMotionType type, JPH::ObjectLayer layer) :
	_type(type), _layer(layer)
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

bool JoltColliderBehaviour::Update(TimeUtils &time, const Input &input)
{

	return true;
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

		// Update body motion type in physics system
		JPH::BodyInterface &bodyInterface = GetBodyInterface();
		bodyInterface.SetMotionType(_bodyID, GetMotionType(), JPH::EActivation::DontActivate);
	}

	// TODO: Layer

	return true;
}
#endif

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