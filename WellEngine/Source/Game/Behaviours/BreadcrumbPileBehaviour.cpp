#include "stdafx.h"
#include "Scenes/Scene.h"
#include "Behaviours/BreadcrumbPileBehaviour.h"
#include "Behaviours/InventoryBehaviour.h"
#include "Behaviours/FlashlightBehaviour.h"
#include "Game.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool BreadcrumbPileBehaviour::Start()
{
	if (_name.empty())
		_name = "BreadcrumbPileBehaviour"; // For categorization in ImGui.

	Entity *ent = GetEntity();

	if (GetScene()->GetName() != "MainMenu") // If not in menu scene
	{
		InteractableBehaviour *interactableBehaviour;
		if (!ent->GetBehaviourByType<InteractableBehaviour>(interactableBehaviour))
		{
			interactableBehaviour = new InteractableBehaviour();
			if (!interactableBehaviour->Initialize(ent))
				Warn("Failed to initialize interactable behaviour!");
		}
		interactableBehaviour->AddInteractionCallback(std::bind(&BreadcrumbPileBehaviour::Pickup, this));
	}

	// Create a rock mesh behaviour
	Content *content = GetScene()->GetContent();
	UINT meshID = content->GetMeshID("Breadcrumb_Collection");
	BoundingOrientedBox bounds = content->GetMesh(meshID)->GetBoundingOrientedBox();

	Material mat;
	mat.textureID = content->GetTextureID("Breadcrumb_Collection");
	mat.normalID = content->GetTextureID("Breadcrumb_Collection_Normal");
	mat.specularID = content->GetTextureID("Breadcrumb_Collection_Specular");
	mat.glossinessID = content->GetTextureID("Breadcrumb_Collection_Glossiness");
	mat.occlusionID = content->GetTextureID("Breadcrumb_Collection_Occlusion");
	mat.ambientID = content->GetTextureID("Breadcrumb_Collection_Light");

	MeshBehaviour *meshBehaviour = new MeshBehaviour(bounds, meshID, &mat);
	if (!meshBehaviour->Initialize(ent))
		Warn("Failed to create mesh!");

	meshBehaviour->SetSerialization(false);
	ent->SetEntityBounds(bounds);

	// Create a billboard mesh behaviour as a child entity
	mat = {};
	mat.textureID = content->GetTextureID("Flare");
	mat.ambientID = content->GetTextureID("White");

	float normalOffset = 0.25f;
	if (GetScene()->GetName() == "MainMenu") // If menu scene is active
		normalOffset *= 10.0f;

	Entity *entity;
	if (!GetScene()->CreateBillboardMeshEntity(&entity, "Flare Billboard Mesh", mat, 0.0f, normalOffset, 0.01f))
		Warn("Failed to create billboard mesh entity!");

	entity->SetParent(ent);
	entity->SetSerialization(false);

	entity->GetBehaviourByType<BillboardMeshBehaviour>(_flare);
	_flare->GetTransform()->Move({ 0.0f, 0.05f, 0.0f }, Local);

	QueueUpdate();

	return true;
}

bool BreadcrumbPileBehaviour::Update(TimeUtils &time, const Input &input)
{
	Scene *scene = GetScene();

	XMFLOAT3A breadcrumbPos = GetTransform()->GetPosition(World);

	Transform *lightTransform;
	float lightAngle;
	float sizeMultiplier;

	if (scene->GetName() == "MainMenu") // If in menu scene
	{
		if (!_flashlightEntity)
			_flashlightEntity = scene->GetSceneHolder()->GetEntityByName("Spotlight Flashlight");

		if (!_flashlightEntity)
			return true;

		SimpleSpotLightBehaviour *simpleSpotlight;
		if (!_flashlightEntity->GetBehaviourByType<SimpleSpotLightBehaviour>(simpleSpotlight))
		{
			Warn("Failed to find spotlight!");
			return true;
		}

		if (!simpleSpotlight->IsEnabled())
		{
			_flare->SetSize(0.0f);
			return true;
		}

		if (!simpleSpotlight->ContainsPoint(breadcrumbPos))
		{
			_flare->SetSize(0.0f);
			return true;
		}

		sizeMultiplier = 3.0f;
		lightTransform = simpleSpotlight->GetTransform();
		lightAngle = simpleSpotlight->GetLightBufferData().angle;
	}
	else
	{
		if (!_flashlightEntity)
			_flashlightEntity = scene->GetSceneHolder()->GetEntityByName("Flashlight");

		if (!_flashlightEntity)
			return true;

		FlashlightBehaviour *flashlight;
		if (!_flashlightEntity->GetBehaviourByType<FlashlightBehaviour>(flashlight))
			return true;

		SpotLightBehaviour *spotlight = flashlight->GetLight();

		if (!spotlight->IsEnabled())
		{
			_flare->SetSize(0.0f);
			return true;
		}

		if (!spotlight->ContainsPoint(breadcrumbPos))
		{
			_flare->SetSize(0.0f);
			return true;
		}

		sizeMultiplier = 1.0f;
		lightTransform = spotlight->GetTransform();
		lightAngle = spotlight->GetShadowCamera()->GetFOV();
	}

	XMVECTOR crumbPos = Load(breadcrumbPos);
	XMVECTOR lightPos = Load(lightTransform->GetPosition(World));
	XMVECTOR lightDir = Load(lightTransform->GetForward(World));
	XMVECTOR crumbDir = XMVector3Normalize(crumbPos - lightPos);

	float angle = XMVectorGetX(XMVector3AngleBetweenVectors(lightDir, crumbDir));

	float flareStrength = 1.0f - std::clamp(angle / lightAngle, 0.0f, 1.0f);

	flareStrength = pow(flareStrength, 3.0f);
	_flare->SetSize(flareStrength * 0.5f * sizeMultiplier);

	return true;
}

bool BreadcrumbPileBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}
bool BreadcrumbPileBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}

void BreadcrumbPileBehaviour::Pickup()
{
	SceneHolder *sceneHolder = GetScene()->GetSceneHolder();

	if (Entity *playerEnt = sceneHolder->GetEntityByName("Player Entity"))
	{
		InventoryBehaviour *inventory;
		if (playerEnt->GetBehaviourByType<InventoryBehaviour>(inventory))
		{
			inventory->AddBreadcrumbs(10);
		}
	}

	if (!sceneHolder->RemoveEntity(GetEntity()))
	{
		Warn("Failed to remove entity from scene holder!");
		return;
	}
}
