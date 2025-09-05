#include "stdafx.h"
#include "Behaviours/SpotLightBehaviour.h"
#include "Behaviours/BillboardMeshBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

SpotLightBehaviour::SpotLightBehaviour(CameraBehaviour *camera, XMFLOAT3 color, float falloff, float fogStrength, UINT updateFrequency) :
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
SpotLightBehaviour::SpotLightBehaviour(ProjectionInfo projInfo, XMFLOAT3 color, float falloff, float fogStrength, bool isOrtho, UINT updateFrequency) :
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
SpotLightBehaviour::~SpotLightBehaviour()
{
	if (!IsInitialized())
		return;

	if (!IsEnabled())
		return;

	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (!spotlights)
		return;

	if (!spotlights->UnregisterLight(this))
		DbgMsg("Failed to unregister spotlight!");
}

bool SpotLightBehaviour::Start()
{
	if (_name.empty())
		_name = "SpotLightBehaviour"; // For categorization in ImGui.

	if (!_shadowCamera)
	{
		_shadowCamera = new CameraBehaviour(_initialProjInfo, _ortho, true);

		if (!_shadowCamera->Initialize(GetEntity()))
		{
			ErrMsg("Failed to bind shadow camera to spotlight!");
			return false;
		}
		_shadowCamera->SetSerialization(false);
	}

	_shadowCamera->SetRendererInfo({ false, true });

	SpotLightCollection *spotlights = GetScene()->GetSpotlights();

	if (spotlights)
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

	auto billboardMeshBehaviour = new BillboardMeshBehaviour(mat, 0.0f, 0.0f, 0.5f, true, false, false, false, true);

	if (!billboardMeshBehaviour->Initialize(GetEntity()))
		Warn("Failed to Initialize billboard mesh behaviour!");
#endif

	QueueParallelUpdate();

	return true;
}

bool SpotLightBehaviour::ParallelUpdate(const TimeUtils &time, const Input &input)
{
	if (_updateTimer <= 0)
	{
		_updateTimer += _updateFrequency;
		_boundsDirty = true;
	}
	_updateTimer--;

	return true;
}

#ifdef USE_IMGUI
bool SpotLightBehaviour::RenderUI()
{
	if (ImGui::Button("Reset Color"))
		_color = { 1.0f, 1.0f, 1.0f };

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
	if (ImGui::InputFloat("Intensity", &colorStrength))
	{
		colorStrength = max(colorStrength, 0.001f);
		newStrength = true;
	}

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

	if (_shadowCamera)
	{
		float angle = _shadowCamera->GetFOV() * RAD_TO_DEG;
		if (ImGui::SliderFloat("Angle", &angle, 0.01f, 179.99f))
		{
			angle = max(min(angle, 179.99f), 0.01f);
			_shadowCamera->SetFOV(angle * DEG_TO_RAD);
		}
	}

	if (recalculateReach)
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

	return true;
}
#endif

void SpotLightBehaviour::OnEnable()
{
	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->RegisterLight(this))
		{
			ErrMsg("Failed to register spotlight!");
		}
	}

	_updateTimer = 1;
}
void SpotLightBehaviour::OnDisable()
{
	SpotLightCollection *spotlights = GetScene()->GetSpotlights();
	if (spotlights)
	{
		if (!spotlights->UnregisterLight(this))
		{
			ErrMsg("Failed to unregister spotlight!");
		}
	}
}

bool SpotLightBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	dx::XMFLOAT4X4 projectionMatrix = _shadowCamera->GetProjectionMatrix();
	float fovAngleY = 2.0f * std::atan(1.0f / projectionMatrix._22);
	float aspectRatio = projectionMatrix._22 / projectionMatrix._11;
	float nearPlane = projectionMatrix._43 / projectionMatrix._33;
	float farPlane = projectionMatrix._43 / (projectionMatrix._33 - 1);

	obj.AddMember("Falloff", _falloff, docAlloc);
	obj.AddMember("Fog Strength", _fogStrength, docAlloc);

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
bool SpotLightBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	float falloff		= obj["Falloff"].GetFloat();
	float fogStrength	= obj["Fog Strength"].GetFloat();

	dx::XMFLOAT3 color;
	SerializerUtils::DeserializeVec(color, obj["Color"]);

	const json::Value &projObj = obj["Projection"];
	float fov		= projObj["FOV"].GetFloat();
	float aspect	= projObj["Aspect"].GetFloat();
	float nearPlane = projObj["Near"].GetFloat();
	float farPlane	= projObj["Far"].GetFloat();

	SetLightBufferData(color, falloff, fogStrength);
	_initialProjInfo = { fov, aspect, nearPlane * -1, farPlane * -1 };

	return true;
}


void SpotLightBehaviour::SetUpdateTimer(UINT timer)
{
	_updateTimer = timer;
}
bool SpotLightBehaviour::DoUpdate() const
{
	return _updateTimer <= 0;
}
bool SpotLightBehaviour::UpdateBuffers()
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

SpotLightBufferData SpotLightBehaviour::GetLightBufferData()
{
	if (!DoUpdate())
		return _lastLightBufferData;

	Transform *transform = GetTransform();
	CameraBehaviour *cam = _shadowCamera;

	SpotLightBufferData &data = _lastLightBufferData;
	data.vpMatrix = cam->GetViewProjectionMatrix();
	data.position = transform->GetPosition(World);
	data.direction = transform->GetForward(World);
	data.color = _color;
	data.angle = cam->GetFOV();
	data.falloff = _falloff;
	data.orthographic = _shadowCamera->GetOrtho() ? 1 : -1;
	data.fogStrength = _fogStrength;

	return data;
}
void SpotLightBehaviour::SetLightBufferData(XMFLOAT3 color, float falloff, float fogStrength)
{
	_color = color;
	_falloff = falloff;
	_fogStrength = fogStrength;

	if (_shadowCamera)
	{
		CameraPlanes planes = _shadowCamera->GetPlanes();

		float reach = CalculateLightReach(color, falloff);

		if (planes.nearZ < planes.farZ)
			planes.farZ = reach;
		else
			planes.nearZ = reach;

		planes.nearZ = planes.nearZ < 0.05f ? 0.05f : planes.nearZ;

		_shadowCamera->SetPlanes(planes);
	}
}
void SpotLightBehaviour::SetIntensity(float intensity)
{
	float maxChannel = max(_color.x, max(_color.y, _color.z));
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

CameraBehaviour *SpotLightBehaviour::GetShadowCamera() const
{
	return _shadowCamera;
}

bool SpotLightBehaviour::ContainsPoint(const XMFLOAT3A &point)
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
bool SpotLightBehaviour::IntersectsLightTile(const BoundingFrustum &tile)
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
bool SpotLightBehaviour::IntersectsLightTile(const BoundingOrientedBox &tile)
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