#include "stdafx.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Behaviours/DebugPlayerBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

BillboardMeshBehaviour::BillboardMeshBehaviour(const Material &material, 
	float rotation, float normalOffset, float size,
	bool keepUpright, bool isTransparent, bool castShadows, bool isOverlay, bool isGizmo) : _material(material)
{
	_transparent = isTransparent;
	_castShadows = castShadows;
	_overlay = isOverlay;
	_rotation = rotation;
	_normalOffset = normalOffset;
	_scale = size;
	_keepUpright = keepUpright;
	_gizmo = isGizmo;
}
BillboardMeshBehaviour::~BillboardMeshBehaviour()
{
#ifdef DEBUG_BUILD
	if (_gizmo)
	{
		auto scene = GetScene();

		if (scene->IsDestroyed())
			return;

		DebugPlayerBehaviour *debugPlayer = scene->GetDebugPlayer();
		if (debugPlayer->IsDestroyed())
			return;

		if (debugPlayer)
			debugPlayer->RemoveGizmoBillboard(this);
	}
#endif
}

bool BillboardMeshBehaviour::Start()
{
	if (_name.empty())
		_name = "BillboardMeshBehaviour"; // For categorization in ImGui.

	// Create a mesh behaviour as a child entity
	static UINT meshID = GetScene()->GetContent()->GetMeshID("Plane");

	Entity *entity;
	if (!GetScene()->CreateMeshEntity(&entity, "Billboard Mesh", meshID, _material, _transparent, _castShadows, _overlay))
	{
		Warn("Failed to create mesh entity!");
		return true;
	}
	entity->SetParent(GetEntity());

	entity->GetBehaviourByType<MeshBehaviour>(_meshBehaviour);
	entity->SetSerialization(false);

#ifdef DEBUG_BUILD
	if (_gizmo)
	{
		SetSerialization(false);
		DebugPlayerBehaviour *debugPlayer = GetScene()->GetDebugPlayer();
		if (debugPlayer)
			debugPlayer->AddGizmoBillboard(this);
	}
#endif

	QueueParallelUpdate();
	return true;
}

bool BillboardMeshBehaviour::ParallelUpdate(const TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();

	CameraBehaviour *viewCamera = scene->GetViewCamera();
	if (!viewCamera)
		return true;

	// Make the mesh face the camera
	Transform *camTrans = viewCamera->GetTransform();

	XMFLOAT3A billboardPos = GetTransform()->GetPosition(World);
	XMFLOAT3A camPos = camTrans->GetPosition(World);

	XMVECTOR billboardPosVec = Load(billboardPos);
	XMVECTOR camPosVec = Load(camPos);

	XMVECTOR toCam = camPosVec - billboardPosVec;
	XMVECTOR lookDir = XMVector3Normalize(toCam);

	// If the camera is within the normal offset distance or the cameras near plane, disable the billboard mesh & return
	if (XMVectorGetX(XMVector3Length(toCam)) <= max(_normalOffset, viewCamera->GetPlanes().nearZ))
	{
		_meshBehaviour->GetEntity()->Disable();
		return true;
	}
	else
	{
		_meshBehaviour->GetEntity()->Enable();
	}

	XMVECTOR up;
	if (_keepUpright)
	{
		up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		up = XMVector3Normalize(XMVector3Cross(lookDir, Load(camTrans->GetRight(World))));
	}

	if (_rotation != 0.0f)
	{
		XMMATRIX rotMat = XMMatrixRotationAxis(lookDir, _rotation);
		up = XMVector3Transform(up, rotMat);
	}

	XMVECTOR right;
	if (XMVectorGetX(XMVector3Dot(up, lookDir)) >= 0.999999f) // If up and lookDir are parallel, create a right vector manually
	{
		right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		right = XMVector3Normalize(XMVector3Cross(up, lookDir));
	}

	up = XMVector3Normalize(XMVector3Cross(lookDir, right));

	billboardPosVec = XMVectorAdd(billboardPosVec, XMVectorScale(lookDir, _normalOffset));

	XMMATRIX billboardMatrix = XMMatrixIdentity();
	billboardMatrix.r[0] = _scale * right;
	billboardMatrix.r[1] = _scale * up;
	billboardMatrix.r[2] = _scale * lookDir;
	billboardMatrix.r[3] = billboardPosVec;
	billboardMatrix.r[3].m128_f32[3] = 1.0f;

	XMFLOAT4X4A billboardMatrixF;
	Store(billboardMatrixF, billboardMatrix);

	_meshBehaviour->GetTransform()->SetMatrix(billboardMatrixF, World);

	if (_scale >= 0.001f)
	{
		_meshBehaviour->GetTransform()->RotatePitch(90.0f * DEG_TO_RAD);
	}

	return true;
}

#ifdef USE_IMGUI
bool BillboardMeshBehaviour::RenderUI()
{
	ImGui::Checkbox("Keep Upright", &_keepUpright);
	ImGui::InputFloat("Rotation", &_rotation);
	ImGui::InputFloat("Normal Offset", &_normalOffset);
	ImGui::InputFloat("Scale", &_scale);

	return true;
}
#endif

void BillboardMeshBehaviour::OnEnable()
{
	_meshBehaviour->GetEntity()->Enable();
}
void BillboardMeshBehaviour::OnDisable()
{
	_meshBehaviour->GetEntity()->Disable();
}

bool BillboardMeshBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	/**code += 
		std::to_string(_keepUpright) + " " +
		std::to_string(_rotation) + " " +
		std::to_string(_normalOffset) + " ";

	std::string meshCode;
	if (!_meshBehaviour->Serialize(&meshCode))
	{
		Warn("Failed to serialize mesh behaviour!");
		return true;
	}

	int meshStart = meshCode.find("(");
	int meshEnd = meshCode.find(")");

	*code += meshCode.substr(meshStart + 1, meshEnd - meshStart - 1);*/

	return true;
}
bool BillboardMeshBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	// Standard code for all behaviours deserialize
	/*std::istringstream stream(code);
	std::vector<UINT> mesh;

	std::string value;
	while (stream >> value)
	{  // Automatically handles spaces correctly
		UINT attribute = std::stoul(value);
		mesh.emplace_back(attribute);
	}

	_isTransparent = mesh[1];

	// This section is for the initialization of the behaviour
	Material mat;
	std::memcpy(&mat, &mesh[2], sizeof(Material));

	SetMeshID(mesh[0], true);
	if (!SetMaterial(&mat))
		Warn("Failed to set material!");*/

	return true;
}

void BillboardMeshBehaviour::SetSize(float size)
{
	_scale = size;
}
void BillboardMeshBehaviour::SetRotation(float rotation)
{
	_rotation = rotation;
}

MeshBehaviour* BillboardMeshBehaviour::GetMeshBehaviour() const
{
	return _meshBehaviour;
}

