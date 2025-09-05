#include "stdafx.h"
#include "Behaviours/CameraCubeBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

CameraCubeBehaviour::CameraCubeBehaviour(const CameraPlanes &planes, bool hasCSBuffer)
{
	_hasCSBuffer = hasCSBuffer;
	_cameraPlanes = planes;
}

// Start runs once when the behaviour is created.
bool CameraCubeBehaviour::Start()
{
	if (_name.empty())
		_name = "CameraCubeBehaviour"; // For categorization in ImGui.

	ID3D11Device *device = GetScene()->GetDevice();

	XMFLOAT4X4A viewProjMatrixArray[6] = {};
	for (int i = 0; i < 6; i++)
		viewProjMatrixArray[i] = GetViewProjectionMatrix(i);

	if (!_viewProjArrayBuffer.Initialize(device, sizeof(XMFLOAT4X4A) * 6, &viewProjMatrixArray))
		Warn("Failed to initialize camera cube GS buffer!");

	if (_hasCSBuffer)
	{
		_posBuffer = std::make_unique<ConstantBufferD3D11>();

		CameraCubeBufferData camBufferData = {
			GetTransform()->GetPosition(World),
			_cameraPlanes.nearZ,
			_cameraPlanes.farZ
		};

		if (!_posBuffer->Initialize(device, sizeof(CameraCubeBufferData), &camBufferData))
			Warn("Failed to initialize camera CS buffer!");
	}

	// Cube bounds
	_bounds.Extents = { _cameraPlanes.farZ, _cameraPlanes.farZ, _cameraPlanes.farZ };

	return true;
}

void CameraCubeBehaviour::OnDirty()
{
	_isDirty = true;
	_recalculateBounds = true;
	_recalculateFrustumBounds = true;
}

#ifdef USE_IMGUI
bool CameraCubeBehaviour::RenderUI()
{
	ImGui::Text(std::format("Near: {}", _cameraPlanes.nearZ).c_str());
	ImGui::Text(std::format("Far: {}", _cameraPlanes.farZ).c_str());

	static bool drawRenderBounds = false;
	ImGui::Checkbox("Draw Bounds", &drawRenderBounds);

	if (drawRenderBounds)
	{
		auto &debugDrawer = DebugDrawer::Instance();

		dx::BoundingBox activeBox;
		if (!StoreBounds(activeBox))
			Warn("Failed to get camera cube bounds!");

		dx::XMFLOAT4 color(0, 1, 0, 0.5f);

		for (int i = 0; i < 2; i++)
		{
			// Axis-aligned box, draw 12 lines
			XMVECTOR centerVec = Load(activeBox.Center);
			XMFLOAT3 extentsAbs = { std::abs(activeBox.Extents.x), std::abs(activeBox.Extents.y), std::abs(activeBox.Extents.z) };
			XMFLOAT3 v0, v1, v2, v3, v4, v5, v6, v7;

			Store(v0, centerVec + XMVectorSet(-extentsAbs.x, -extentsAbs.y, -extentsAbs.z, 0));
			Store(v1, centerVec + XMVectorSet(extentsAbs.x, -extentsAbs.y, -extentsAbs.z, 0));
			Store(v2, centerVec + XMVectorSet(extentsAbs.x, -extentsAbs.y, extentsAbs.z, 0));
			Store(v3, centerVec + XMVectorSet(-extentsAbs.x, -extentsAbs.y, extentsAbs.z, 0));
			Store(v4, centerVec + XMVectorSet(-extentsAbs.x, extentsAbs.y, -extentsAbs.z, 0));
			Store(v5, centerVec + XMVectorSet(extentsAbs.x, extentsAbs.y, -extentsAbs.z, 0));
			Store(v6, centerVec + XMVectorSet(extentsAbs.x, extentsAbs.y, extentsAbs.z, 0));
			Store(v7, centerVec + XMVectorSet(-extentsAbs.x, extentsAbs.y, extentsAbs.z, 0));

			debugDrawer.DrawLine(v0, v4, 0.2f, color, false);
			debugDrawer.DrawLine(v1, v5, 0.2f, color, false);
			debugDrawer.DrawLine(v2, v6, 0.2f, color, false);
			debugDrawer.DrawLine(v3, v7, 0.2f, color, false);
			debugDrawer.DrawLine(v0, v1, 0.2f, color, false);
			debugDrawer.DrawLine(v1, v2, 0.2f, color, false);
			debugDrawer.DrawLine(v2, v3, 0.2f, color, false);
			debugDrawer.DrawLine(v3, v0, 0.2f, color, false);
			debugDrawer.DrawLine(v4, v5, 0.2f, color, false);
			debugDrawer.DrawLine(v5, v6, 0.2f, color, false);
			debugDrawer.DrawLine(v6, v7, 0.2f, color, false);
			debugDrawer.DrawLine(v7, v4, 0.2f, color, false);

			activeBox.Extents = { _cameraPlanes.nearZ, _cameraPlanes.nearZ, _cameraPlanes.nearZ };
			dx::XMFLOAT4 color(1, 0, 0, 0.5f);
		}
	}

	return true;
}
#endif

bool CameraCubeBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	return true;
}
bool CameraCubeBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	return true;
}


void CameraCubeBehaviour::GetAxes(UINT cameraIndex, XMFLOAT3A *right, XMFLOAT3A *up, XMFLOAT3A *forward)
{
	switch (cameraIndex)
	{
	case 0:
		if (right)	 *right		= { 0, 0, 1 };
		if (up)		 *up		= { 0, -1, 0 };
		if (forward) *forward	= { 1, 0, 0 };
		break;

	case 1:
		if (right)	 *right		= { 0, 0, -1 };
		if (up)		 *up		= { 0, -1, 0 };
		if (forward) *forward	= { -1, 0, 0 };
		break;

	case 2:
		if (right)	 *right		= { 1, 0, 0 };
		if (up)		 *up		= { 0, 0, 1 };
		if (forward) *forward	= { 0, -1, 0 };
		break;

	case 3:
		if (right)	 *right		= { 1, 0, 0 };
		if (up)		 *up		= { 0, 0, -1 };
		if (forward) *forward	= { 0, 1, 0 };
		break;

	case 4:
		if (right)	 *right		= { 1, 0, 0 };
		if (up)		 *up		= { 0, -1, 0 };
		if (forward) *forward	= { 0, 0, -1 };
		break;

	case 5:
		if (right)	 *right		= { -1, 0, 0 };
		if (up)		 *up		= { 0, -1, 0 };
		if (forward) *forward	= { 0, 0, 1 };
		break;

	default:
		Warn("Invalid camera index!");
		break;
	}
}

XMFLOAT4A CameraCubeBehaviour::GetRotation(UINT cameraIndex)
{
	XMFLOAT4A result;

	switch (cameraIndex)
	{
	case 0:
		Store(result, XMQuaternionRotationRollPitchYaw(0, XM_PIDIV2, XM_PI));
		return result;

	case 1:
		Store(result, XMQuaternionRotationRollPitchYaw(0, -XM_PIDIV2, XM_PI));
		return result;

	case 2:
		Store(result, XMQuaternionRotationRollPitchYaw(XM_PIDIV2, 0, 0));
		return result;

	case 3:
		Store(result, XMQuaternionRotationRollPitchYaw(-XM_PIDIV2, 0, 0));
		return result;

	case 4:
		Store(result, XMQuaternionRotationRollPitchYaw(0, XM_PI, XM_PI));
		return result;

	case 5:
		Store(result, XMQuaternionRotationRollPitchYaw(0, 0, XM_PI));
		return result;

	default:
		Warn("Invalid camera index!");
		return { 0, 0, 0, 1 };
	}
}

XMFLOAT4X4A CameraCubeBehaviour::GetViewMatrix(UINT cameraIndex)
{
	XMFLOAT3A r, u, f;
	GetAxes(cameraIndex, &r, &u, &f);

	XMVECTOR 
		posVec = Load(GetTransform()->GetPosition(World)),
		up = Load(u),
		forward = Load(f);

	XMMATRIX projectionMatrix = XMMatrixLookAtLH(
		posVec,
		posVec + forward,
		up
	);

	XMFLOAT4X4A result;
	Store(result, projectionMatrix);
	return result;
}
XMFLOAT4X4A CameraCubeBehaviour::GetProjectionMatrix() const
{
	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		1,
		_cameraPlanes.nearZ,
		_cameraPlanes.farZ
	);

	XMFLOAT4X4A pMat;
	Store(pMat, projectionMatrix);
	return pMat;
}
XMFLOAT4X4A CameraCubeBehaviour::GetViewProjectionMatrix(UINT cameraIndex)
{
	XMFLOAT4X4A vpMatrix = { };
	Store(vpMatrix, XMMatrixTranspose(Load(GetViewMatrix(cameraIndex)) * Load(GetProjectionMatrix())));
	return vpMatrix;
}

bool CameraCubeBehaviour::UpdateBuffers()
{
	if (!_isDirty)
		return true;

	auto context = GetScene()->GetContext();

	XMFLOAT4X4A viewProjArray[6] = {};
	for (int i = 0; i < 6; i++)
		viewProjArray[i] = GetViewProjectionMatrix(i);

	if (!_viewProjArrayBuffer.UpdateBuffer(context, &viewProjArray))
		Warn("Failed to update camera view projection buffer!");

	if (_hasCSBuffer)
	{
		CameraCubeBufferData camBufferData = {
			GetTransform()->GetPosition(World),
			_cameraPlanes.nearZ,
			_cameraPlanes.farZ
		};

		if (!_posBuffer->UpdateBuffer(context, &camBufferData))
			Warn("Failed to update camera CS buffer!");
	}

	_isDirty = false;
	return true;
}

bool CameraCubeBehaviour::BindShadowCasterBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const vpmaBuffer = GetCameraGSBuffer();
	context->GSSetConstantBuffers(1, 1, &vpmaBuffer);

	ID3D11Buffer *const camPosBuffer = GetCameraCSBuffer();
	if (!camPosBuffer)
	{
		Warn("Failed to bind pos buffer, camera does not have that buffer!");
		return false;
	}
	context->PSSetConstantBuffers(1, 1, &camPosBuffer);

	return true;
}
bool CameraCubeBehaviour::BindGeometryBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const vpmaBuffer = GetCameraGSBuffer();
	context->GSSetConstantBuffers(1, 1, &vpmaBuffer);

	return true;
}
bool CameraCubeBehaviour::BindLightingBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const camPosBuffer = GetCameraCSBuffer();
	if (camPosBuffer == nullptr)
	{
		Warn("Failed to bind lighting buffer, camera does not have that buffer!");
		return true;
	}
	context->PSSetConstantBuffers(3, 1, &camPosBuffer);

	return true;
}
bool CameraCubeBehaviour::BindTransparentBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const vpmaBuffer = GetCameraGSBuffer();
	context->GSSetConstantBuffers(1, 1, &vpmaBuffer);

	ID3D11Buffer *const camPosBuffer = GetCameraCSBuffer();
	if (camPosBuffer == nullptr)
	{
		Warn("Failed to bind lighting buffer, camera does not have that buffer!");
		return true;
	}
	context->PSSetConstantBuffers(3, 1, &camPosBuffer);

	return true;
}
bool CameraCubeBehaviour::BindViewBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const vpmaBuffer = GetCameraGSBuffer();
	context->GSSetConstantBuffers(1, 1, &vpmaBuffer);

	return true;
}

bool CameraCubeBehaviour::StoreBounds(BoundingBox &bounds) // TODO: Change to use bounding sphere
{
	if (_recalculateBounds)
	{
		_transformedBounds.Center = GetTransform()->GetPosition(World);
		_transformedBounds.Extents = { _cameraPlanes.farZ, _cameraPlanes.farZ, _cameraPlanes.farZ };
		_recalculateBounds = false;
	}

	bounds = _transformedBounds;
	return true;
}

void CameraCubeBehaviour::QueueGeometry(const RenderQueueEntry &entry)
{
	_geometryRenderQueue.emplace_back(entry);
}
void CameraCubeBehaviour::QueueTransparent(const RenderQueueEntry &entry)
{
	_transparentRenderQueue.emplace_back(entry);
}
void CameraCubeBehaviour::ResetRenderQueue()
{
	size_t geometryDrawCount = _geometryRenderQueue.size();
	size_t transparentDrawCount = _transparentRenderQueue.size();

	UINT drawCount = static_cast<UINT>(geometryDrawCount + transparentDrawCount);

	_geometryRenderQueue.clear();
	_transparentRenderQueue.clear();

	_geometryRenderQueue.reserve(geometryDrawCount);
	_transparentRenderQueue.reserve(transparentDrawCount);

	_lastCullCount = drawCount;
}

void CameraCubeBehaviour::SortGeometryQueue()
{
	ZoneScopedXC(RandomUniqueColor());

	std::sort(_geometryRenderQueue.begin(), _geometryRenderQueue.begin());
}
void CameraCubeBehaviour::SortTransparentQueue()
{
	ZoneScopedXC(RandomUniqueColor());

	std::sort(_transparentRenderQueue.begin(), _transparentRenderQueue.begin());
}

const std::vector<RenderQueueEntry> &CameraCubeBehaviour::GetGeometryQueue() const
{
	return _geometryRenderQueue;
}
const std::vector<RenderQueueEntry> &CameraCubeBehaviour::GetTransparentQueue() const
{
	return _transparentRenderQueue;
}

UINT CameraCubeBehaviour::GetCullCount() const
{
	return _lastCullCount;
}
void CameraCubeBehaviour::SetCullCount(UINT cullCount) 
{
	_lastCullCount = cullCount;
}

void CameraCubeBehaviour::SetRendererInfo(const RendererInfo &rendererInfo)
{
	_rendererInfo = rendererInfo;
}
void CameraCubeBehaviour::SetFarZ(float farZ)
{
	_cameraPlanes.farZ = farZ;

	_isDirty = true;
	_recalculateBounds = true;
	_recalculateFrustumBounds = true;
}
RendererInfo CameraCubeBehaviour::GetRendererInfo() const
{
	return _rendererInfo;
}
float CameraCubeBehaviour::GetNearZ() const
{
	return _cameraPlanes.nearZ;
}
float CameraCubeBehaviour::GetFarZ() const
{
	return _cameraPlanes.farZ;
}

ID3D11Buffer *CameraCubeBehaviour::GetCameraGSBuffer() const
{
	return _viewProjArrayBuffer.GetBuffer();
}
ID3D11Buffer *CameraCubeBehaviour::GetCameraCSBuffer() const
{
	if (_posBuffer == nullptr)
		return nullptr;

	return _posBuffer->GetBuffer();
}