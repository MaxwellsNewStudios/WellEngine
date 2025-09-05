#include "stdafx.h"
#include "Behaviours/CompassBehaviour.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool CompassBehaviour::Start()
{
	if (_name.empty())
		_name = "CompassBehaviour";

	Content* content = GetScene()->GetContent();

	dx::XMFLOAT3A scale = { 0.1f, 0.1f, 0.1f };

	// Compass body
	{
		UINT meshID = content->GetMeshID("CompassFlipped");
		Material mat{};
		mat.textureID = content->GetTextureID("Compass_SpecularMaterial_Diffuse");
		mat.normalID = content->GetTextureID("Compass_SpecularMaterial_Normal");
		mat.specularID = content->GetTextureID("Compass_SpecularMaterial_Specular");
		mat.glossinessID = content->GetTextureID("Compass_SpecularMaterial_Glossiness");
		mat.ambientID = content->GetTextureID("White");
		mat.vsID = content->GetShaderID("VS_Geometry");

		if (!GetScene()->CreateMeshEntity(&_compassBodyMesh, "Compass Body Mesh", meshID, mat, false, false, true))
		{
			ErrMsg("Failed to initialize compass");
			return false;
		}
		_compassBodyMesh->SetSerialization(false);
		_compassBodyMesh->SetParent(GetEntity());
		_compassBodyMesh->GetTransform()->SetScale(scale);
	}

	// Compass needle
	{
		UINT meshID = content->GetMeshID("CompassNeedle");
		Material mat{};
		mat.textureID = content->GetTextureID("Stalagmites_Small_1");
		mat.ambientID = content->GetTextureID("Red");
		mat.vsID = content->GetShaderID("VS_Geometry");

		if (!GetScene()->CreateMeshEntity(&_compassNeedleMesh, "Compass Needle Mesh", meshID, mat, false, false, true))
		{
			ErrMsg("Failed to initialize compass needle");
			return false;
		}
		_compassNeedleMesh->SetSerialization(false);
		_compassNeedleMesh->SetParent(_compassBodyMesh);
		_compassBodyMesh->GetTransform()->SetScale(scale);

		_compassNeedle = _compassNeedleMesh->GetTransform();
	}

	QueueLateUpdate();

	return true;
}

bool CompassBehaviour::LateUpdate(TimeUtils &time, const Input& input)
{
	float playerRotationY = GetScene()->GetPlayer()->GetTransform()->GetEuler(World).y;
	_compassNeedle->SetEuler({ 0.0f, dx::XM_PI - playerRotationY, 0.0f });

	return true;
}

void CompassBehaviour::OnEnable()
{
	_compassBodyMesh->Enable();
	_compassNeedleMesh->Enable();
}

void CompassBehaviour::OnDisable()
{
	_compassBodyMesh->Disable();
	_compassNeedleMesh->Disable();
}
