#include "stdafx.h"
#include "Behaviours/RestrictedViewBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool RestrictedViewBehaviour::Start()
{
	if (_name.empty())
		_name = "RestrictedViewBehaviour"; // For categorization in ImGui.

	QueueUpdate();

	return true;
}

bool RestrictedViewBehaviour::Update(TimeUtils &time, const Input &input)
{
	if (!input.IsCursorLocked())
		return true;

	Transform *t = GetTransform();
	const MouseState mState = input.GetMouse();

	float moveAmplitude = powf(mState.delta.x * mState.delta.x + mState.delta.y * mState.delta.y, 0.2f);
	float sensitivity = 0.5f * _prevOffset / (360.0f * moveAmplitude);

	if (mState.delta.x != 0.0f)
	{
		t->RotateAxis(_upDirection, sensitivity * mState.delta.x, World);
	}

	if (mState.delta.y != 0.0f)
	{
		XMFLOAT3A right = t->GetRight(World);
		t->RotateAxis(right, sensitivity * mState.delta.y, World);
	}
	
	XMVECTOR restrictedFwdDir = Load(_viewDirection);
	XMVECTOR newFwdDir = Load(t->GetForward(World));

	// Check if new view direction is within allowed offset
	float dot;
	Store(dot, XMVector3Dot(newFwdDir, restrictedFwdDir));
	if (dot < _allowedDotOffset)
	{
		XMFLOAT3A cross;
		Store(cross, -XMVector3Cross(newFwdDir, restrictedFwdDir));

		// Rotate the transform back to the allowed offset
		t->SetEuler(_startOrientation, World);
		t->RotateAxis(cross, _offsetInDeg * DEG_TO_RAD, World);
	}

	// Offset normalized so that no offset is 1, and max offset is 0.
	_prevOffset = std::clamp((dot - _allowedDotOffset) / (1.0f - _allowedDotOffset), 0.02f, 1.0f);
	return true;
}

void RestrictedViewBehaviour::SetViewDirection(const XMFLOAT3 &orientation, const XMFLOAT3 &viewDirection, const dx::XMFLOAT3 &upDirection)
{
	_startOrientation = To3(orientation);

	Store(
		_viewDirection, 
		XMVector3Normalize(Load(viewDirection))
	);

	Store(
		_upDirection, 
		XMVector3Normalize(Load(upDirection))
	);
}
void RestrictedViewBehaviour::SetAllowedOffset(float offsetDegrees)
{
	_offsetInDeg = offsetDegrees;

	// Calculate offset in as dot product result
	_allowedDotOffset = cosf(offsetDegrees * DEG_TO_RAD);
}


bool RestrictedViewBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}
bool RestrictedViewBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

