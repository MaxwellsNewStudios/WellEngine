#include "stdafx.h"
#include "B_LightSpot.h"
#include "Game/Entity.h"
#include "Game/Scene/Scene.h"
#include "B_LightSpotSimple.h"
#include "../Mesh/B_MeshBillboard.h"
#include "Engine/Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

B_LightSpot::B_LightSpot(B_Camera *camera, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
	_shadowCamera(camera), _color(color), _falloff(falloff), _fogStrength(fogStrength), _updateFrequency(updateFrequency)
{
	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		_shadowCamera->SetPlanes(planes);
	}
}
B_LightSpot::B_LightSpot(ProjectionInfo projInfo, XMFLOAT3 color, float falloff, float fogStrength, bool isOrtho, UINT updateFrequency) :
	_shadowCamera(nullptr), _initialProjInfo(projInfo), _color(color), _falloff(falloff), _fogStrength(fogStrength), _ortho(isOrtho), _updateFrequency(updateFrequency)
{
	float reach = CalculateLightReach(_color, _falloff);

	if (_initialProjInfo.planes.nearZ < _initialProjInfo.planes.farZ)
		_initialProjInfo.planes.farZ = reach;
	else
		_initialProjInfo.planes.nearZ = reach;

	_initialProjInfo.planes.nearZ = _initialProjInfo.planes.nearZ < 0.05f ? 
		0.05f : 
		_initialProjInfo.planes.nearZ;
}
B_LightSpot::~B_LightSpot()
{
	if (!IsInitialized())
		return;

	if (_shadowCamera)
	{
		_shadowCamera->Destroy();
		_shadowCamera = nullptr;
	}

#ifdef DEBUG_BUILD
	if (!GetScene()->IsDestroyed() && !GetEntity()->IsRemoved())
		if (_billboardMeshBehaviour.IsValid())
			_billboardMeshBehaviour.Get()->Destroy();
#endif

	if (!IsEnabled())
		return;

	LightSpotCollection *spotlights = GetScene()->GetSpotlights();
	if (!spotlights)
		return;

	if (!spotlights->UnregisterLight(this))
		DbgMsg("Failed to unregister spotlight!");
}

bool B_LightSpot::Start()
{
	if (!_shadowCamera)
	{
		_shadowCamera = new B_Camera(_initialProjInfo, _ortho, true);

		if (!_shadowCamera->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind shadow camera to spotlight!");
			return false;
		}
		_shadowCamera->SetSerialization(false);
	}

	_shadowCamera->SetRendererInfo({ false, true });

	auto planes = _shadowCamera->GetPlanes();
	planes.farZ = CalculateLightReach(_color, _falloff);
	_shadowCamera->SetPlanes(planes);

	LightSpotCollection *spotlights = GetScene()->GetSpotlights();

	if (spotlights && IsEnabled())
	{
		if (!spotlights->RegisterLight(this))
		{
			ErrMsg("Failed to register spotlight!");
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

bool B_LightSpot::ParallelUpdate(const TimeUtils &time, const Input &input)
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
bool B_LightSpot::RenderUI()
{
	if (ImGui::Button("Swap with Non-Shadowcasting Variant"))
	{
		Entity *ent = GetEntity();

		B_LightSpotSimple *simpleLight = new B_LightSpotSimple(
			_color, _shadowCamera->GetFOV(), _falloff, _shadowCamera->GetOrtho(), _fogStrength
		);

		if (!simpleLight->Initialize(ent))
		{
			delete simpleLight;
			ErrMsg("Failed to initialize simple spotlight!");
			return false;
		}

		ent->ReorderBehaviour(simpleLight, ent->GetBehaviourIndex(this) + 1);
		simpleLight->SetUIOpen(true);

		Destroy();
		return true;
	}

	if (ImGui::Button("Reset Color"))
		_color = { 1.0f, 1.0f, 1.0f };

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
	if (ImGui::DragFloat("Intensity", &colorStrength, 0.01f, 0.1f))
	{
		colorStrength = MAX(colorStrength, 0.1f);
		newStrength = true;
	}
	ImGuiUtils::LockMouseOnActive();

	if (newColor || newStrength)
	{
		recalculateReach = true;
		float inputStr = MAX(color[0], MAX(color[1], color[2]));

		if (inputStr > 0.1f)
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

	if (_shadowCamera)
	{
		float angle = _shadowCamera->GetFOV() * RAD_TO_DEG;
		if (ImGui::SliderFloat("Angle", &angle, 0.01f, 179.99f))
		{
			angle = CLAMP(angle, 0.01f, 179.99f);
			_shadowCamera->SetFOV(angle * DEG_TO_RAD);
		}
	}

	if (recalculateReach && _shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		_shadowCamera->SetPlanes(planes);
	}

	UINT step = 1;
	UINT stepFast = 5;
	if (ImGui::InputScalar("Shadow Update Frequency", ImGuiDataType_U32, &_updateFrequency, &step, &stepFast))
		_updateFrequency = MAX(_updateFrequency, 1);

	ImGui::Separator();
	ImGui::Text("Light Reach: %.3f units", CalculateLightReach(_color, _falloff));

	return true;
}
#endif

void B_LightSpot::OnEnable()
{
	LightSpotCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->RegisterLight(this))
		{
			ErrMsg("Failed to register spotlight!");
		}
	}
}
void B_LightSpot::OnDisable()
{
	LightSpotCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->UnregisterLight(this))
		{
			ErrMsg("Failed to unregister spotlight!");
		}
	}
}

bool B_LightSpot::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Update Frequency", _updateFrequency, docAlloc);

	dx::XMFLOAT4X4 projectionMatrix = _shadowCamera->GetProjectionMatrix();
	float fovAngleY = 2.0f * std::atan(1.0f / projectionMatrix._22);
	float aspectRatio = projectionMatrix._22 / projectionMatrix._11;
	float nearPlane = projectionMatrix._43 / projectionMatrix._33;
	float farPlane = projectionMatrix._43 / (projectionMatrix._33 - 1);

	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);
	obj.AddMember("Shadow Strength", _shadowStrength, docAlloc);

	json::Value colorArr(json::kArrayType);
	colorArr.PushBack(_color.x, docAlloc);
	colorArr.PushBack(_color.y, docAlloc);
	colorArr.PushBack(_color.z, docAlloc);
	obj.AddMember("Color", colorArr, docAlloc);

	json::Value projObj(json::kObjectType);
	projObj.AddMember("FOV", fovAngleY, docAlloc);
	projObj.AddMember("Aspect", aspectRatio, docAlloc);
	projObj.AddMember("Near", nearPlane, docAlloc);
	projObj.AddMember("Far", farPlane, docAlloc);
	obj.AddMember("Projection", projObj, docAlloc);

	return true;
}
bool B_LightSpot::Deserialize(const json::Value &obj, Scene *scene)
{
	if (obj.HasMember("Update Frequency"))
		_updateFrequency = obj["Update Frequency"].GetUint();

	float
		falloff = 1.0f,
		fogStrength = 1.0f,
		shadowStrength = 1.0f;

	if (obj.HasMember("Falloff"))
		falloff = obj["Falloff"].GetFloat();
	if (obj.HasMember("Fog Strength"))
		fogStrength = obj["Fog Strength"].GetFloat();
	if (obj.HasMember("Shadow Strength"))
		shadowStrength = obj["Shadow Strength"].GetFloat();

	dx::XMFLOAT3 color;
	SerializerUtils::DeserializeVec(color, obj["Color"]);

	const json::Value &projObj = obj["Projection"];
	float fov		= projObj["FOV"].GetFloat();
	float aspect	= projObj["Aspect"].GetFloat();
	float nearPlane = projObj["Near"].GetFloat();
	float farPlane	= projObj["Far"].GetFloat();

	SetLightBufferData(color, falloff, fogStrength, shadowStrength);
	_initialProjInfo = { fov, aspect, nearPlane * -1, farPlane * -1 };

	return true;
}

UINT B_LightSpot::GetUpdateFrequency() const
{
	return _updateFrequency;
}
void B_LightSpot::SetUpdateFrequency(UINT frequency)
{
	_updateFrequency = MAX(1, frequency);
}
int B_LightSpot::GetUpdateTimer() const
{
	return _updateTimer;
}
void B_LightSpot::SetUpdateTimer(int timer)
{
	_updateTimer = timer;
}

void B_LightSpot::ForceUpdate()
{
	_updateShadows = true;
	_boundsDirty = true;
}
void B_LightSpot::MarkUpdated()
{
	_updateShadows = false;
}
bool B_LightSpot::DoUpdate() const
{
	return _updateShadows;
}
bool B_LightSpot::UpdateBuffers()
{
	if (!DoUpdate())
		return true;

	if (!_shadowCamera->UpdateBuffers())
	{
		ErrMsg("Failed to update shadow camera buffers!");
		return false;
	}

	return true;
}

SpotLightBufferData B_LightSpot::GetLightBufferData()
{
	if (!DoUpdate())
		return _lastLightBufferData;

	Transform *transform = GetTransform();
	B_Camera *cam = _shadowCamera;

	SpotLightBufferData &data = _lastLightBufferData;
	data.vpMatrix = cam->GetViewProjectionMatrix();
	data.position = transform->GetPosition(World);
	data.direction = transform->GetForward(World);
	data.color = _color;
	data.angle = cam->GetFOV();
	data.falloff = _falloff;
	data.orthographic = _shadowCamera->GetOrtho() ? 1 : -1;
	data.fogStrength = _fogStrength;
	data.shadowStrength = _shadowStrength;

	return data;
}
void B_LightSpot::SetLightBufferData(XMFLOAT3 color, float falloff, float fogStrength, float shadowStrength)
{
	_color = color;
	_falloff = falloff;
	_fogStrength = fogStrength;
	_shadowStrength = shadowStrength;

	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		planes.nearZ = planes.nearZ < 0.05f ? 0.05f : planes.nearZ;

		_shadowCamera->SetPlanes(planes);
	}
}

void B_LightSpot::SetIntensity(float intensity)
{
	float maxChannel = MAX(_color.x, MAX(_color.y, _color.z));
	_color.x = (_color.x / maxChannel) * intensity;
	_color.y = (_color.y / maxChannel) * intensity;
	_color.z = (_color.z / maxChannel) * intensity;

	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		_shadowCamera->SetPlanes(planes);
	}
}
void B_LightSpot::SetColor(dx::XMFLOAT3 color)
{
	_color = color;

	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		planes.nearZ = planes.nearZ < 0.05f ? 0.05f : planes.nearZ;

		_shadowCamera->SetPlanes(planes);
	}
}
void B_LightSpot::SetFalloff(float falloff)
{
	_falloff = falloff;

	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(_color, _falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		planes.nearZ = planes.nearZ < 0.05f ? 0.05f : planes.nearZ;

		_shadowCamera->SetPlanes(planes);
	}
}
void B_LightSpot::SetFogStrength(float fogStrength)
{
	_fogStrength = fogStrength;
}
void B_LightSpot::SetShadowStrength(float shadowStrength)
{
	_shadowStrength = shadowStrength;
}

B_Camera *B_LightSpot::GetShadowCamera() const
{
	return _shadowCamera;
}

bool B_LightSpot::ContainsPoint(const XMFLOAT3A &point)
{
	if (DoUpdate())
	{
		bool failed = false;
#pragma omp critical
		{
			if (_boundsDirty)
			{
				_boundsDirty = false;
				if (!_shadowCamera->StoreBounds(_transformedBounds, false))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					failed = true;
				}
			}
		}

		if (failed)
			return false;
	}

	return _transformedBounds.Contains(Load(point));
}
bool B_LightSpot::IntersectsLightTile(const BoundingFrustum &tile)
{
	if (DoUpdate())
	{
		bool failed = false;
#pragma omp critical
		{
			if (_boundsDirty)
			{
				_boundsDirty = false;
				if (!_shadowCamera->StoreBounds(_transformedBounds, false))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					failed = true;
				}
			}
		}

		if (failed)
			return false;
	}

	dx::BoundingFrustum noNearFrustum(_transformedBounds);
	noNearFrustum.Near = LIGHT_CULLING_NEAR_PLANE; // Ignore near plane for light tile intersection

	return tile.Intersects(noNearFrustum);
}
bool B_LightSpot::IntersectsLightTile(const BoundingOrientedBox &tile)
{
	if (DoUpdate())
	{
		bool failed = false;
#pragma omp critical
		{
			if (_boundsDirty)
			{
				_boundsDirty = false;
				if (!_shadowCamera->StoreBounds(_transformedBounds, false))
				{
					ErrMsg("Failed to store spotlight camera frustum!");
					failed = true;
				}
			}
		}

		if (failed)
			return false;
	}

	dx::BoundingFrustum noNearFrustum(_transformedBounds);
	noNearFrustum.Near = LIGHT_CULLING_NEAR_PLANE; // Ignore near plane for light tile intersection

	return tile.Intersects(noNearFrustum);
}