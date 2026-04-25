#include "stdafx.h"
#include "B_LightPointSimple.h"
#include "Game/Entity.h"
#include "Game/Scene/Scene.h"
#include "B_LightPoint.h"
#include "../Mesh/B_MeshBillboard.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

B_LightPointSimple::~B_LightPointSimple()
{
	if (!IsInitialized())
		return;

#ifdef DEBUG_BUILD
	if (!GetScene()->IsDestroyed() && !GetEntity()->IsRemoved())
		if (_billboardMeshBehaviour.IsValid())
			_billboardMeshBehaviour.Get()->Destroy();
#endif

	if (!IsEnabled())
		return;

	LightPointCollection *pointlights = GetScene()->GetPointlights();
	if (!pointlights)
		return;

	if (!pointlights->UnregisterSimpleLight(this))
		DbgMsg("Failed to unregister simple pointlight!");
}

bool B_LightPointSimple::Start()
{
	LightPointCollection *pointlights = GetScene()->GetPointlights();

	if (!pointlights)
	{
		ErrMsg("Failed to get pointlight collection!");
		return false;
	}

	if (IsEnabled())
	{
		if (!pointlights->RegisterSimpleLight(this))
		{
			ErrMsg("Failed to register simple pointlight!");
			return false;
		}
	}

#ifdef DEBUG_BUILD
	Material mat;
	mat.textureID = GetScene()->GetContent()->GetTextureID("LightSource");
	mat.ambientID = GetScene()->GetContent()->GetTextureID("White");

	auto billboardMeshBehaviour = new B_MeshBillboard(mat, 0.0f, 0.0f, 0.5f, true, false, false, false, true);

	if (!billboardMeshBehaviour->Initialize(GetEntity()))
		Warn("Failed to Initialize billboard mesh behaviour!");

	_billboardMeshBehaviour = billboardMeshBehaviour;
#endif

	return true;
}

#ifdef USE_IMGUI
bool B_LightPointSimple::RenderUI()
{
	if (ImGui::Button("Swap with Shadowcasting Variant"))
	{
		Entity *ent = GetEntity();

		B_LightPoint *shadowLight = new B_LightPoint(
			{ 0.1f, CalculateLightReach(_color, _falloff) },
			_color, _falloff, _fogStrength
		);

		if (!shadowLight->Initialize(ent))
		{
			delete shadowLight;
			ErrMsg("Failed to initialize shadow pointlight!");
			return false;
		}

		ent->ReorderBehaviour(shadowLight, ent->GetBehaviourIndex(this) + 1);
		shadowLight->SetUIOpen(true);

		Destroy();
		return true;
	}

	float color[3] = { _color.x, _color.y, _color.z };
	float colorStrength = MAX(color[0], MAX(color[1], color[2]));

	color[0] /= colorStrength;
	color[1] /= colorStrength;
	color[2] /= colorStrength;

	bool newColor = false;
	if (ImGui::ColorEdit3("Color", color))
		newColor = true;

	bool newStrength = false;
	if (ImGui::DragFloat("Intensity", &colorStrength, 0.01f, LIGHT_MIN_INTENSITY))
	{
		colorStrength = MAX(colorStrength, LIGHT_MIN_INTENSITY);
		newStrength = true;
	}
	ImGuiUtils::LockMouseOnActive();

	if (newColor || newStrength)
	{
		float inputStr = MAX(color[0], MAX(color[1], color[2]));

		if (inputStr > LIGHT_MIN_INTENSITY)
		{
			_color.x = color[0] * colorStrength / inputStr;
			_color.y = color[1] * colorStrength / inputStr;
			_color.z = color[2] * colorStrength / inputStr;
		}
	}

	if (ImGui::DragFloat("Falloff", &_falloff, 0.01f, 0.001f))
	{
		_falloff = MAX(_falloff, 0.001f);
	}
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Fog Dispersion", &_fogStrength, 0.005f);
	ImGuiUtils::LockMouseOnActive();

	static bool drawBounds = false;
	ImGui::Checkbox("Draw Bounds", &drawBounds);

	if (drawBounds)
	{
		float reach = CalculateLightReach(_color, _falloff);
		BoundingBox boxBounds = BoundingBox(GetTransform()->GetPosition(World), XMFLOAT3(reach, reach, reach));
		DebugDrawer::Instance().DrawBoxAABB(boxBounds, XMFLOAT4(color[0], color[1], color[2], 0.2f), 0.0f, true);
	}

	ImGui::Separator();
	ImGui::Text("Light Reach: %.3f units", CalculateLightReach(_color, _falloff));

	return true;
}
#endif

void B_LightPointSimple::OnEnable()
{
	LightPointCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->RegisterSimpleLight(this))
		{
			ErrMsg("Failed to register simple pointlight!");
		}
	}
}
void B_LightPointSimple::OnDisable()
{
	LightPointCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->UnregisterSimpleLight(this))
		{
			ErrMsg("Failed to unregister simple pointlight!");
		}
	}
}

bool B_LightPointSimple::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);

	json::Value colorArr(json::kArrayType);
	colorArr.PushBack(_color.x, docAlloc);
	colorArr.PushBack(_color.y, docAlloc);
	colorArr.PushBack(_color.z, docAlloc);
	obj.AddMember("Color", colorArr, docAlloc);

	return true;
}
bool B_LightPointSimple::Deserialize(const json::Value &obj, Scene *scene)
{
	_falloff		= obj["Falloff"].GetFloat();
	_fogStrength	= obj["Fog Strength"].GetFloat();
	SerializerUtils::DeserializeVec(_color, obj["Color"]);

	return true;
}

SimplePointLightBufferData B_LightPointSimple::GetLightBufferData() const
{
	Transform *transform = GetTransform();

	SimplePointLightBufferData data = { };
	data.position = transform->GetPosition(World);
	data.color = _color;
	data.falloff = _falloff;
	data.fogStrength = _fogStrength;

	return data;
}
void B_LightPointSimple::SetLightBufferData(XMFLOAT3 color, float falloff, float fogStrength)
{
	_color = color;
	_falloff = falloff;
	_fogStrength = fogStrength;
}

bool B_LightPointSimple::ContainsPoint(const XMFLOAT3A &point) const
{
	float reach = CalculateLightReach(_color, _falloff);

	// Test sphere-point intersection.
	BoundingSphere sphereBounds = BoundingSphere(GetTransform()->GetPosition(World), reach);
	
	return sphereBounds.Contains(Load(point));
}
bool B_LightPointSimple::IntersectsLightTile(const dx::BoundingFrustum &tile) const
{
	float reach = CalculateLightReach(_color, _falloff);

	BoundingBox boxBounds = BoundingBox(GetTransform()->GetPosition(World), XMFLOAT3(reach, reach, reach));

	// Test box-frustum intersection.
	//if (!tile.Intersects(boxBounds))
	//	return false;

	// Test sphere-frustum intersection.
	BoundingSphere sphereBounds = BoundingSphere(boxBounds.Center, reach);

	return tile.Intersects(sphereBounds);
}
bool B_LightPointSimple::IntersectsLightTile(const dx::BoundingOrientedBox &tile) const
{
	float reach = CalculateLightReach(_color, _falloff);

	BoundingBox boxBounds = BoundingBox(GetTransform()->GetPosition(World), XMFLOAT3(reach, reach, reach));

	// Test box-box intersection.
	//if (!tile.Intersects(boxBounds))
	//	return false;

	// Test sphere-box intersection.
	BoundingSphere sphereBounds = BoundingSphere(boxBounds.Center, reach);

	return tile.Intersects(sphereBounds);
}
