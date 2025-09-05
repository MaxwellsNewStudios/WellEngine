#include "stdafx.h"
#include "Scenes/Scene.h"
#include "Behaviours/BreadcrumbBehaviour.h"
#include "Behaviours/FlashlightBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

BreadcrumbBehaviour::BreadcrumbBehaviour(BreadcrumbColor color) : _color(color)
{

}

bool BreadcrumbBehaviour::Start()
{
	if (_name.empty())
		_name = "BreadcrumbBehaviour"; // For categorization in ImGui.

	Content *content = GetScene()->GetContent();

	std::string crumbColorName = "";
	switch (_color)
	{
	case BreadcrumbColor::Red:
		crumbColorName = "Red";
		break;

	case BreadcrumbColor::Green:
		crumbColorName = "Green";
		break;

	case BreadcrumbColor::Blue:
		crumbColorName = "Blue";
		break;
	}

	// Create a rock mesh behaviour
	UINT meshID = content->GetMeshID("Breadcrumb");
	BoundingOrientedBox bounds = content->GetMesh(meshID)->GetBoundingOrientedBox();

	Material mat;
	mat.textureID = content->GetTextureID(std::format("Breadcrumb_{}", crumbColorName));
	mat.normalID = content->GetTextureID("Breadcrumb_Normal");
	mat.specularID = content->GetTextureID(std::format("Breadcrumb_{}_Specular", crumbColorName));
	mat.glossinessID = content->GetTextureID("Breadcrumb_Glossiness");
	mat.occlusionID = content->GetTextureID("Breadcrumb_Occlusion");
	mat.ambientID = content->GetTextureID(std::format("Breadcrumb_{}_Light", crumbColorName));

	MeshBehaviour *meshBehaviour = new MeshBehaviour(bounds, meshID, &mat);
	if (!meshBehaviour->Initialize(GetEntity()))
		Warn("Failed to create mesh!");

	meshBehaviour->SetSerialization(false);

	// Create a billboard mesh behaviour as a child entity
	mat = {};
	mat.textureID = content->GetTextureID(std::format("Flare_{}", crumbColorName));
	mat.ambientID = content->GetTextureID(std::format("{}", crumbColorName));

	Entity *entity;
	if (!GetScene()->CreateBillboardMeshEntity(&entity, "Flare Billboard Mesh", mat, 0.0f, 0.25f, 0.01f))
		Warn("Failed to create billboard mesh entity!");

	entity->SetParent(GetEntity());
	entity->SetSerialization(false);

	entity->GetBehaviourByType<BillboardMeshBehaviour>(_flare);
	_flare->GetTransform()->Move({ 0.0f, 0.025f, 0.0f }, Local);

#ifdef DEBUG_BUILD
	GetEntity()->SetSerialization(false);
#endif

	QueueUpdate();

	return true; //-V773
}

bool BreadcrumbBehaviour::Update(TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();

	if (!_flashlight)
	{
		Entity *flashlightEntity = scene->GetSceneHolder()->GetEntityByName("Flashlight");

		if (!flashlightEntity)
			return true;

		if (!flashlightEntity->GetBehaviourByType<FlashlightBehaviour>(_flashlight))
			return true;

		if (!_flashlight)
			return true;
	}

	SpotLightBehaviour *spotlight = _flashlight->GetLight();
	if (!spotlight->IsEnabled())
	{
		_flare->SetSize(0.0f);
		return true;
	}

	XMFLOAT3A breadcrumbPos = GetTransform()->GetPosition(World);

	if (!spotlight->ContainsPoint(breadcrumbPos))
	{
		_flare->SetSize(0.0f);
		return true;
	}

	XMVECTOR crumbPos = Load(breadcrumbPos);
	XMVECTOR lightPos = Load(spotlight->GetTransform()->GetPosition(World));
	XMVECTOR lightDir = Load(spotlight->GetTransform()->GetForward(World));
	XMVECTOR crumbDir = XMVector3Normalize(crumbPos - lightPos);

	float angle = XMVectorGetX(XMVector3AngleBetweenVectors(lightDir, crumbDir));
	float lightAngle = spotlight->GetShadowCamera()->GetFOV();

	float flareStrength = 1.0f - std::clamp(angle / lightAngle, 0.0f, 1.0f);

	flareStrength = pow(flareStrength, 3.0f);
	_flare->SetSize(flareStrength * 0.3f);
	return true;
}


bool BreadcrumbBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Color", (UINT)_color, docAlloc);
	return true;
}
bool BreadcrumbBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_color = (BreadcrumbColor)obj["Color"].GetUint();

	return true;
}
