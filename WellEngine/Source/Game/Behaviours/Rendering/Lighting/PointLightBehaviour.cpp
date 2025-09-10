#include "stdafx.h"
#include "PointLightBehaviour.h"
#include "Source/Game/Entity.h"
#include "Source/Game/Scenes/Scene.h"
#include "SimplePointLightBehaviour.h"
#include "../Mesh/BillboardMeshBehaviour.h"
#include "Source/Engine/Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

PointLightBehaviour::PointLightBehaviour(CameraCubeBehaviour *cameraCube, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
	_shadowCameraCube(cameraCube), _color(color), _falloff(falloff), _fogStrength(fogStrength), _updateFrequency(updateFrequency)
{
	if (_shadowCameraCube)
		_shadowCameraCube->SetFarZ(CalculateLightReach(color, falloff));

}
PointLightBehaviour::PointLightBehaviour(CameraPlanes planes, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
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
PointLightBehaviour::~PointLightBehaviour()
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

	PointLightCollection *pointlights = GetScene()->GetPointlights();
	if (!pointlights)
		return;

	if (!pointlights->UnregisterLight(this))
		DbgMsg("Failed to unregister pointlight!");
}

bool PointLightBehaviour::Start()
{
	if (_name.empty())
		_name = "PointLightBehaviour"; // For categorization in ImGui.
	
	if (!_shadowCameraCube)
	{
		_shadowCameraCube = new CameraCubeBehaviour(_initialCameraPlanes, true);

		if (!_shadowCameraCube->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind shadow camera cube to pointlight!");
			return false;
		}
	}

	_shadowCameraCube->SetRendererInfo({ false, true });
	_shadowCameraCube->SetSerialization(false);
	_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));

	PointLightCollection *pointlights = GetScene()->GetPointlights();

	if (!pointlights)
	{
		ErrMsg("Failed to get pointlight collection!");
		return false;
	}

	if (!pointlights->RegisterLight(this))
	{
		ErrMsg("Failed to register pointlight!");
		return false;
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

	QueueParallelUpdate();

	return true;
}

bool PointLightBehaviour::ParallelUpdate(const TimeUtils &time, const Input &input)
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
bool PointLightBehaviour::RenderUI()
{
	if (ImGui::Button("Swap with Non-Shadowcasting Variant"))
	{
		Entity *ent = GetEntity();

		SimplePointLightBehaviour *simpleLight = new SimplePointLightBehaviour(
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
	float colorStrength = max(color[0], max(color[1], color[2]));

	color[0] /= colorStrength;
	color[1] /= colorStrength;
	color[2] /= colorStrength;

	bool recalculateReach = false;

	bool newColor = false;
	if (ImGui::ColorEdit3("Color", color))
		newColor = true;

	bool newStrength = false;
	if (ImGui::DragFloat("Intensity", &colorStrength, 0.01f, 0.1f))
	{
		colorStrength = max(colorStrength, 0.1f);
		newStrength = true;
	}
	ImGuiUtils::LockMouseOnActive();

	if (newColor || newStrength)
	{
		recalculateReach = true;
		float inputStr = max(color[0], max(color[1], color[2]));

		if (inputStr > 0.1f)
		{
			_color.x = color[0] * colorStrength / inputStr;
			_color.y = color[1] * colorStrength / inputStr;
			_color.z = color[2] * colorStrength / inputStr;
		}
	}

	if (ImGui::DragFloat("Falloff", &_falloff, 0.01f, 0.001f))
	{
		_falloff = max(_falloff, 0.001f);
		recalculateReach = true;
	}
	ImGuiUtils::LockMouseOnActive();

	ImGui::DragFloat("Fog Dispersion", &_fogStrength, 0.005f);
	ImGuiUtils::LockMouseOnActive();

	if (recalculateReach && _shadowCameraCube)
		_shadowCameraCube->SetFarZ(CalculateLightReach(_color, _falloff));

	float step = 1.0f;
	float stepFast = 5.0f;
	if (ImGui::InputScalar("Shadow Update Frequency", ImGuiDataType_U32, &_updateFrequency, &step, &stepFast))
		_updateFrequency = max(_updateFrequency, 1);

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

void PointLightBehaviour::OnEnable()
{
	PointLightCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->RegisterLight(this))
		{
			ErrMsg("Failed to register pointlight!");
		}
	}
}
void PointLightBehaviour::OnDisable()
{
	PointLightCollection *pointlights = GetScene()->GetPointlights();
	if (pointlights)
	{
		if (!pointlights->UnregisterLight(this))
		{
			ErrMsg("Failed to unregister pointlight!");
		}
	}
}

bool PointLightBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Update Frequency", _updateFrequency, docAlloc);

	// Save near and far plane for easy initialization
	dx::XMFLOAT4X4 projectionMatrix = _shadowCameraCube->GetProjectionMatrix();
	float nearPlane = projectionMatrix._43 / projectionMatrix._33;
	float farPlane = projectionMatrix._43 / (projectionMatrix._33 - 1);

	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);
	obj.AddMember("Near", nearPlane, docAlloc);
	obj.AddMember("Far", farPlane, docAlloc);

	json::Value colorArr = SerializerUtils::SerializeVec(_color, docAlloc);
	obj.AddMember("Color", colorArr, docAlloc);

	return true;
}
bool PointLightBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Update Frequency"))
		_updateFrequency = obj["Update Frequency"].GetUint();

	float falloff		= obj["Falloff"].GetFloat();
	float fogStrength	= obj["Fog Strength"].GetFloat();
	float nearPlane		= obj["Near"].GetFloat();
	float farPlane		= obj["Far"].GetFloat();

	dx::XMFLOAT3 color;
	SerializerUtils::DeserializeVec(color, obj["Color"]);

	SetLightBufferData(color, falloff, fogStrength);
	_initialCameraPlanes = { nearPlane * -1, farPlane * -1};

	return true;
}

UINT PointLightBehaviour::GetUpdateFrequency() const
{
	return _updateFrequency;
}
void PointLightBehaviour::SetUpdateFrequency(UINT frequency)
{
	_updateFrequency = max(1, frequency);
}
int PointLightBehaviour::GetUpdateTimer() const
{
	return _updateTimer;
}
void PointLightBehaviour::SetUpdateTimer(int timer)
{
	_updateTimer = timer;
}

void PointLightBehaviour::ForceUpdate()
{
	_updateShadows = true;
	_boundsDirty = true;
}
void PointLightBehaviour::MarkUpdated()
{
	_updateShadows = false;
}
bool PointLightBehaviour::DoUpdate() const
{
	return _updateShadows;
}
bool PointLightBehaviour::UpdateBuffers()
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

PointLightBufferData PointLightBehaviour::GetLightBufferData()
{
	if (!DoUpdate())
		return _lastLightBufferData;

	PointLightBufferData &data = _lastLightBufferData;
	data.position = GetTransform()->GetPosition(World);
	data.falloff = _falloff;
	data.color = _color;
	data.fogStrength = _fogStrength;
	data.nearZ = _shadowCameraCube->GetNearZ();
	data.farZ = _shadowCameraCube->GetFarZ();

	return data;
}
void PointLightBehaviour::SetLightBufferData(XMFLOAT3 color, float falloff, float fogStrength)
{
	_color = color;
	_falloff = falloff;
	_fogStrength = fogStrength;

	if (_shadowCameraCube)
		_shadowCameraCube->SetFarZ(CalculateLightReach(color, falloff));
}
void PointLightBehaviour::SetIntensity(float intensity)
{
	float maxChannel = max(_color.x, max(_color.y, _color.z));
	_color.x = (_color.x / maxChannel) * intensity;
	_color.y = (_color.y / maxChannel) * intensity;
	_color.z = (_color.z / maxChannel) * intensity;

	if (_shadowCameraCube)
	{
		float reach = CalculateLightReach(_color, _falloff);
		_shadowCameraCube->SetFarZ(reach);
	}
}

CameraCubeBehaviour *PointLightBehaviour::GetShadowCameraCube() const
{
	return _shadowCameraCube;
}

bool PointLightBehaviour::ContainsPoint(const XMFLOAT3A &point)
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
bool PointLightBehaviour::IntersectsLightTile(const BoundingFrustum &tile)
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
bool PointLightBehaviour::IntersectsLightTile(const BoundingOrientedBox &tile)
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
