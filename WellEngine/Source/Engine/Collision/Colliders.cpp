#include "stdafx.h"
#include "Collision/Colliders.h"

#include "Entity.h"
#include "Content/Content.h"
#include "Scenes/Scene.h"
#include "Debug/DebugDrawer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;
using namespace Collisions;

Collisions::Collider::Collider(ColliderTypes type, const std::string &name, ColliderTags tag):
	colliderType(type), 
#ifdef DEBUG_BUILD
	debug(false), _debugMeshName(name), 
#endif
	_min(0, 0, 0), _max(0, 0, 0), 
	_isDirty(true), _tagFlag((uint8_t)tag), _active(true)
#ifdef DEBUG_BUILD
	, haveDebugEnt(false)
#endif
{

}

Collisions::Collider::~Collider()
{
#ifdef DEBUG_BUILD
	delete _debugEnt;
#endif
}

#ifdef USE_IMGUI
bool Collisions::Collider::RenderUI()
{
	return true;
}
#endif

static Scene *_scene;

#ifdef DEBUG_BUILD
bool Collisions::Collider::SetDebugEnt(Scene* scene, const Content* content)
{
	/*
	const UINT
		meshID = content->GetMeshID(_debugMeshName),
		textureID = content->GetTextureID("Green"),
		normalID = CONTENT_NULL,
		specularID = CONTENT_NULL,
		reflectiveID = CONTENT_NULL,
		ambientID = content->GetTextureID("Green"),
		heightID = CONTENT_NULL,
		samplerID = CONTENT_NULL;

	if (_debugEnt)
		delete _debugEnt;

	_debugEnt = new Object(-1, BoundingOrientedBox({ 0, 0, 0 }, { 1, 1, 1 }, { 0, 0, 0, 1 }));
	Object *obj = reinterpret_cast<Object *>(_debugEnt);

	if (!obj->Initialize(device, nullptr, "Maxwell Marker", meshID, textureID, normalID, specularID, reflectiveID, ambientID, heightID, samplerID))
	{
		ErrMsg("Failed to initialize Maxwell marker object!");
	}
	*/

	_scene = scene;

	UINT meshID = content->GetMeshID(_debugMeshName);
	Material mat;
	mat.textureID = content->GetTextureID("Green");
	mat.ambientID = content->GetTextureID("Green");
	mat.vsID = content->GetShaderID("VS_Geometry");

	if (!scene->CreateGlobalMeshEntity(&_debugEnt, "Collider Wireframe", meshID, mat))
	{
		ErrMsg("Failed to create collider debug object!");
		return false;
	}
	//debug = true;
	haveDebugEnt = true;

	return true;
}

bool Collisions::Collider::RenderDebug(Scene* scene, const RenderQueuer &queuer, const RendererInfo &renderInfo)
{
	if (debug)
	{
		DebugDrawer &debugDraw = DebugDrawer::Instance();
		debugDraw.DrawLineThreadSafe(_min, _max, 0.01f, { 1, 0, 1, 1 });

		if (_debugEnt)
			return _debugEnt->InitialRender(queuer, renderInfo);
	}
	return true;
}
#endif

void Collisions::Collider::AddIntersectionCallback(std::function<void(const CollisionData &)> callback)
{
	_intersectionCallbacks.emplace_back(callback);
}

void Collisions::Collider::AddOnCollisionEnterCallback(std::function<void(const CollisionData&)> callback)
{
	_onCollisionEnterCallbacks.emplace_back(callback);
}

void Collisions::Collider::AddOnCollisionExitCallback(std::function<void(const CollisionData&)> callback)
{
	_onCollisionExitCallbacks.emplace_back(callback);
}

void Collisions::Collider::Intersection(const CollisionData &data) const
{
	for (auto &callback : _intersectionCallbacks)
		callback(data);
}

void Collisions::Collider::OnCollisionEnter(const CollisionData &data) const
{
	for (auto &callback : _onCollisionEnterCallbacks)
		callback(data);
}

void Collisions::Collider::OnCollisionExit(const CollisionData &data) const
{
	for (auto &callback : _onCollisionExitCallbacks)
		callback(data);
}

dx::XMFLOAT3 Collisions::Collider::GetMin() const
{
	return _min;
}

dx::XMFLOAT3 Collisions::Collider::GetMax() const
{
	return _max;
}

void Collisions::Collider::SetTag(ColliderTags tag)
{
	_tagFlag |= (int)tag;
}

void Collisions::Collider::RemoveTag(ColliderTags tag)
{
	_tagFlag &= ~(int)tag;
}

bool Collisions::Collider::HasTag(ColliderTags tag) const
{
	return (_tagFlag & (int)tag) == (int)tag;
}

uint8_t Collisions::Collider::GetTag() const
{
	return _tagFlag;
}

void Collisions::Collider::SetActive(bool status)
{
	_active = status;
}

bool Collisions::Collider::GetActive() const
{
	return _active;
}

bool Collisions::Collider::GetDirty() const
{
	return _isDirty;
}

bool Collisions::Collider::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}

bool Collisions::Collider::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}


/********************************************
*			 RAY COLLIDER					*
*********************************************/

Collisions::Ray::Ray(const Ray &other):
	Ray(other.origin, other.dir, other.length, (ColliderTags)other._tagFlag)
{
}

Collisions::Ray::Ray(const dx::XMFLOAT3 &o, const dx::XMFLOAT3 &d, float l, ColliderTags tag):
	Collider(RAY_COLLIDER, "WireframeCube", tag), origin(o), dir(d), length(l)
{
}

#ifdef USE_IMGUI
bool Collisions::Ray::RenderUI()
{
	ImGui::Text("Ray Collider");
	ImGui::Separator();

	// TODO: Fix rotation

	XMVECTOR o = XMLoadFloat3(&origin),
			 d = XMLoadFloat3(&dir);

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Origin", ImGuiDataType_Float, o.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalarN("Direction", ImGuiDataType_Float, d.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalar("Length", ImGuiDataType_Float, &length, speed, minValues, maxValues, "%.3f");

	ImGui::Separator();

	Store(&origin, o);
	Store(&dir, XMVector3Normalize(d));

	return true;

}
#endif

#ifdef DEBUG_BUILD
bool Collisions::Ray::RenderDebug(Scene *scene, const RenderQueuer &queuer, const RendererInfo &renderInfo)
{
	if (!debug)
		return true;

	DebugDrawer &debugDraw = DebugDrawer::Instance();
	XMFLOAT3 max;
	Store(&max, XMVectorAdd(Load(origin), XMVectorScale(Load(dir), length)));
	debugDraw.DrawLineThreadSafe(origin, max, 0.01f, { 0, 1, 0, 1 });

	return true;
}
void Collisions::Ray::DebugRayRender(Scene *scene, XMFLOAT4 color) const
{
	DebugDrawer &debugDraw = DebugDrawer::Instance();
	XMFLOAT3 max;
	Store(&max, XMVectorAdd(Load(origin), XMVectorScale(Load(dir), length)));
	debugDraw.DrawLineThreadSafe(origin, max, 0.01f, color);
}
#endif

void Collisions::Ray::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != RAY_COLLIDER)
		return;

	Ray *transformedRay = dynamic_cast<Ray *>(transformed);

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Transform center position
	Store(&transformedRay->origin, XMVector3Transform(Load(origin), worldMatrix));

	XMVECTOR axisVec = XMLoadFloat3(&dir);
	axisVec = XMVector3Normalize(XMVector3TransformNormal(axisVec, worldMatrix));
	Store(&transformedRay->dir, axisVec);

	// Scale Half-Lenghts
	XMFLOAT3 scale {
		XMVectorGetX(XMVector3Length(worldMatrix.r[0])),
		XMVectorGetX(XMVector3Length(worldMatrix.r[1])),
		XMVectorGetX(XMVector3Length(worldMatrix.r[2]))
	};

	// TODO: scale this length
	transformedRay->length = length;
	
	XMVECTOR startPoint = Load(transformedRay->origin),
			 endPoint = XMVectorAdd(startPoint, XMVectorScale(axisVec, transformedRay->length));

	Store(&transformedRay->_min, XMVectorMin(startPoint, endPoint));
	Store(&transformedRay->_max, XMVectorMax(startPoint, endPoint));

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::Ray::TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input)
{
	return true;
}
#endif


/********************************************
*		   SPHERE COLLIDER					*
*********************************************/

Collisions::Sphere::Sphere(const Sphere &other):
	Sphere(other.center, other.radius, (ColliderTags)other._tagFlag)
{
}

Collisions::Sphere::Sphere(const dx::XMFLOAT3& c, float r, ColliderTags tag):
	Collider(SPHERE_COLLIDER, "WireframeSphere", tag), center(c), radius(r)
{
}

Collisions::Sphere::Sphere(const dx::BoundingSphere& sphere, ColliderTags tag):
	Collider(SPHERE_COLLIDER, "WireframeSphere", tag), center(sphere.Center), radius(sphere.Radius)
{
}

#ifdef USE_IMGUI
bool Collisions::Sphere::RenderUI()
{
	ImGui::Text("Sphere Collider");
	ImGui::Separator();

	XMVECTOR c = XMLoadFloat3(&center);

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Center", ImGuiDataType_Float, c.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalar("Radius", ImGuiDataType_Float, &radius, speed, &minValues[0], &maxValues[0], "%.3f");

	ImGui::Separator();

	Store(&center, c);

	return true;
}
#endif

void Collisions::Sphere::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != SPHERE_COLLIDER)
		return;

	Sphere *transformedSphere = dynamic_cast<Sphere *>(transformed);

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Translation
	Store(&transformedSphere->center, XMVector3Transform(XMLoadFloat3(&center), worldMatrix));

	// Scaling
	XMFLOAT3 scale{
		XMVectorGetX(XMVector3Length(worldMatrix.r[0])) * radius,
		XMVectorGetX(XMVector3Length(worldMatrix.r[1])) * radius,
		XMVectorGetX(XMVector3Length(worldMatrix.r[2])) * radius 
	};

	transformedSphere->radius = fabsf(fmaxf(scale.x, fmaxf(scale.y, scale.z)));

	// Update Min and Max
	transformedSphere->_min = { transformedSphere->center.x - transformedSphere->radius,
							    transformedSphere->center.y - transformedSphere->radius,
							    transformedSphere->center.z - transformedSphere->radius };

	transformedSphere->_max = { transformedSphere->center.x + transformedSphere->radius,
							    transformedSphere->center.y + transformedSphere->radius,
							    transformedSphere->center.z + transformedSphere->radius };

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::Sphere::TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input)
{
	if (!debug || !_debugEnt)
		return true;

	auto *dbgT = _debugEnt->GetTransform();

	dbgT->SetPosition(To4(center));

	dbgT->SetScale(XMFLOAT4A(radius, radius, radius, 0));

	if (!dbgT->UpdateConstantBuffer(context))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}

	if (!_debugEnt->InitialUpdate(time, input))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}
	return true;
}
#endif


/********************************************
*		   CAPSULE COLLIDER					*
*********************************************/

Collisions::Capsule::Capsule(const Capsule &other):
	Capsule(other.center, other.upDir, other.radius, other.height, (ColliderTags)other._tagFlag)
{
}

Collisions::Capsule::Capsule(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &u, float r, float h, ColliderTags tag):
	Collider(CAPSULE_COLLIDER, "WireframeCapsule", tag), center(c), upDir(u), radius(r), height(h)
{
	if (radius * 2 > height)
		radius = height / 2;
}

#ifdef USE_IMGUI
bool Collisions::Capsule::RenderUI()
{
	ImGui::Text("Capsule Collider");
	ImGui::Separator();

	XMVECTOR c = XMLoadFloat3(&center);
	XMVECTOR u = XMLoadFloat3(&upDir);

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Center", ImGuiDataType_Float, c.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalarN("Up Direction", ImGuiDataType_Float, u.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalar("Height", ImGuiDataType_Float, &height, speed, 0, &maxValues[0], "%.3f");
	_isDirty |= ImGui::DragScalar("Radius", ImGuiDataType_Float, &radius, speed, 0, &maxValues[0], "%.3f");

	ImGui::Separator();

	u = XMVector3Normalize(u);
	if (XMVectorGetX(XMVector3LengthSq(u)) == 0)
		u = XMVectorSet(0, 1, 0, 0); // Set default up
	Store(&upDir, u);
	Store(&center, c);

	return true;
}
#endif

void Collisions::Capsule::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != CAPSULE_COLLIDER)
		return;

	Capsule *transformedCapsule = dynamic_cast<Capsule *>(transformed);

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Translation
	Store(&transformedCapsule->center, XMVector3Transform(XMLoadFloat3(&center), worldMatrix));

	// TODO: Transform rotation of up vector
    XMVECTOR axisVec = XMLoadFloat3(&upDir);
	axisVec = XMVector3TransformNormal(axisVec, worldMatrix);
	Store(&transformedCapsule->upDir, XMVector3Normalize(axisVec));


	// Scaling
	XMFLOAT3 scale{
		XMVectorGetX(XMVector3Length(worldMatrix.r[0])) * radius,
		XMVectorGetX(XMVector3Length(worldMatrix.r[1])) * height,
		XMVectorGetX(XMVector3Length(worldMatrix.r[2])) * radius
	};

	transformedCapsule->radius = fabsf(fmaxf(scale.x, scale.z));
	transformedCapsule->height = fabsf(scale.y);

	// Update Min and Max
	XMVECTOR normal1 = XMVector3Normalize(XMLoadFloat3(&transformedCapsule->upDir)),
			 lineEndOffset1 = XMVectorScale(normal1, (transformedCapsule->height / 2) - transformedCapsule->radius),
			 A1 = XMVectorAdd(XMLoadFloat3(&transformedCapsule->center), lineEndOffset1),
			 B1 = XMVectorSubtract(XMLoadFloat3(&transformedCapsule->center), lineEndOffset1);

	XMVECTOR p1 = XMVectorAdd(A1, XMVectorScale(XMVectorSet(1, 1, 1, 0), transformedCapsule->radius));
	XMVECTOR p2 = XMVectorSubtract(B1, XMVectorScale(XMVectorSet(-1, -1, -1, 0), transformedCapsule->radius));
	XMVECTOR p3 = XMVectorAdd(A1, XMVectorScale(XMVectorSet(-1, -1, -1, 0), transformedCapsule->radius));
	XMVECTOR p4 = XMVectorSubtract(B1, XMVectorScale(XMVectorSet(1, 1, 1, 0), transformedCapsule->radius));

	Store(&transformedCapsule->_min, XMVectorMin(XMVectorMin(XMVectorMin(p1, p2), p3), p4));
	Store(&transformedCapsule->_max, XMVectorMax(XMVectorMax(XMVectorMax(p1, p2), p3), p4));

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::Capsule::TransformDebug(ID3D11DeviceContext *context, TimeUtils &time, const Input &input)
{
	if (!_debugEnt || !debug)
		return true;

	// Translation
	auto dbgT = _debugEnt->GetTransform();
	dbgT->SetPosition(To4(center));

	// Rotation
	XMVECTOR defaultUp = XMVectorSet(0, 1, 0, 0);
	XMVECTOR transformedUp = XMVector3Normalize(XMLoadFloat3(&upDir));

	// Compute rotation axis
	XMVECTOR rotationAxis = XMVector3Cross(defaultUp, transformedUp);
	float dot = XMVectorGetX(XMVector3Dot(defaultUp, transformedUp));

	// Handle parallel vectors 
	if (XMVector3Equal(defaultUp, transformedUp)) 
	{
		dbgT->SetRotation({ 0, 0, 0, 1 });
	}
	else if (dot <= -1.0f)
	{
		rotationAxis = XMVectorSet(0, 1, 0, 0);
		XMVECTOR rotationQuat = XMQuaternionRotationAxis(rotationAxis, XM_PI);
		XMFLOAT4A quaternion{};
		Store(quaternion, rotationQuat);
		dbgT->SetRotation(quaternion);
	}
	else
	{
		// Compute rotation angle
		float angle = acosf(dot); // Angle between defaultUp and transformedUp

		// Compute quaternion from axis-angle representation
		XMVECTOR rotationQuat = XMQuaternionRotationAxis(rotationAxis, angle);

		// Store and apply rotation
		XMFLOAT4A quaternion{};
		Store(quaternion, rotationQuat);
		dbgT->SetRotation(quaternion);
	}

	// Scaling
	dbgT->SetScale(XMFLOAT3A(radius, height / 2.0f, radius));

	if (!dbgT->UpdateConstantBuffer(context))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}

	if (!_debugEnt->InitialUpdate(time, input))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}
	return true;
}
#endif


/********************************************
*		      OBB COLLIDER					*
*********************************************/

Collisions::OBB::OBB(const OBB &other):
	OBB(other.center, other.halfLength, other.axes, (ColliderTags)other._tagFlag)
{
}

Collisions::OBB::OBB(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const dx::XMFLOAT3 a[3], ColliderTags tag):
    Collider(OBB_COLLIDER, "WireframeCube", tag),	center(c), halfLength(hl)
{
	for (int i = 0; i < 3; i++)
		axes[i] = a[i];
}

Collisions::OBB::OBB(const BoundingOrientedBox &obb, ColliderTags tag):
	Collider(OBB_COLLIDER, "WireframeCube", tag), center(obb.Center), halfLength(obb.Extents)
{
	XMVECTOR orientation = XMLoadFloat4(&obb.Orientation);

	Store(&axes[0], XMVector3Rotate({1, 0, 0}, orientation));
	Store(&axes[1], XMVector3Rotate({ 0, 1, 0 }, orientation));
	Store(&axes[2], XMVector3Rotate({0, 0, 1}, orientation));
}

Collisions::OBB::OBB(const AABB &aabb, ColliderTags tag):
	Collider(OBB_COLLIDER, "WireframeCube", tag), center(aabb.center), halfLength(aabb.halfLength)
{
	axes[0] = { 1, 0, 0 };
	axes[1] = { 0, 1, 0 };
	axes[2] = { 0, 0, 1 };

	_min = { center.x - halfLength.x, center.y - halfLength.y, center.z - halfLength.z };
	_max = { center.x + halfLength.x, center.y + halfLength.y, center.z + halfLength.z };
}

#ifdef USE_IMGUI
bool Collisions::OBB::RenderUI()
{
	ImGui::Text("OBB Collider");
	ImGui::Separator();

	// TODO: Fix rotation

	XMVECTOR c = XMLoadFloat3(&center),
			 hl = XMLoadFloat3(&halfLength),
			 a[3] = { XMLoadFloat3(&axes[0]),
					  XMLoadFloat3(&axes[1]),
				      XMLoadFloat3(&axes[2]) };

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Center", ImGuiDataType_Float, c.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalarN("Half Lengths", ImGuiDataType_Float, hl.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	ImGui::Text("Axes: ");
	_isDirty |= ImGui::DragScalarN("x-axes", ImGuiDataType_Float, a[0].m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalarN("y-axes", ImGuiDataType_Float, a[1].m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::DragScalarN("z-axes", ImGuiDataType_Float, a[2].m128_f32, 3, speed, minValues, maxValues, "%.3f");

	ImGui::Separator();

	Store(&center, c);
	Store(&halfLength, hl);
	for (int i = 0; i < 3; i++)
	{
		a[i] = XMVector3Normalize(a[i]);
		Store(&axes[i], a[i]);
	}

	return true;
}
#endif

void Collisions::OBB::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != OBB_COLLIDER)
		return;

	OBB *transformedOBB = dynamic_cast<OBB *>(transformed);

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Transform center position
	Store(&transformedOBB->center, XMVector3Transform(XMLoadFloat3(&center), worldMatrix));

	// Rotate axes
    for (int i = 0; i < 3; i++)
    {
        XMVECTOR axisVec = XMLoadFloat3(&axes[i]);
        axisVec = XMVector3TransformNormal(axisVec, worldMatrix);
        Store(&transformedOBB->axes[i], XMVector3Normalize(axisVec));
    }

	// Scale Half-Lenghts
	XMFLOAT3 scale{
		XMVectorGetX(XMVector3Length(worldMatrix.r[0])),
		XMVectorGetX(XMVector3Length(worldMatrix.r[1])),
		XMVectorGetX(XMVector3Length(worldMatrix.r[2]))
	};

    Store(&transformedOBB->halfLength, XMVectorAbs(XMVectorMultiply(XMLoadFloat3(&halfLength), XMLoadFloat3(&scale))));

	// Update min max
	// TODO: Verify this part
    XMVECTOR corners[8] = {
        XMVectorSet(-transformedOBB->halfLength.x, -transformedOBB->halfLength.y, -transformedOBB->halfLength.z, 0),
        XMVectorSet( transformedOBB->halfLength.x, -transformedOBB->halfLength.y, -transformedOBB->halfLength.z, 0),
        XMVectorSet(-transformedOBB->halfLength.x,  transformedOBB->halfLength.y, -transformedOBB->halfLength.z, 0),
        XMVectorSet( transformedOBB->halfLength.x,  transformedOBB->halfLength.y, -transformedOBB->halfLength.z, 0),
        XMVectorSet(-transformedOBB->halfLength.x, -transformedOBB->halfLength.y,  transformedOBB->halfLength.z, 0),
        XMVectorSet( transformedOBB->halfLength.x, -transformedOBB->halfLength.y,  transformedOBB->halfLength.z, 0),
        XMVectorSet(-transformedOBB->halfLength.x,  transformedOBB->halfLength.y,  transformedOBB->halfLength.z, 0),
        XMVectorSet( transformedOBB->halfLength.x,  transformedOBB->halfLength.y,  transformedOBB->halfLength.z, 0)
    };
	
    XMVECTOR minPoint = XMVectorSet(INFINITY, INFINITY, INFINITY, 0);
    XMVECTOR maxPoint = XMVectorSet(-INFINITY, -INFINITY, -INFINITY, 0);

    for (int i = 0; i < 8; ++i)
    {
		XMVECTOR ex = XMVectorScale(XMLoadFloat3(&transformedOBB->axes[0]), corners[i].m128_f32[0]);
		XMVECTOR ey = XMVectorScale(XMLoadFloat3(&transformedOBB->axes[1]), corners[i].m128_f32[1]);
		XMVECTOR ez = XMVectorScale(XMLoadFloat3(&transformedOBB->axes[2]), corners[i].m128_f32[2]);

		XMVECTOR sum = XMVectorAdd(XMVectorAdd(ex, ey), ez);

		minPoint = XMVectorMin(minPoint, sum);
		maxPoint = XMVectorMax(maxPoint, sum);
	}

	minPoint = XMVectorAdd(minPoint, XMLoadFloat3(&transformedOBB->center));
	maxPoint = XMVectorAdd(maxPoint, XMLoadFloat3(&transformedOBB->center));

	Store(&transformedOBB->_min, minPoint);
	Store(&transformedOBB->_max, maxPoint);

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::OBB::TransformDebug(ID3D11DeviceContext *context, TimeUtils &time, const Input &input)
{
	if (!_debugEnt || !debug)
		return true;

	auto dbgT = _debugEnt->GetTransform();
	dbgT->SetPosition(To4(center));

	XMFLOAT4A rot;
	Store(rot, XMQuaternionRotationMatrix(XMMatrixSet(axes[0].x, axes[0].y, axes[0].z, 0,
													  axes[1].x, axes[1].y, axes[1].z, 0,
													  axes[2].x, axes[2].y, axes[2].z, 0,
													  0,		 0,			0,		   1)));

	dbgT->SetRotation(rot);

	dbgT->SetScale(XMFLOAT4A(halfLength.x,
							 halfLength.y, 
							 halfLength.z, 
							 0));

	if (!dbgT->UpdateConstantBuffer(context))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}

	if (!_debugEnt->InitialUpdate(time, input))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}
	return true;
}
#endif


/********************************************
*		      AABB COLLIDER					*
*********************************************/

Collisions::AABB::AABB(const AABB &other):
	AABB(other.center, other.halfLength, (ColliderTags)other._tagFlag)
{
}

Collisions::AABB::AABB(const dx::XMFLOAT3& c, const dx::XMFLOAT3& hl, ColliderTags tag):
	Collider(AABB_COLLIDER, "WireframeCube", tag), center(c), halfLength(hl)
{
}

Collisions::AABB::AABB(const dx::BoundingBox& aabb, ColliderTags tag):
	Collider(AABB_COLLIDER, "WireframeCube", tag), center(aabb.Center), halfLength(aabb.Extents)
{
}

Collisions::AABB::AABB(const dx::BoundingOrientedBox &obb, ColliderTags tag):
	Collider(AABB_COLLIDER, "WireframeCube", tag), center(obb.Center), halfLength(obb.Extents)
{
}

#ifdef USE_IMGUI
bool Collisions::AABB::RenderUI()
{
	ImGui::Text("AABB Collider");
	ImGui::Separator();

	XMVECTOR c = XMLoadFloat3(&center),
			 hl = XMLoadFloat3(&halfLength);

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Center", ImGuiDataType_Float, c.m128_f32, 3, speed, minValues, maxValues, "%.3f");

	// TODO: Fix half length scale in x-z axes
	_isDirty |= ImGui::DragScalarN("Half Lengths", ImGuiDataType_Float, hl.m128_f32, 3, speed, minValues, maxValues, "%.3f");

	ImGui::Separator();

	Store(&center, c);
	Store(&halfLength, hl);

	return true;
}
#endif

void Collisions::AABB::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != AABB_COLLIDER)
		return;

	AABB *transformedAABB = dynamic_cast<AABB *>(transformed);

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Translation
	Store(&transformedAABB->center, XMVector3Transform(XMLoadFloat3(&center), worldMatrix));

	// Rotation
	XMVECTOR maxAxis = XMVectorSet(0, 0, 0, 0);
	XMVECTOR hl = XMLoadFloat3(&halfLength);
	XMVECTOR tempAxes[3] = { XMVectorSet(1, 0, 0, 0), 
							 XMVectorSet(0, 1, 0, 0), 
							 XMVectorSet(0, 0, 1, 0) };

    for (int i = 0; i < 3; i++)
    {
        XMVECTOR axisVec = tempAxes[i];
        axisVec = XMVector3Normalize(XMVector3TransformNormal(axisVec, worldMatrix));
		XMVECTOR temp = XMVectorSet(0, 0, 0, 0);
		temp = XMVectorMax(temp, XMVectorScale(axisVec,		 hl.m128_f32[i] * XMVectorGetX(XMVector3Length(worldMatrix.r[i]))));
		temp = XMVectorMax(temp, XMVectorScale(axisVec, -1 * hl.m128_f32[i] * XMVectorGetX(XMVector3Length(worldMatrix.r[i]))));
		maxAxis = XMVectorAdd(maxAxis, temp);
    }

	Store(&transformedAABB->halfLength, maxAxis);

	// Update min max
	transformedAABB->_min = { transformedAABB->center.x - transformedAABB->halfLength.x,
							  transformedAABB->center.y - transformedAABB->halfLength.y,
							  transformedAABB->center.z - transformedAABB->halfLength.z };

	transformedAABB->_max = { transformedAABB->center.x + transformedAABB->halfLength.x,
							  transformedAABB->center.y + transformedAABB->halfLength.y,
							  transformedAABB->center.z + transformedAABB->halfLength.z };

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::AABB::TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input)
{
	if (!_debugEnt || !debug)
		return true;

	auto dbgT = _debugEnt->GetTransform();
	dbgT->SetPosition(To4(center));

	dbgT->SetScale(To4(halfLength));

	if (!dbgT->UpdateConstantBuffer(context))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}

	if (!_debugEnt->InitialUpdate(time, input))
	{
		ErrMsg("Failed to set world matrix buffer!");
		return false;
	}
	return true;
}
#endif


/********************************************
*		      Terrain COLLIDER				*
*********************************************/

Collisions::Terrain::Terrain(const Terrain &other):
	Collider(TERRAIN_COLLIDER, "WireframeCube", (ColliderTags)other._tagFlag)
{
	_minIndex = other._minIndex;
	_maxIndex = other._maxIndex;
	_heightScale = other._heightScale;

	_invertHeight = other._invertHeight;
	_isWallCollider = other._isWallCollider;

	tileSize = other.tileSize;

	center = other.center;
	halfLength = other.halfLength;
	map = other.map;

	if (other._isWallCollider)
		walls = std::vector<bool>(other.walls);
	else
		heightValues = std::vector<float>(other.heightValues);
}

Collisions::Terrain::Terrain(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const HeightMap *heightMap, ColliderTags tag, bool invertHeight, bool isWallCollider):
	Collider(TERRAIN_COLLIDER, "WireframeCube", tag)
{
	_invertHeight = invertHeight;
	_isWallCollider = isWallCollider;
	Init(c, hl, heightMap->GetHeightValues(), heightMap->GetWidth(), heightMap->GetHeight(), tag);
	map = heightMap->GetMap();
	_debugFlip = false;
}

Collisions::Terrain::Terrain(const dx::BoundingBox &aabb, const HeightMap *heightMap, ColliderTags tag, bool invertHeight, bool isWallCollider):
	Terrain(aabb.Center, aabb.Extents, heightMap, tag, invertHeight, isWallCollider)
{
	map = heightMap->GetMap();
	heightMap->GetMap();
}

void Collisions::Terrain::Init(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const std::vector<float> &heightMap, UINT width, UINT height, ColliderTags tag)
{
	tileSize = { (int)width, (int)height };
	center = c;
	halfLength = hl; 
	_minIndex = 0;
	_maxIndex = 0;
	_heightScale = 1;
#ifdef DEBUG_BUILD
	debug = true;
#endif

	float minHeight = INFINITY;
	float maxHeight = -INFINITY;

	if (_isWallCollider)
		walls.resize(static_cast<std::vector<bool, std::allocator<bool>>::size_type>(tileSize.x) * tileSize.y);
	else
		heightValues.resize(static_cast<std::vector<float, std::allocator<float>>::size_type>(tileSize.x) * tileSize.y);

	for (UINT x = 0; x < static_cast<UINT>(tileSize.x); x++)
	{
		for (UINT y = 0; y < static_cast<UINT>(tileSize.y); y++)
		{
			UINT index = y * tileSize.x + x;
			float temp = heightMap[index];

			if (_isWallCollider)
				walls[index] = temp > 0.9f;
			else
				heightValues[index] = temp;

			if (temp < minHeight)
			{
				_minIndex = index;
				minHeight = temp;
			}
			if (temp > maxHeight)
			{
				_maxIndex = index;
				maxHeight = temp;
			}
		}
	}
}

bool Collisions::Terrain::GetHeight(UINT x, UINT y, float &height) const
{
	if (_isWallCollider)
		return false;

	if (x < static_cast<UINT>(tileSize.x) && y < static_cast<UINT>(tileSize.y))
	{
		int scalar = _invertHeight ? -1 : 1;
		height = (center.y - scalar * halfLength.y) + scalar * heightValues[(tileSize.y - 1.0f - y) * tileSize.x + x] * halfLength.y * 2.0f;
		return true;
	}
	return false;
}

bool Collisions::Terrain::GetHeight(const dx::XMFLOAT2 &pos, float &height) const
{
	if (_isWallCollider)
		return false;

	float localX = pos.x - (center.x - halfLength.x);
    float localZ = pos.y - (center.z - halfLength.z);

    float localU = localX / (2.0f * halfLength.x);
    float localV = localZ / (2.0f * halfLength.z);

    float mapX = localU * tileSize.x;
    float mapY = localV * tileSize.y;

    int baseX = static_cast<int>(std::floor(mapX));
    int baseY = static_cast<int>(std::floor(mapY));

    float fracX = mapX - baseX;
    float fracY = mapY - baseY;

    if (localU < 0 || localU > 1 || localV < 0 || localV > 1)
        return false;  // Outside terrain bounds

    // Prevent out of bounds
    baseX = std::clamp(baseX, 0, tileSize.x - 2);
    baseY = std::clamp(baseY, 0, tileSize.y - 2);

    int nextX = baseX + 1;
    int nextY = baseY + 1;

    // Compute step sizes based on terrain dimensions
    float stepSizeX = (2.0f * halfLength.x) / static_cast<float>(tileSize.x);
    float stepSizeZ = (2.0f * halfLength.z) / static_cast<float>(tileSize.y);

    // Heights at four surrounding points
    float h00, h10, h01, h11;
    GetHeight(baseX, baseY, h00);      // Top-left
    GetHeight(nextX, baseY, h10);      // Top-right
    GetHeight(baseX, nextY, h01);      // Bottom-left
    GetHeight(nextX, nextY, h11);      // Bottom-right

    // Bilinear interpolation for height
    float heightTop = (1.0f - fracX) * h00 + fracX * h10;
    float heightBottom = (1.0f - fracX) * h01 + fracX * h11;
    height = (1.0f - fracY) * heightTop + fracY * heightBottom;

	return true;
}

bool Collisions::Terrain::GetHeight(const dx::XMFLOAT2 &pos, float &height, dx::XMFLOAT3 &normal) const
{
	if (_isWallCollider)
		return false;

    float localX = pos.x - (center.x - halfLength.x);
    float localZ = pos.y - (center.z - halfLength.z);

    float localU = localX / (2.0f * halfLength.x);
    float localV = localZ / (2.0f * halfLength.z);

    if (localU < 0 || localU > 1 || localV < 0 || localV > 1)
        return false;

    float mapX = localU * tileSize.x;
    float mapY = localV * tileSize.y;

    int baseX = (int)std::floor(mapX);
    int baseY = (int)std::floor(mapY);

    float fracX = mapX - baseX;
    float fracY = mapY - baseY;

    baseX = std::clamp(baseX, 2, tileSize.x - 2);
    baseY = std::clamp(baseY, 2, tileSize.y - 2);

    int nextX = baseX + 1;
    int nextY = baseY + 1;

    float stepSizeX = (2.0f * halfLength.x) / (float)tileSize.x;
    float stepSizeZ = (2.0f * halfLength.z) / (float)tileSize.y;

    float h00, h10, h01, h11;
    GetHeight(baseX, baseY, h00);      // Top-left
    GetHeight(nextX, baseY, h10);      // Top-right
    GetHeight(baseX, nextY, h01);      // Bottom-left
    GetHeight(nextX, nextY, h11);      // Bottom-right

    // Bilinear interpolation for height
    float heightTop = (1.0f - fracX) * h00 + fracX * h10;
    float heightBottom = (1.0f - fracX) * h01 + fracX * h11;
    height = (1.0f - fracY) * heightTop + fracY * heightBottom;

	XMVECTOR n = XMVectorSet(
		(h00 - h10) / (2 * stepSizeX),
		1,
		(h01 - h11) / (2 * stepSizeZ),
		0);

	n = XMVector3Normalize(n);

	if (_invertHeight)
		n = XMVectorNegate(n);

	Store(&normal, n);

	return true;
}

bool Collisions::Terrain::GetNormal(UINT x, UINT y, dx::XMFLOAT3 &normal) const
{
	if (_isWallCollider)
		return false;

	int baseX = (int)std::floor(x);
    int baseY = (int)std::floor(y);

    float fracX = x - baseX;
    float fracY = y - baseY;

    baseX = std::clamp(baseX, 1, tileSize.x - 2);
    baseY = std::clamp(baseY, 1, tileSize.y - 2);

    float stepSizeX = (2.0f * halfLength.x) / (float)tileSize.x;
    float stepSizeZ = (2.0f * halfLength.z) / (float)tileSize.y;


	float h00, h10, h01, h11;

	h00 = heightValues[(tileSize.y - 1.f - baseY) * tileSize.x + baseX-1];
	h10 = heightValues[(tileSize.y - 1.f - baseY) * tileSize.x + baseX+1];
	h01 = heightValues[(tileSize.y - 1.f - (baseY-1)) * tileSize.x + baseX];
	h11 = heightValues[(tileSize.y - 1.f - (baseY+1)) * tileSize.x + baseX];

	XMVECTOR n = XMVectorSet(
		(h00 - h10) / (2*stepSizeX),
		1,
		(h01 - h11) / (2*stepSizeZ),
		0);

	n = XMVector3Normalize(n);

	if (_invertHeight)
		n = XMVectorNegate(n);

	Store(&normal, n);

	return true;
}

bool Collisions::Terrain::GetNormal(const dx::XMFLOAT2 &pos, dx::XMFLOAT3 &normal) const
{
	if (_isWallCollider)
		return false;

	float localX = pos.x - (center.x - halfLength.x);
    float localZ = pos.y - (center.z - halfLength.z);

    float localU = localX / (2.0f * halfLength.x);
    float localV = localZ / (2.0f * halfLength.z);

    if (localU < 0 || localU > 1 || localV < 0 || localV > 1)
        return false;

    float mapX = localU * tileSize.x;
    float mapY = localV * tileSize.y;

    int baseX = (int)std::floor(mapX);
    int baseY = (int)std::floor(mapY);

    float fracX = mapX - baseX;
    float fracY = mapY - baseY;

    baseX = std::clamp(baseX, 1, tileSize.x - 2);
    baseY = std::clamp(baseY, 1, tileSize.y - 2);

    float stepSizeX = (2.0f * halfLength.x) / (float)tileSize.x;
    float stepSizeZ = (2.0f * halfLength.z) / (float)tileSize.y;

    float h00, h10, h01, h11;

	h00 = heightValues[(tileSize.y - 1.f - baseY) * tileSize.x + baseX-1];
	h10 = heightValues[(tileSize.y - 1.f - baseY) * tileSize.x + baseX+1];
	h01 = heightValues[(tileSize.y - 1.f - (baseY-1)) * tileSize.x + baseX];
	h11 = heightValues[(tileSize.y - 1.f - (baseY+1)) * tileSize.x + baseX];

	XMVECTOR n = XMVectorSet(
		(h00 - h10) / (2*stepSizeX),
		1,
		(h01 - h11) / (2*stepSizeZ),
		0);

	n = XMVector3Normalize(n);

	if (_invertHeight)
		n = XMVectorNegate(n);

	Store(&normal, n);

	return true;
}

bool Collisions::Terrain::IsWall(const dx::XMFLOAT2 &pos) const
{
	if (!_isWallCollider)
		return false;

	float localX = pos.x - (center.x - halfLength.x);
    float localZ = pos.y - (center.z - halfLength.z);

    float localU = localX / (2.0f * halfLength.x);
    float localV = localZ / (2.0f * halfLength.z);

    float mapX = localU * tileSize.x;
    float mapY = localV * tileSize.y;

    int baseX = static_cast<int>(std::floor(mapX));
    int baseY = static_cast<int>(std::floor(mapY));

	return IsWall(baseX, baseY);
}

bool Collisions::Terrain::IsWall(UINT x, UINT y) const
{
	if (!_isWallCollider)
		return false;

	if (x < static_cast<UINT>(tileSize.x) && y < static_cast<UINT>(tileSize.y))
	{
		int scalar = _invertHeight ? -1 : 1;
		return walls[(tileSize.y - 1.f - y) * tileSize.x + x];
	}
	return true; // Outside of terrain
}

dx::XMFLOAT2 Collisions::Terrain::GetTileWorldSize() const
{
	return { halfLength.x / tileSize.x, halfLength.z / tileSize.y };
}

bool Collisions::Terrain::IsInverted() const
{
	return _invertHeight;
}

bool Collisions::Terrain::IsWallCollider() const
{
	return _isWallCollider;
}

#ifdef DEBUG_BUILD
bool Collisions::Terrain::RenderDebug(Scene *scene, const RenderQueuer &queuer, const RendererInfo &renderInfo)
{
	if (renderInfo.shadowCamera)
		return true;

	if (!debug)
		return true;

	DebugDrawer &debugDraw = DebugDrawer::Instance();

	debugDraw.DrawLineThreadSafe(_min, _max, 0.01f, { 1, 0, 1, 1 });


	XMFLOAT4 color;
	XMFLOAT3 max, min;

	for (int i = _selected[0]; i < _selected[0] + _selectedSize[0]; i+=_selectedSkip)
	{
		for (int j = _selected[1]; j < _selected[1] + _selectedSize[1]; j+=_selectedSkip)
		{ 
			//if ((j != _selected[1] && !_debugFlip) || (i != _selected[0] && _debugFlip)) continue;
			//if (i > _selected[0] && i < _selected[0] + _selectedSize[0] && j > _selected[1] && j < _selected[1] + _selectedSize[0])
			float z = (j * halfLength.z * 2) / tileSize.y + (center.z - halfLength.z);
			float x = (i * halfLength.x * 2) / tileSize.x + (center.x - halfLength.x);

			 //if (_debugFlip)
				//std::swap<float>(x, z);

			if (!_isWallCollider)
			{
				float sampValue;
				GetHeight(i, j, sampValue);


				int scalar = _invertHeight ? -1 : 1;
				min = XMFLOAT3(x, center.y - scalar * halfLength.y, z);

				GetHeight(i, j, sampValue);
				max = XMFLOAT3(x, sampValue, z);

				color = { sampValue, sampValue, sampValue, 1 };
			}
			else
			{
				float y = center.y - halfLength.y;
				min = XMFLOAT3(x, y, z);
			
				y = center.y + halfLength.y;
				max = XMFLOAT3(x, y, z);

				float colorR = IsWall(i, j) ? 1 : 0;
				color = { colorR, 1 - colorR, 0, 1 };
			}

			debugDraw.DrawLineThreadSafe(min, max, _debugLineSize*0.175f*_selectedSkip, _debugColor);
		}
	}

	float localX = _uv[0] * halfLength.x * 2,
		  localZ = _uv[1] * halfLength.z * 2;

	float worldX = localX + (center.x - halfLength.x),
		  worldZ = localZ + (center.z - halfLength.z);

	if (!_isWallCollider)
	{
		min = XMFLOAT3(worldX, center.y - halfLength.y, worldZ);
		float y;
		XMFLOAT3 n;
		GetHeight({ worldX, worldZ }, y, n);
		max = XMFLOAT3(worldX, y, worldZ);

		debugDraw.DrawLineThreadSafe(min, max, 0.02f, { 0, 1, 0.5f, 1 });

		float nScale = 0.2f;
		debugDraw.DrawLineThreadSafe(max, { max.x + n.x * nScale, max.y + n.y * nScale, max.z + n.z * nScale }, 0.02f, { 0.25f, 0.75f, 0.25f, 1 });
	}

	return true;
}
#endif

bool Collisions::Terrain::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	json::Value centerArr(json::kArrayType);
	centerArr.PushBack(center.x, docAlloc);
	centerArr.PushBack(center.y, docAlloc);
	centerArr.PushBack(center.z, docAlloc);
	obj.AddMember("Center", centerArr, docAlloc);

	json::Value halfLengthArr(json::kArrayType);
	halfLengthArr.PushBack(halfLength.x, docAlloc);
	halfLengthArr.PushBack(halfLength.y, docAlloc);
	halfLengthArr.PushBack(halfLength.z, docAlloc);
	obj.AddMember("Half-Length", halfLengthArr, docAlloc);

	json::Value tileSizeArr(json::kArrayType);
	tileSizeArr.PushBack(tileSize.x, docAlloc);
	tileSizeArr.PushBack(tileSize.y, docAlloc);
	obj.AddMember("Tile Size", tileSizeArr, docAlloc);

	json::Value mapStr(json::kStringType);
	mapStr.SetString(map.c_str(), docAlloc);
	obj.AddMember("Map", mapStr, docAlloc);

	return true;
}

#ifdef USE_IMGUI
bool Collisions::Terrain::RenderUI()
{
	ImGui::Text("Terrain Collider");
	ImGui::Separator();

	XMVECTOR c = XMLoadFloat3(&center);

	float minValues[3] = { -100, -100, -100 },
		  maxValues[3] = { 100, 100, 100 },
		  speed = 0.001f;

	_isDirty |= ImGui::DragScalarN("Center", ImGuiDataType_Float, c.m128_f32, 3, speed, minValues, maxValues, "%.3f");
	_isDirty |= ImGui::Checkbox("Invert Height", &_invertHeight);

	ImGui::Separator();

	ImGui::Text("Debug Controlls");

	_isDirty |= ImGui::Checkbox("Flip Debug:", &_debugFlip);

	Store(&center, c);

	_isDirty |= ImGui::DragInt2("HeightMap", _selected, 0.25f, 0, tileSize.x-1);
	_isDirty |= ImGui::DragInt2("Selected Size", _selectedSize, 0.25f, 0, 256);
	_isDirty |= ImGui::DragInt("Selected Skip", &_selectedSkip, 0.25f, 0, 64);
	_selectedSkip = std::max<float>(_selectedSkip, 1);

	_isDirty |= ImGui::DragFloat("Debug Line Size", &_debugLineSize, 0.001f, 0, 64);

	_isDirty |= ImGui::DragFloat2("UV", _uv, 0.0005f, 0, 1);

	_isDirty |= ImGui::ColorEdit4("Debug Color", &_debugColor.x);


	return true;
}
#endif

void Collisions::Terrain::Transform(dx::XMFLOAT4X4A M, Collider *transformed)
{
	if (!transformed)
		return;
	if (transformed->colliderType != TERRAIN_COLLIDER)
		return;

	Terrain *transformedTerrain = dynamic_cast<Terrain *>(transformed);

	transformedTerrain->_active = _active;

	XMMATRIX worldMatrix = XMLoadFloat4x4(&M);

	// Translation
	Store(&transformedTerrain->center, XMVector3Transform(XMLoadFloat3(&center), worldMatrix));

	// Rotation
	XMVECTOR maxAxis = XMVectorSet(0, 0, 0, 0);
	XMVECTOR hl = XMLoadFloat3(&halfLength);
	XMVECTOR tempAxes[3] = { XMVectorSet(1, 0, 0, 0), 
							 XMVectorSet(0, 1, 0, 0), 
							 XMVectorSet(0, 0, 1, 0) };

    for (int i = 0; i < 3; i++)
    {
        XMVECTOR axisVec = tempAxes[i];
        axisVec = XMVector3Normalize(XMVector3TransformNormal(axisVec, worldMatrix));
		XMVECTOR temp = XMVectorSet(0, 0, 0, 0);
		temp = XMVectorMax(temp, XMVectorScale(axisVec,		 hl.m128_f32[i] * XMVectorGetX(XMVector3Length(worldMatrix.r[i]))));
		temp = XMVectorMax(temp, XMVectorScale(axisVec, -1 * hl.m128_f32[i] * XMVectorGetX(XMVector3Length(worldMatrix.r[i]))));
		maxAxis = XMVectorAdd(maxAxis, temp);
    }

	Store(&transformedTerrain->halfLength, maxAxis);

	// Scale Height
	_heightScale = XMVectorGetX(XMVector3Length(worldMatrix.r[1]));


	// Update min max

	float minHeight = 0, maxHeight = 0, maxHeightScalar = 1;
	if (!_isWallCollider)
	{
		minHeight = heightValues[_minIndex] * _heightScale;
		maxHeight = heightValues[_maxIndex] * _heightScale;
		maxHeightScalar = -1;
	}

	transformedTerrain->_min = { transformedTerrain->center.x - transformedTerrain->halfLength.x,
								 transformedTerrain->center.y - transformedTerrain->halfLength.y + minHeight,
								 transformedTerrain->center.z - transformedTerrain->halfLength.z};

	transformedTerrain->_max = { transformedTerrain->center.x + transformedTerrain->halfLength.x,
								 transformedTerrain->center.y + maxHeightScalar * transformedTerrain->halfLength.y + maxHeight,
								 transformedTerrain->center.z + transformedTerrain->halfLength.z};

	transformedTerrain->_invertHeight = _invertHeight;

#ifdef DEBUG_BUILD
	if (debug)
	{
		transformedTerrain->_selected[0] = _selected[0];
		transformedTerrain->_selected[1] = _selected[1];

		transformedTerrain->_selectedSize[0] = _selectedSize[0];
		transformedTerrain->_selectedSize[1] = _selectedSize[1];

		transformedTerrain->_selectedSkip = _selectedSkip;
		transformedTerrain->_debugLineSize = _debugLineSize;
		transformedTerrain->_debugColor = _debugColor;

		transformedTerrain->_uv[0] = _uv[0];
		transformedTerrain->_uv[1] = _uv[1];

		transformedTerrain->_debugFlip = _debugFlip;
	}
#endif

	_isDirty = false;
}

#ifdef DEBUG_BUILD
bool Collisions::Terrain::TransformDebug(ID3D11DeviceContext *context, TimeUtils &time, const Input &input)
{
	return true;
}
#endif
