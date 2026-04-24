#include "stdafx.h"
#include "B_LightPoint.h"
#include "Game/Entity.h"
#include "Game/Scene/Scene.h"
#include "B_LightPointSimple.h"
#include "../Mesh/B_MeshBillboard.h"
#include "Engine/Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

B_LightPoint::B_LightPoint(B_CameraCube *cameraCube, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
	_shadowCameraCube(cameraCube), _color(color), _falloff(falloff), _fogStrength(fogStrength), _updateFrequency(updateFrequency)
{
	if (_shadowCameraCube)
		_shadowCameraCube->SetFarZ(CalculateLightReach(color, falloff));

}
B_LightPoint::B_LightPoint(CameraPlanes planes, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
	_shadowCameraCube(nullptr), _color(color), _falloff(falloff), _fogStrength(fogStrength), _updateFrequency(updateFrequency)
{
	float reach = CalculateLightReach(color, falloff);

	if (planes.nearZ < planes.farZ)
		planes.farZ = reach;
	else
		planes.nearZ = reach;

	planes.nearZ = planes.nearZ < 0.05f ? 0.05f : planes.nearZ;

	_initialCameraPlanes = planes;
}
B_LightPoint::~B_LightPoint()
{
	if (!IsInitialized())
		return;

	if (_shadowCameraCube)
	{
		_shadowCameraCube->Destroy();
		_shadowCameraCube = nullptr;
	}

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

	if (!pointlights->UnregisterLight(this))
		DbgMsg("Failed to unregister pointlight!");
}

bool B_LightPoint::Start()
{
	if (!_shadowCameraCube)
	{
		_shadowCameraCube = new B_CameraCube(_initialCameraPlanes, true);

		if (!_shadowCameraCube->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind shadow camera cube to pointlight!");
			return false;
		}
	}

	_shadowCameraCube->SetRendererInfo({ false, true });
	_shadowCameraCube->SetSerialization(false);
	_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));

	LightPointCollection *pointlights = GetScene()->GetPointlights();

	if (!pointlights)
	{
		ErrMsg("Failed to get pointlight collection!");
		return false;
	}

	if (IsEnabled())
	{
		if (!pointlights->RegisterLight(this))
		{
			ErrMsg("Failed to register pointlight!");
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

	QueueParallelUpdate();

	return true;
}

bool B_LightPoint::ParallelUpdate(const TimeUtils &time, const Input &input)
{
	if (_updateTimer <= 0)
	{
		_updateTimer += (int)_updateFrequency;
		_updateShadows = true;
		_boundsDirty = true;
	}

	_updateTimer--;
	return true;
}

#ifdef USE_IMGUI
bool B_LightPoint::RenderUI()
{
	if (ImGui::Button("Swap with Non-Shadowcasting Variant"))
	{
		Entity *ent = GetEntity();

		B_LightPointSimple *simpleLight = new B_LightPointSimple(
			_color, _falloff,  _fogStrength
		);

		if (!simpleLight->Initialize(ent))
		{
			delete simpleLight;
			ErrMsg("Failed to initialize simple pointlight!");
			return false;
		}

		ent->ReorderBehaviour(simpleLight, ent->GetBehaviourIndex(this) + 1);
		simpleLight->SetUIOpen(true);

		Destroy();
		return true;
	}

	float color[3] = { _color.x, _color.y, _color.z };
	float colorStrength = MAX(color[0], MAX(color[1], color[2]));

	color[0] /= colorStrength;
	color[1] /= colorStrength;
	color[2] /= colorStrength;

	bool recalculateReach = false;

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
		recalculateReach = true;
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
		recalculateReach = true;
	}
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Fog Dispersion", &_fogStrength, 0.005f);
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Shadow Strength", &_shadowStrength, 0.005f, FLT_MIN, 1.0f);
	ImGuiUtils::LockMouseOnActive();

	if (recalculateReach && _shadowCameraCube)
		_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));

	UINT step = 1;
	UINT stepFast = 5;
	if (ImGui::InputScalar("Shadow Update Frequency", ImGuiDataType_U32, &_updateFrequency, &step, &stepFast))
		_updateFrequency = MAX(_updateFrequency, 1);

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

void B_LightPoint::OnEnable()
{
	LightPointCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->RegisterLight(this))
		{
			ErrMsg("Failed to register pointlight!");
		}
	}
}
void B_LightPoint::OnDisable()
{
	LightPointCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->UnregisterLight(this))
		{
			ErrMsg("Failed to unregister pointlight!");
		}
	}
}

bool B_LightPoint::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Update Frequency", _updateFrequency, docAlloc);

	// Save near and far plane for easy initialization
	dx::XMFLOAT4X4 projectionMatrix = _shadowCameraCube->GetProjectionMatrix();
	float nearPlane = projectionMatrix._43 / projectionMatrix._33;
	float farPlane = projectionMatrix._43 / (projectionMatrix._33 - 1);

	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);
	obj.AddMember("Shadow Strength", _shadowStrength, docAlloc);
	obj.AddMember("Near", nearPlane, docAlloc);
	obj.AddMember("Far", farPlane, docAlloc);

	json::Value colorArr = SerializerUtils::SerializeVec(_color, docAlloc);
	obj.AddMember("Color", colorArr, docAlloc);

	return true;
}
bool B_LightPoint::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Update Frequency"))
		_updateFrequency = obj["Update Frequency"].GetUint();

	float 
		falloff = 1.0f, 
		fogStrength = 1.0f, 
		shadowStrength = 1.0f, 
		nearPlane = 0.1f, 
		farPlane = 10.0f;

	if (obj.HasMember("Falloff"))
		falloff = obj["Falloff"].GetFloat();
	if (obj.HasMember("Fog Strength"))
		fogStrength = obj["Fog Strength"].GetFloat();
	if (obj.HasMember("Shadow Strength"))
		shadowStrength = obj["Shadow Strength"].GetFloat();
	if (obj.HasMember("Near"))
		nearPlane = obj["Near"].GetFloat();
	if (obj.HasMember("Far"))
		farPlane = obj["Far"].GetFloat();

	dx::XMFLOAT3 color;
	SerializerUtils::DeserializeVec(color, obj["Color"]);

	SetLightBufferData(color, falloff, fogStrength, shadowStrength);
	_initialCameraPlanes = { nearPlane * -1, farPlane * -1};

	return true;
}

UINT B_LightPoint::GetUpdateFrequency() const
{
	return _updateFrequency;
}
void B_LightPoint::SetUpdateFrequency(UINT frequency)
{
	_updateFrequency = MAX(1, frequency);
}
int B_LightPoint::GetUpdateTimer() const
{
	return _updateTimer;
}
void B_LightPoint::SetUpdateTimer(int timer)
{
	_updateTimer = timer;
}

void B_LightPoint::ForceUpdate()
{
	_updateShadows = true;
	_boundsDirty = true;
}
void B_LightPoint::MarkUpdated()
{
	_updateShadows = false;
}
bool B_LightPoint::DoUpdate() const
{
	return _updateShadows;
}
bool B_LightPoint::UpdateBuffers()
{
	if (!DoUpdate())
		return true;

	if (!_shadowCameraCube->UpdateBuffers())
	{
		ErrMsg("Failed to update shadow camera cube buffers!");
		return false;
	}

	return true;
}

PointLightBufferData B_LightPoint::GetLightBufferData()
{
	if (!DoUpdate())
		return _lastLightBufferData;

	PointLightBufferData &data = _lastLightBufferData;
	data.position = GetTransform()->GetPosition(World);
	data.falloff = _falloff;
	data.color = _color;
	data.fogStrength = _fogStrength;
	data.shadowStrength = _shadowStrength;
	data.nearZ = _shadowCameraCube->GetNearZ();
	data.farZ = _shadowCameraCube->GetFarZ();

	return data;
}
void B_LightPoint::SetLightBufferData(XMFLOAT3 color, float falloff, float fogStrength, float shadowStrength)
{
	_color = color;
	_falloff = falloff;
	_fogStrength = fogStrength;
	_shadowStrength = shadowStrength;

	if (_shadowCameraCube)
	{
		_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));
	}
}

void B_LightPoint::SetIntensity(float intensity)
{
	float maxChannel = MAX(_color.x, MAX(_color.y, _color.z));
	_color.x = (_color.x / maxChannel) * intensity;
	_color.y = (_color.y / maxChannel) * intensity;
	_color.z = (_color.z / maxChannel) * intensity;

	if (_shadowCameraCube)
	{
		float reach = CalculateLightReach(_color, _falloff);
		_shadowCameraCube->SetFarZ(reach);
	}
}
void B_LightPoint::SetColor(dx::XMFLOAT3 color)
{
	_color = color;

	if (_shadowCameraCube)
	{
		_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));
	}
}
void B_LightPoint::SetFalloff(float falloff)
{
	_falloff = falloff;

	if (_shadowCameraCube)
	{
		_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));
	}
}
void B_LightPoint::SetFogStrength(float fogStrength)
{
	_fogStrength = fogStrength;
}
void B_LightPoint::SetShadowStrength(float shadowStrength)
{
	_shadowStrength = shadowStrength;
}

B_CameraCube *B_LightPoint::GetShadowCameraCube() const
{
	return _shadowCameraCube;
}

bool B_LightPoint::ContainsPoint(const XMFLOAT3A &point)
{
	XMFLOAT3 lightPos = _transformedBounds.Center;
	if (DoUpdate())
	{
#pragma omp critical
		{
			if (_boundsDirty)
				lightPos = GetTransform()->GetPosition(World);
		}
	}

	// Test sphere-point intersection.
	BoundingSphere lightSphere(GetTransform()->GetPosition(World), _shadowCameraCube->GetFarZ());

	return lightSphere.Contains(Load(point));
}
bool B_LightPoint::IntersectsLightTile(const BoundingFrustum &tile)
{
	if (DoUpdate())
	{
		bool failed = false;
#pragma omp critical
		{
			if (_boundsDirty)
			{
				_boundsDirty = false;
				if (!_shadowCameraCube->StoreBounds(_transformedBounds))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					failed = true;
				}
			}
		}

		if (failed)
			return false;
	}

	// Test box-frustum intersection.
	//if (!tile.Intersects(_transformedBounds))
	//	return false;

	// Test sphere-frustum intersection.
	BoundingSphere lightSphere(_transformedBounds.Center, _transformedBounds.Extents.x);

	return tile.Intersects(lightSphere);
}
bool B_LightPoint::IntersectsLightTile(const BoundingOrientedBox &tile)
{
	if (DoUpdate())
	{
		bool failed = false;
#pragma omp critical
		{
			if (_boundsDirty)
			{
				_boundsDirty = false;
				if (!_shadowCameraCube->StoreBounds(_transformedBounds))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					failed = true;
				}
			}
		}

		if (failed)
			return false;
	}

	// Test box-box intersection.
	//if (!tile.Intersects(_transformedBounds))
	//	return false;

	// Test sphere-box intersection.
	BoundingSphere lightSphere(_transformedBounds.Center, _transformedBounds.Extents.x);

	return tile.Intersects(lightSphere);
}
