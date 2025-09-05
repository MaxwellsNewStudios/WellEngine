#include "stdafx.h"
#include "Behaviours/MenuCameraBehaviour.h"
#include "Scenes/Scene.h"
#include "Scenes/SceneHolder.h"
#include "Behaviours/CameraBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool MenuCameraBehaviour::RayCastFromMouse(RaycastOut &out, XMFLOAT3A &pos, const Input &input)
{
	const dx::XMFLOAT2 mPos = input.GetLocalMousePos();

	// Wiewport to NDC coordinates
	float xNDC = (2.0f * mPos.x) - 1.0f;
	float yNDC = 1.0f - (2.0f * mPos.y); // can also be - 1 depending on coord-system
	float zNDC = 1.0f;	// not really needed yet (specified anyways)
	XMFLOAT3A ray_clip = XMFLOAT3A(xNDC, yNDC, zNDC);

	// Wiew space -> clip space
	XMVECTOR rayClipVec = Load(ray_clip);
	XMMATRIX projMatrix = Load(_mainCamera->GetProjectionMatrix());
	XMVECTOR rayEyeVec = XMVector4Transform(rayClipVec, XMMatrixInverse(nullptr, projMatrix));

	// Set z and w to mean forwards and not a point
	rayEyeVec = XMVectorSet(XMVectorGetX(rayEyeVec), XMVectorGetY(rayEyeVec), 1, 0.0);

	// Clip space -> world space
	XMMATRIX viewMatrix = Load(_mainCamera->GetViewMatrix());
	XMVECTOR rayWorldVec = XMVector4Transform(rayEyeVec, XMMatrixInverse(nullptr, viewMatrix));

	rayWorldVec = XMVector4Normalize(rayWorldVec);	// Normalize
	XMFLOAT3A dir; Store(dir, rayWorldVec);

	// Camera 
	XMFLOAT3A camPos = _mainCamera->GetTransform()->GetPosition(World);

	// Perform raycast
	if (!GetScene()->GetSceneHolder()->RaycastScene(
		{ camPos.x, camPos.y, camPos.z },
		{ dir.x, dir.y, dir.z },
		out))
	{
		return false;
	}

	pos = {
		camPos.x + (dir.x * out.distance),
		camPos.y + (dir.y * out.distance),
		camPos.z + (dir.z * out.distance)
	};

	return true;
}

bool MenuCameraBehaviour::Start()
{
	if (_name.empty())
		_name = "MenuCameraBehaviour"; // For categorization in ImGui.

	Scene *scene = GetScene();

	// Create main camera
	{
		ProjectionInfo projInfo = ProjectionInfo(65.0f * DEG_TO_RAD, 16.0f / 9.0f, { 0.2f, 125.0f });
		CameraBehaviour *camera = new CameraBehaviour(projInfo);

		if (!camera->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind MainCamera behaviour!");
			return false;
		}

		camera->SetSerialization(false);

		scene->SetViewCamera(camera);
		_mainCamera = camera;
	}

	// Spot light
	{
		if (!GetScene()->CreateSimpleSpotLightEntity(&_spotLight, "Spotlight Flashlight", { 30.0f, 29.0f, 26.5f }, 1.0f, 45.0f, false, 0.3f))
		{
			ErrMsg("Failed to create spotlight entity to flashlight!");
			return false;
		}
		_spotLight->SetParent(GetEntity());
		_spotLight->SetSerialization(false);
	}

	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();
	_buttons[0] = sceneHolder->GetEntityByName("StartButton");
	_buttons[1] = sceneHolder->GetEntityByName("SaveButton");
	_buttons[2] = sceneHolder->GetEntityByName("NewSaveButton");
	_buttons[3] = sceneHolder->GetEntityByName("CreditsButton");
	_buttons[4] = sceneHolder->GetEntityByName("ExitButton");

	QueueUpdate();

	return true;
}

bool MenuCameraBehaviour::Update(TimeUtils &time, const Input &input)
{
	RaycastOut out;
	XMFLOAT3A cursorScenePos = {};
	bool hasHit = RayCastFromMouse(out, cursorScenePos, input);
	if (input.GetKey(KeyCode::M1) == KeyState::Pressed)
	{
		if (hasHit)
		{
			if (!out.entity->InitialOnSelect())
			{
				ErrMsg("OnSelect Failed!");
				return false;
			}
		}
	}

	for (int i = 0; i < 5; i++)
	{
		_buttons[i]->GetTransform()->SetScale({ 0.7f, 0.2f, 0.2f });
	}

	if (out.entity && !out.entity->InitialOnHover())
	{
		ErrMsg("OnHover Failed!");
		return false;
	}

	//XMFLOAT4A rotate = { cursorScenePos.x / 2.3f, cursorScenePos.y / 2.3f, cursorScenePos.z, 1.0f };
	//_spotLight->GetTransform()->SetRotation(rotate);

	Transform *spotlightTransform = _spotLight->GetTransform();

	XMVECTOR lightFwd = Load(spotlightTransform->GetForward(World));

	XMVECTOR toCursor = XMVectorSubtract(
		Load(cursorScenePos),
		Load(spotlightTransform->GetPosition(World))
	);

	// skip if length of toCursor is too small
	if (XMVector3Less(XMVector3LengthSq(toCursor), XMVectorReplicate(0.1f)))
		return true;



	XMFLOAT3A fwdLerp;
	Store(fwdLerp, XMVectorLerp(lightFwd, XMVector3Normalize(toCursor), 0.1f));

	XMFLOAT3A up = {0, 1, 0};
	_spotLight->GetTransform()->SetLookDir(fwdLerp, up, World);

	return true;
}

bool MenuCameraBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool MenuCameraBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}
