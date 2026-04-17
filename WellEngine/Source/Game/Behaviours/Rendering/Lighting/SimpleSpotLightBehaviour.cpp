#include "stdafx.h"
#include "SimpleSpotLightBehaviour.h"
#include "Game/Entity.h"
#include "Game/Scenes/Scene.h"
#include "SpotLightBehaviour.h"
#include "../Mesh/BillboardMeshBehaviour.h"
#include "Engine/Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

SimpleSpotLightBehaviour::~SimpleSpotLightBehaviour()
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

	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (!spotlights)
		return;

	if (!spotlights->UnregisterSimpleLight(this))
		DbgMsg("Failed to unregister simple spotlight!");
}

bool SimpleSpotLightBehaviour::Start()
{
	if (_name.empty())
		_name = "SimpleSpotLightBehaviour"; // For categorization in ImGui.

	SpotLightCollection *spotlights = GetScene()->GetSpotlights();

	if (spotlights)
	{
		if (!spotlights->RegisterSimpleLight(this))
		{
			ErrMsg("Failed to register simple spotlight!");
			return false;
		}
	}

#ifdef DEBUG_BUILD
	Material mat;
	mat.textureID = GetScene()->GetContent()->GetTextureID("LightSource");
	mat.ambientID = GetScene()->GetContent()->GetTextureID("White");

	auto billboardMeshBehaviour = new BillboardMeshBehaviour(mat, 0.0f, 0.0f, 0.5f, true, false, false, false, true);

	if (!billboardMeshBehaviour->Initialize(GetEntity()))
		Warn("Failed to Initialize billboard mesh behaviour!");

	_billboardMeshBehaviour = billboardMeshBehaviour;
#endif

	return true;
}

#ifdef USE_IMGUI
bool SimpleSpotLightBehaviour::RenderUI()
{
	if (ImGui::Button("Swap with Shadowcasting Variant"))
	{
		Entity *ent = GetEntity();

		ProjectionInfo projInfo = ProjectionInfo(
			min(_angle, 179.9f * DEG_TO_RAD), 
			1.0f, 
			{ 0.1f, CalculateLightReach(_color, _falloff) }
		);

		SpotLightBehaviour *shadowLight = new SpotLightBehaviour(
			projInfo, _color, _falloff, _fogStrength, _isOrtho
		);

		if (!shadowLight->Initialize(ent))
		{
			delete shadowLight;
			ErrMsg("Failed to initialize shadow spotlight!");
			return false;
		}

		ent->ReorderBehaviour(shadowLight, ent->GetBehaviourIndex(this) + 1);
		shadowLight->SetUIOpen(true);

		Destroy();
		return true;
	}

	float color[3] = { _color.x, _color.y, _color.z };
	float colorStrength = max(color[0], max(color[1], color[2]));

	color[0] /= colorStrength;
	color[1] /= colorStrength;
	color[2] /= colorStrength;

	bool newColor = false;
	if (ImGui::ColorEdit3("Color", color))
	{
		newColor = true;
		_recalculateBounds = true;
	}

	bool newStrength = false;
	if (ImGui::DragFloat("Intensity", &colorStrength, 0.01f, LIGHT_MIN_INTENSITY))
	{
		colorStrength = max(colorStrength, LIGHT_MIN_INTENSITY);
		newStrength = true;
		_recalculateBounds = true;
	}
	ImGuiUtils::LockMouseOnActive();

	if (newColor || newStrength)
	{
		float inputStr = max(color[0], max(color[1], color[2]));

		if (inputStr > LIGHT_MIN_INTENSITY)
		{
			_color.x = color[0] * colorStrength / inputStr;
			_color.y = color[1] * colorStrength / inputStr;
			_color.z = color[2] * colorStrength / inputStr;
		}
	}

	if (ImGui::DragFloat("Falloff", &_falloff, 0.01f, 0.001f))
	{
		_falloff = max(_falloff, 0.001f);
		_recalculateBounds = true;
	}
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Fog Dispersion", &_fogStrength, 0.005f);

	float angle = _angle * RAD_TO_DEG;
	if (ImGui::SliderFloat("Angle", &angle, 0.01f, 359.99f))
	{
		_angle = angle * DEG_TO_RAD;
		_recalculateBounds = true;
	}
	ImGuiUtils::LockMouseOnActive();

	ImGui::Separator();
	ImGui::Text("Light Reach: %.3f units", CalculateLightReach(_color, _falloff));

	return true;
}
#endif

void SimpleSpotLightBehaviour::OnEnable()
{
	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->RegisterSimpleLight(this))
		{
			ErrMsg("Failed to register simple spotlight!");
		}
	}
}
void SimpleSpotLightBehaviour::OnDisable()
{
	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->UnregisterSimpleLight(this))
		{
			ErrMsg("Failed to unregister simple spotlight!");
		}
	}
}

void SimpleSpotLightBehaviour::OnDirty()
{
	_recalculateBounds = true;
}

bool SimpleSpotLightBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Ortho", _isOrtho, docAlloc);
	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);
	obj.AddMember("Angle", _angle, docAlloc);

	json::Value colorArr(json::kArrayType);
	colorArr.PushBack(_color.x, docAlloc);
	colorArr.PushBack(_color.y, docAlloc);
	colorArr.PushBack(_color.z, docAlloc);
	obj.AddMember("Color", colorArr, docAlloc);

	return true;
}
bool SimpleSpotLightBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Ortho"))
		_isOrtho = obj["Ortho"].GetBool();

	if (obj.HasMember("Falloff"))
		_falloff = obj["Falloff"].GetFloat();

	if (obj.HasMember("Fog Strength"))
		_fogStrength = obj["Fog Strength"].GetFloat();

	if (obj.HasMember("Angle"))
		_angle = obj["Angle"].GetFloat();

	if (obj.HasMember("Color"))
		SerializerUtils::DeserializeVec(_color, obj["Color"]);

	_recalculateBounds = true;
	return true;
}

SimpleSpotLightBufferData SimpleSpotLightBehaviour::GetLightBufferData() const
{
	Transform *transform = GetTransform();

	SimpleSpotLightBufferData data = { };
	data.position = transform->GetPosition(World);
	data.direction = transform->GetForward(World);
	data.color = _color;
	data.angle = _angle;
	data.falloff = _falloff;
	data.orthographic = _isOrtho ? 1 : -1;
	data.fogStrength = _fogStrength;

	return data;
}
void SimpleSpotLightBehaviour::SetLightBufferData(XMFLOAT3 color, float angle, float falloff, bool isOrtho, float fogStrength)
{
	_color = color;
	_angle = angle;
	_falloff = falloff;
	_isOrtho = isOrtho;
	_fogStrength = fogStrength;

	_recalculateBounds = true;
}

bool SimpleSpotLightBehaviour::ContainsPoint(const XMFLOAT3A &point)
{
	if (_recalculateBounds)
	{
		float reach = CalculateLightReach(_color, _falloff);

		// Get the lights projection matrix and create a frustum from it.
		XMMATRIX projection = XMMatrixPerspectiveFovLH(_angle, 1.0f, 0.01f, reach);
		BoundingFrustum::CreateFromMatrix(_bounds, projection);

		// Transform the frustum to world space.
		XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		_bounds.Transform(_bounds, Load(worldMatrix));
		_recalculateBounds = false;
	}

	return _bounds.Contains(Load(point));
}
bool SimpleSpotLightBehaviour::IntersectsLightTile(const BoundingFrustum &tile)
{
	if (_recalculateBounds)
	{
		float reach = CalculateLightReach(_color, _falloff);

		// Get the lights projection matrix and create a frustum from it.
		XMMATRIX projection = XMMatrixPerspectiveFovLH(_angle, 1.0f, 0.01f, reach);
		BoundingFrustum::CreateFromMatrix(_bounds, projection);

		// Transform the frustum to world space.
		XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		_bounds.Transform(_bounds, Load(worldMatrix));
		_recalculateBounds = false;
	}

	dx::BoundingFrustum noNearFrustum(_bounds);
	noNearFrustum.Near = LIGHT_CULLING_NEAR_PLANE; // Ignore near plane for light tile intersection

	return tile.Intersects(noNearFrustum);
}
bool SimpleSpotLightBehaviour::IntersectsLightTile(const BoundingOrientedBox &tile)
{
	if (_recalculateBounds)
	{
		float reach = CalculateLightReach(_color, _falloff);

		// Get the lights projection matrix and create a frustum from it.
		XMMATRIX projection = XMMatrixPerspectiveFovLH(_angle, 1.0f, 0.01f, reach);
		BoundingFrustum::CreateFromMatrix(_bounds, projection);

		// Transform the frustum to world space.
		XMFLOAT4X4A worldMatrix = GetTransform()->GetWorldMatrix();
		_bounds.Transform(_bounds, Load(worldMatrix));
		_recalculateBounds = false;
	}

	dx::BoundingFrustum noNearFrustum(_bounds);
	noNearFrustum.Near = LIGHT_CULLING_NEAR_PLANE; // Ignore near plane for light tile intersection

	return tile.Intersects(noNearFrustum);
}
