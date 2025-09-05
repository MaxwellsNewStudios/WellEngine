#include "stdafx.h"
#include "Behaviours/CameraBehaviour.h"
#include "Entity.h"
#include "Scenes/Scene.h"
#include "Rendering/RenderQueuer.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

CameraBehaviour::CameraBehaviour(const ProjectionInfo &projectionInfo, bool isOrtho, bool invertDepth)
{
	_invertedDepth = invertDepth;
	_ortho = isOrtho;
	_currProjInfo = _defaultProjInfo = projectionInfo;
}

// Start runs once when the behaviour is created.
bool CameraBehaviour::Start()
{
	if (_name.empty())
		_name = "CameraBehaviour"; // For categorization in ImGui.

	ID3D11Device *device = GetScene()->GetDevice();

	const XMFLOAT4X4A viewProjMatrix = GetViewProjectionMatrix();
	if (!_viewProjBuffer.Initialize(device, sizeof(XMFLOAT4X4A), &viewProjMatrix))
		Warn("Failed to initialize camera VS buffer!");

	const XMFLOAT4A pos = To4(GetTransform()->GetPosition());
	const GeometryBufferData bufferData = { GetViewMatrix(), pos };
	_viewProjPosBuffer = std::make_unique<ConstantBufferD3D11>();
	if (!_viewProjPosBuffer->Initialize(device, sizeof(GeometryBufferData), &bufferData))
		Warn("Failed to initialize camera GS buffer!");

	const XMFLOAT4X4A projMatrix = GetProjectionMatrix(_invertedDepth);

	_invCamBuffer = std::make_unique<ConstantBufferD3D11>();
	XMFLOAT4X4 invCamBufferData[2] = { };
	Store(invCamBufferData[0], XMMatrixInverse(nullptr, Load(projMatrix)));
	Store(invCamBufferData[1], XMMatrixInverse(nullptr, Load(GetViewMatrix())));

	if (!_invCamBuffer->Initialize(device, sizeof(invCamBufferData), &invCamBufferData))
		Warn("Failed to initialize inverse camera buffer!");

	XMFLOAT4X4 invViewProjBufferData = { };
	Store(invViewProjBufferData, XMMatrixInverse(nullptr, Load(viewProjMatrix)));

	_posBuffer = std::make_unique<ConstantBufferD3D11>();
	CameraBufferData camBufferData = { 
		projMatrix,
		viewProjMatrix,
		invViewProjBufferData,
		invCamBufferData[0],
		GetTransform()->GetPosition(World),
		GetTransform()->GetForward(World),
		_invertedDepth ? _currProjInfo.planes.farZ : _currProjInfo.planes.nearZ,
		_invertedDepth ? _currProjInfo.planes.nearZ : _currProjInfo.planes.farZ
	};

	if (!_posBuffer->Initialize(device, sizeof(CameraBufferData), &camBufferData))
		Warn("Failed to initialize camera CS buffer!");

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	QueueParallelUpdate();

	return true;
}

void CameraBehaviour::OnDirty()
{
	_isDirty = true;
	_recalculateBounds = true;
}

bool CameraBehaviour::ParallelUpdate(const TimeUtils &time, const Input &input)
{
	if (_screenFadeRate != 0.0f)
	{
		_screenFadeAmount += _screenFadeRate * time.GetDeltaTime();

		if (_screenFadeAmount <= 0.0f || _screenFadeAmount >= 1.0f)
		{
			_screenFadeAmount = std::clamp(_screenFadeAmount, 0.0f, 1.0f);
			_screenFadeRate = 0.0f;
		}
	}

	if (_debugDraw)
	{
		using namespace DebugDraw;

		XMFLOAT3 corners[8]{};

		if (_ortho)
		{
			BoundingOrientedBox transformedBounds;
			if (!StoreBounds(transformedBounds, false))
				Warn("Failed to store camera bounds!");

			transformedBounds.GetCorners(corners);
		}
		else
		{
			BoundingFrustum transformedBounds;
			if (!StoreBounds(transformedBounds, false))
				Warn("Failed to store camera bounds!");

			transformedBounds.GetCorners(corners);
		}

		// Draw line segment between 0-1-2-3-0, 4-5-6-7-4, 0-4, 1-5, 2-6, 3-7
		// LineSection takes a point, a size and a color.
		// Line takes two LineSections.
		std::vector<LineSection> nearLineStrip; // size = 0.05f, color = green
		std::vector<LineSection> farLineStrip; // size = 0.05f, color = red
		std::vector<Line> connectingLines; // size = 0.05f, color = yellow

		float thickness = 0.05f;
		XMFLOAT4 green = { 0.0f, 1.0f, 0.0f, 1 };
		XMFLOAT4 red = { 1.0f, 0.0f, 0.0f, 1 };

		for (int i = 0; i < 4; i++)
		{
			nearLineStrip.emplace_back(corners[i], thickness, green);
			farLineStrip.emplace_back(corners[i + 4], thickness, red);
			connectingLines.emplace_back( 
				LineSection(corners[i], 0.05f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1)), 
				LineSection(corners[i + 4], 0.05f, { 1.0f, 1.0f, 0.0f, 1 }) 
			);
		}
		nearLineStrip.emplace_back(corners[0], thickness, green);
		farLineStrip.emplace_back(corners[4], thickness, red);

		DebugDrawer::Instance().DrawLineStripThreadSafe(nearLineStrip, !_overlayDraw);
		DebugDrawer::Instance().DrawLineStripThreadSafe(farLineStrip, !_overlayDraw);
		DebugDrawer::Instance().DrawLinesThreadSafe(connectingLines, !_overlayDraw);
	}

	return true;
}

#ifdef USE_IMGUI
bool CameraBehaviour::RenderUI()
{
	if (ImGui::Button("Control"))
		GetScene()->SetViewCamera(this);

	if (ImGui::InputInt("Sort Mode", &_sortMode))
	{
		while (_sortMode < 0)
			_sortMode += 3;

		while (_sortMode >= 3)
			_sortMode -= 3;
	}

	ImGui::Checkbox("Debug Draw", &_debugDraw);
	
	if (_debugDraw)
		ImGui::Checkbox("Overlayed", &_overlayDraw);

	ImGui::Checkbox("Invert Depth", &_invertedDepth);

	ImGui::Text("Mode:"); ImGui::SameLine();
	if (ImGui::Button(_ortho ? "Orthographic" : "Perspective"))
		SetOrtho(!_ortho);

	bool valueChanged = false;

	CameraPlanes planes = GetPlanes();
	valueChanged |= ImGui::DragFloat("NearZ:", &planes.nearZ);
	ImGuiUtils::LockMouseOnActive();

	valueChanged |= ImGui::DragFloat("FarZ:", &planes.farZ);
	ImGuiUtils::LockMouseOnActive();

	if (valueChanged)
		SetPlanes(planes);

	float fov = GetFOV();
	ImGui::Text("FOV:"); ImGui::SameLine();
	if (_ortho)
	{
		if (ImGui::DragFloat("##FOVDrag", &fov, 1.0f, 0.1f))
			SetFOV(max(fov, 0.1f));
		ImGuiUtils::LockMouseOnActive();
	}
	else
	{
		fov *= RAD_TO_DEG;
		if (ImGui::SliderFloat("##FOVSlider", &fov, 0.1f, 179.9f))
			SetFOV(std::clamp(fov, 0.1f, 179.9f) * DEG_TO_RAD);
	}

	float aspect = _currProjInfo.aspectRatio;
	ImGui::Text("Aspect Ratio:"); ImGui::SameLine();
	if (ImGui::DragFloat("##AspectDrag", &aspect, 0.1f, 0.01f, 100.0f))
		SetAspectRatio(std::clamp(aspect, 0.01f, 100.0f));
	ImGuiUtils::LockMouseOnActive();

	static bool showCullingTree = false;
	static bool showCullingEntities = false;

	ImGui::Checkbox("Show Culling Tree", &showCullingTree);
	ImGui::Checkbox("Show Culling Entities", &showCullingEntities);

	if (showCullingEntities || showCullingTree)
	{
		BoundingFrustum frustum;
		if (!StoreBounds(frustum, false))
		{
			Warn("Failed to store camera bounds for culling!");
			return true;
		}

		auto &debugRawer = DebugDrawer::Instance();

		if (showCullingEntities)
		{
			std::vector<Entity *> cullingEntities;
			cullingEntities.reserve(_lastCullCount);

			if (!GetScene()->GetSceneHolder()->FrustumCull(frustum, cullingEntities))
			{
				Warn("Failed to cull entities in camera frustum!");
				return true;
			}

			for (auto &ent : cullingEntities)
			{
				dx::BoundingOrientedBox obb;
				ent->StoreEntityBounds(obb, World);
				debugRawer.DrawBoxOBB(obb, {0, 1, 0, 0.1f}, false, true);
			}
		}

		if (showCullingTree)
		{
			std::vector<dx::BoundingBox> boxCollection;
			GetScene()->GetSceneHolder()->DebugGetTreeStructure(boxCollection, frustum, true, true);

			dx::XMFLOAT3 lastCenter = { 0, 0, 0 };
			bool first = true;
			for (auto &box : boxCollection)
			{
				debugRawer.DrawBoxAABB(box, {1, 0, 0, 0.1f}, false, true);

				if (first)
					debugRawer.DrawLine(lastCenter, box.Center, 0.2f, dx::XMFLOAT4(1, 1, 1, 0.1f), true); // just to identify the start
				else
					debugRawer.DrawLine(lastCenter, box.Center, 0.5f, dx::XMFLOAT4(0, 0, 1, 0.2f), true);

				lastCenter = box.Center;
				first = false;
			}
		}
	}

	return true;
}
#endif

bool CameraBehaviour::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	obj.AddMember("Ortho", _ortho, docAlloc);
	obj.AddMember("Inverted", _invertedDepth, docAlloc);

	json::Value defaultProjObj(json::kObjectType);
	defaultProjObj.AddMember("FOV", _defaultProjInfo.fovAngleY, docAlloc);
	defaultProjObj.AddMember("Aspect", _defaultProjInfo.aspectRatio, docAlloc);
	defaultProjObj.AddMember("Near", _defaultProjInfo.planes.nearZ, docAlloc);
	defaultProjObj.AddMember("Far", _defaultProjInfo.planes.farZ, docAlloc);
	obj.AddMember("Default Projection", defaultProjObj, docAlloc);

	json::Value currProjObj(json::kObjectType);
	currProjObj.AddMember("FOV", _currProjInfo.fovAngleY, docAlloc);
	currProjObj.AddMember("Aspect", _currProjInfo.aspectRatio, docAlloc);
	currProjObj.AddMember("Near", _currProjInfo.planes.nearZ, docAlloc);
	currProjObj.AddMember("Far", _currProjInfo.planes.farZ, docAlloc);
	obj.AddMember("Current Projection", currProjObj, docAlloc);

	return true;
}
bool CameraBehaviour::Deserialize(const json::Value &obj, Scene *scene)
{
	_ortho			= obj["Ortho"].GetBool();
	_invertedDepth	= obj["Inverted"].GetBool();

	const json::Value &defaultProjObj = obj["Default Projection"];
	_defaultProjInfo.fovAngleY		= defaultProjObj["FOV"].GetFloat();
	_defaultProjInfo.aspectRatio	= defaultProjObj["Aspect"].GetFloat();
	_defaultProjInfo.planes.nearZ	= defaultProjObj["Near"].GetFloat();
	_defaultProjInfo.planes.farZ	= defaultProjObj["Far"].GetFloat();

	const json::Value &currProjObj = obj["Current Projection"];
	_currProjInfo.fovAngleY		= currProjObj["FOV"].GetFloat();
	_currProjInfo.aspectRatio	= currProjObj["Aspect"].GetFloat();
	_currProjInfo.planes.nearZ	= currProjObj["Near"].GetFloat();
	_currProjInfo.planes.farZ	= currProjObj["Far"].GetFloat();

	return true;
}


XMFLOAT4X4A CameraBehaviour::GetViewMatrix()
{
	XMFLOAT3A r, u, f;
	GetTransform()->GetAxes(&r, &u, &f, World);

	XMVECTOR
		posVec = Load(GetTransform()->GetPosition(World)),
		up = Load(u),
		forward = Load(f);

	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		posVec,
		posVec + forward,
		up
	);

	XMFLOAT4X4A result;
	Store(result, viewMatrix);
	return result;
}
XMFLOAT4X4A CameraBehaviour::GetProjectionMatrix(bool invert) const
{
	XMFLOAT4X4A projectionMatrix;

	if (_ortho)
	{
		Store(projectionMatrix, XMMatrixOrthographicLH(
			_currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			_currProjInfo.fovAngleY,
			invert ? _currProjInfo.planes.farZ : _currProjInfo.planes.nearZ,
			invert ? _currProjInfo.planes.nearZ : _currProjInfo.planes.farZ
		));
	}
	else
	{
		Store(projectionMatrix, XMMatrixPerspectiveFovLH(
			_currProjInfo.fovAngleY,
			_currProjInfo.aspectRatio,
			invert ? _currProjInfo.planes.farZ : _currProjInfo.planes.nearZ,
			invert ? _currProjInfo.planes.nearZ : _currProjInfo.planes.farZ
		));
	}

	return projectionMatrix;

	/*XMMATRIX projectionMatrix;

	if (_ortho)
	{
		projectionMatrix = XMMatrixOrthographicLH(
			_currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			_currProjInfo.fovAngleY,
			_currProjInfo.planes.nearZ,
			_currProjInfo.planes.farZ
		);
	}
	else
	{
		projectionMatrix = XMMatrixPerspectiveFovLH(
			_currProjInfo.fovAngleY,
			_currProjInfo.aspectRatio,
			_currProjInfo.planes.nearZ,
			_currProjInfo.planes.farZ
		);
	}

	return *reinterpret_cast<XMFLOAT4X4A *>(&projectionMatrix);*/
}
XMFLOAT4X4A CameraBehaviour::GetViewProjectionMatrix()
{
	XMFLOAT4X4A vpMatrix = { };
	Store(vpMatrix, XMMatrixTranspose(Load(GetViewMatrix()) * Load(GetProjectionMatrix(_invertedDepth))));
	return vpMatrix;

	/*XMFLOAT4X4A vpMatrix = { };
	
	if (!_invertedDepth)
	{
		Store(vpMatrix, XMMatrixTranspose(Load(GetViewMatrix()) * Load(GetProjectionMatrix())));
		return vpMatrix;
	}

	XMFLOAT3A r, u, f;
	GetTransform()->GetAxes(&r, &u, &f, World);

	XMVECTOR
		posVec = Load(GetTransform()->GetPosition(World)),
		up = Load(u),
		forward = Load(f);

	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		posVec,
		posVec + forward,
		up
	);

	XMMATRIX projectionMatrix;
	if (_ortho)
	{
		projectionMatrix = XMMatrixOrthographicLH(
			_currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			_currProjInfo.fovAngleY,
			_currProjInfo.planes.farZ,
			_currProjInfo.planes.nearZ
		);
	}
	else
	{
		projectionMatrix = XMMatrixPerspectiveFovLH(
			_currProjInfo.fovAngleY,
			_currProjInfo.aspectRatio,
			_currProjInfo.planes.farZ,
			_currProjInfo.planes.nearZ
		);
	}

	Store(vpMatrix, XMMatrixTranspose(viewMatrix * projectionMatrix));
	return vpMatrix;*/
}
const ProjectionInfo &CameraBehaviour::GetCurrProjectionInfo() const
{
	return _currProjInfo;
}

bool CameraBehaviour::ScaleToContents(const std::vector<XMFLOAT4A> &nearBounds, const std::vector<XMFLOAT4A> &innerBounds)
{
	if (!_ortho)
		return false;

	XMFLOAT3A f, r, u;
	GetTransform()->GetAxes(&r, &u, &f);

	const XMVECTOR
		forward = Load(f),
		right = Load(r),
		up = Load(u);

	XMVECTOR mid = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (const XMFLOAT4A &point : innerBounds)
		mid = XMVectorAdd(mid, Load(point));
	mid = XMVectorScale(mid, 1.0f / static_cast<float>(innerBounds.size()));

	float
		nearDist = FLT_MAX, farDist = -FLT_MAX,
		leftDist = FLT_MAX, rightDist = -FLT_MAX,
		downDist = FLT_MAX, upDist = -FLT_MAX;

	float
		sceneFarDist = -FLT_MAX,
		sceneLeftDist = FLT_MAX, sceneRightDist = -FLT_MAX,
		sceneDownDist = FLT_MAX, sceneUpDist = -FLT_MAX;

	for (const XMFLOAT4A &point : nearBounds)
	{
		const XMVECTOR pointVec = Load(point);
		const XMVECTOR toPoint = pointVec - mid;

		const float
			xDot = XMVectorGetX(XMVector3Dot(toPoint, right)),
			yDot = XMVectorGetX(XMVector3Dot(toPoint, up)),
			zDot = XMVectorGetX(XMVector3Dot(toPoint, forward));

		if (xDot < sceneLeftDist)	sceneLeftDist = xDot;
		if (xDot > sceneRightDist)	sceneRightDist = xDot;

		if (yDot < sceneDownDist)	sceneDownDist = yDot;
		if (yDot > sceneUpDist)		sceneUpDist = yDot;

		if (zDot < nearDist)		nearDist = zDot;
		if (zDot > sceneFarDist)	sceneFarDist = zDot;
	}

	for (const XMFLOAT4A &point : innerBounds)
	{
		const XMVECTOR pointVec = Load(point);
		const XMVECTOR toPoint = pointVec - mid;

		const float
			xDot = XMVectorGetX(XMVector3Dot(toPoint, right)),
			yDot = XMVectorGetX(XMVector3Dot(toPoint, up)),
			zDot = XMVectorGetX(XMVector3Dot(toPoint, forward));

		if (xDot < leftDist)	leftDist = xDot;
		if (xDot > rightDist)	rightDist = xDot;

		if (yDot < downDist)	downDist = yDot;
		if (yDot > upDist)		upDist = yDot;

		if (zDot > farDist)		farDist = zDot;
	}

	if (sceneLeftDist > leftDist)	leftDist = sceneLeftDist;
	if (sceneRightDist < rightDist)	rightDist = sceneRightDist;
	if (sceneDownDist > downDist)	downDist = sceneDownDist;
	if (sceneUpDist < upDist)		upDist = sceneUpDist;

	if (farDist - nearDist < 0.001f)
	{
		DbgMsg("Near and far planes are very close, camera can likely be disabled.");
		return true;
	}

	XMVECTOR newPosVec = XMVectorAdd(mid, XMVectorScale(forward, nearDist - 1.0f));
	newPosVec = XMVectorAdd(newPosVec, XMVectorScale(right, (rightDist + leftDist) * 0.5f));
	newPosVec = XMVectorAdd(newPosVec, XMVectorScale(up, (upDist + downDist) * 0.5f));

	XMFLOAT4A newPos{};
	Store(newPos, newPosVec);

	GetTransform()->SetPosition(newPos);

	const float
		nearZ = 1.0f,
		farZ = (farDist - nearDist) + 1.0f,
		width = (rightDist - leftDist) * 0.5f,
		height = (upDist - downDist) * 0.5f;

	const XMFLOAT3 corners[8] = {
		XMFLOAT3(-width, -height, nearZ),
		XMFLOAT3(width, -height, nearZ),
		XMFLOAT3(-width,  height, nearZ),
		XMFLOAT3(width,  height, nearZ),
		XMFLOAT3(-width, -height, farZ),
		XMFLOAT3(width, -height, farZ),
		XMFLOAT3(-width,  height, farZ),
		XMFLOAT3(width,  height, farZ)
	};

	BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));

	_currProjInfo.planes.nearZ = nearZ;
	_currProjInfo.planes.farZ = farZ;
	_currProjInfo.fovAngleY = height * 2.0f;
	_currProjInfo.aspectRatio = width / height;

	_isDirty = true;
	return true;
}
bool CameraBehaviour::FitPlanesToPoints(const std::vector<XMFLOAT4A> &points)
{
	const float
		currNear = _currProjInfo.planes.nearZ,
		currFar = _currProjInfo.planes.farZ;

	const XMFLOAT3A f = GetTransform()->GetForward();
	const XMVECTOR direction = Load(f);
	const XMVECTOR origin = Load(GetTransform()->GetPosition());

	float minDist = FLT_MAX, maxDist = -FLT_MAX;
	for (const XMFLOAT4A &point : points)
	{
		const XMVECTOR pointVec = Load(point);
		const XMVECTOR toPoint = pointVec - origin;

		const float dot = XMVectorGetX(XMVector3Dot(toPoint, direction));

		if (dot < minDist)
			minDist = dot;

		if (dot > maxDist)
			maxDist = dot;
	}

	//_currProjInfo.planes.nearZ = max(minDist, _defaultProjInfo.planes.nearZ);
	_currProjInfo.planes.farZ = min(maxDist, _defaultProjInfo.planes.farZ);

	if (_currProjInfo.planes.farZ - _currProjInfo.planes.nearZ < 0.001f)
	{
		DbgMsg("Near and far planes are very close, camera could be ignored.");
		return true;
	}

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	if (abs(currNear - _currProjInfo.planes.nearZ) + abs(currFar - _currProjInfo.planes.farZ) > 0.01f)
	{
		_isDirty = true;
		_recalculateBounds = true;
		_lightGrid.clear();
	}
	return true;
}

bool CameraBehaviour::UpdateBuffers()
{
	if (!_isDirty)
		return true;

	auto context = GetScene()->GetContext();

	const XMFLOAT4X4A viewProjMatrix = GetViewProjectionMatrix();
	if (!_viewProjBuffer.UpdateBuffer(context, &viewProjMatrix))
		Warn("Failed to update camera view projection buffer!");

	const XMFLOAT4X4A projMatrix = GetProjectionMatrix(_invertedDepth);

	XMFLOAT4X4 invProjMatrix;
	if (_invCamBuffer != nullptr)
	{
		XMFLOAT4X4 invCamBufferData[2] = { };
		Store(invCamBufferData[0], XMMatrixInverse(nullptr, Load(projMatrix)));
		Store(invCamBufferData[1], XMMatrixInverse(nullptr, Load(GetViewMatrix())));
		invProjMatrix = invCamBufferData[0];

		if (!_invCamBuffer->UpdateBuffer(context, &invCamBufferData))
			Warn("Failed to update inverse camera buffer!");
	}

	if (_viewProjPosBuffer != nullptr)
	{
		XMFLOAT3A pos3 = GetTransform()->GetPosition();
		const GeometryBufferData bufferData = { viewProjMatrix, To4(pos3) };
		if (!_viewProjPosBuffer->UpdateBuffer(context, &bufferData))
			Warn("Failed to update camera view projection positon buffer!");
	}

	if (_posBuffer != nullptr)
	{
		if (!_invCamBuffer)
			Store(invProjMatrix, XMMatrixInverse(nullptr, Load(projMatrix)));

		XMFLOAT4X4 invViewProjBufferData = { };
		Store(invViewProjBufferData, XMMatrixInverse(nullptr, Load(viewProjMatrix)));

		CameraBufferData camBufferData = { 
			projMatrix,
			viewProjMatrix,
			invViewProjBufferData,
			invProjMatrix,
			GetTransform()->GetPosition(World),
			GetTransform()->GetForward(World),
			_invertedDepth ? _currProjInfo.planes.farZ : _currProjInfo.planes.nearZ,
			_invertedDepth ? _currProjInfo.planes.nearZ : _currProjInfo.planes.farZ
		};

		if (!_posBuffer->UpdateBuffer(context, &camBufferData))
			Warn("Failed to update camera position buffer!");
	}

	_isDirty = false;
	return true;
}

bool CameraBehaviour::BindDebugDrawBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const camViewProjBuffer = GetCameraVSBuffer();
	if (camViewProjBuffer == nullptr)
	{
		Warn("Failed to bind vertex buffer, camera does not have that buffer!");
		return true;
	}
	context->VSSetConstantBuffers(0, 1, &camViewProjBuffer);

	ID3D11Buffer *const camViewPosBuffer = GetCameraGSBuffer();
	if (camViewPosBuffer == nullptr)
	{
		Warn("Failed to bind geometry buffer, camera does not have that buffer!");
		return true;
	}
	context->GSSetConstantBuffers(0, 1, &camViewPosBuffer);

	return true;
}
bool CameraBehaviour::BindShadowCasterBuffers() const
{
	return BindViewBuffers();
}
bool CameraBehaviour::BindPSLightingBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const camPosBuffer = GetCameraCSBuffer();
	if (camPosBuffer == nullptr)
	{
		Warn("Failed to bind PS lighting buffer, camera does not have that buffer!");
		return true;
	}
	context->PSSetConstantBuffers(3, 1, &camPosBuffer);

	return true;
}
bool CameraBehaviour::BindCSLightingBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const camPosBuffer = GetCameraCSBuffer();
	if (camPosBuffer == nullptr)
	{
		Warn("Failed to bind CS lighting buffer, camera does not have that buffer!");
		return true;
	}
	context->CSSetConstantBuffers(3, 1, &camPosBuffer);

	return true;
}
bool CameraBehaviour::BindTransparentBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const camViewPosBuffer = GetCameraGSBuffer();
	if (camViewPosBuffer == nullptr)
	{
		Warn("Failed to bind geometry buffer, camera does not have that buffer!");
		return true;
	}
	context->GSSetConstantBuffers(0, 1, &camViewPosBuffer);

	if (!BindPSLightingBuffers())
	{
		Warn("Failed to bind lighting buffer, camera does not have that buffer!");
		return true;
	}

	return BindViewBuffers();
}
bool CameraBehaviour::BindViewBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const vpmBuffer = GetCameraVSBuffer();
	context->VSSetConstantBuffers(1, 1, &vpmBuffer);

	return true;
}
bool CameraBehaviour::BindInverseBuffers() const
{
	auto context = GetScene()->GetContext();

	ID3D11Buffer *const invBuffer = _invCamBuffer->GetBuffer();
	context->CSSetConstantBuffers(4, 1, &invBuffer);

	return true;
}

ID3D11Buffer *const CameraBehaviour::GetInverseBuffers()
{
	return _invCamBuffer->GetBuffer();
}

bool CameraBehaviour::StoreBounds(BoundingFrustum &bounds, bool includeScale)
{
	if (_ortho)
		return false;

	if (_recalculateBounds || _transformedWithScale != includeScale)
	{
		XMFLOAT4X4A worldMatrix;

		if (includeScale)
			worldMatrix = GetTransform()->GetWorldMatrix();
		else
			worldMatrix = GetTransform()->GetUnscaledWorldMatrix();

		_bounds.perspective.Transform(_transformedBounds.perspective, Load(worldMatrix));

		_recalculateBounds = false;
		_transformedWithScale = includeScale;
	}

	bounds = _transformedBounds.perspective;
	return true;
}
bool CameraBehaviour::StoreBounds(BoundingOrientedBox &bounds, bool includeScale)
{
	if (!_ortho)
		return false;

	if (_recalculateBounds || _transformedWithScale != includeScale)
	{
		XMFLOAT4X4A worldMatrix;

		if (includeScale)
			worldMatrix = GetTransform()->GetWorldMatrix();
		else
			worldMatrix = GetTransform()->GetUnscaledWorldMatrix();

		_bounds.ortho.Transform(_transformedBounds.ortho, Load(worldMatrix));

		_recalculateBounds = false;
		_transformedWithScale = includeScale;
	}

	bounds = _transformedBounds.ortho;
	return true;
}

const CamBounds *CameraBehaviour::GetLightGridBounds()
{
	ZoneScopedXC(RandomUniqueColor());

	if (_lightGrid.empty())
	{
		if (_ortho)
		{
			Graphics *graphics = GetScene()->GetGraphics();
			const UINT lightTileCount = LIGHT_GRID_RES * LIGHT_GRID_RES;

			_lightGrid.resize(lightTileCount);
			_transformedLightGrid.resize(lightTileCount);

			XMMATRIX projMatrix = Load(GetProjectionMatrix());

			float nearZ = LIGHT_CULLING_NEAR_PLANE; //_currProjInfo.planes.nearZ;
			float farZ = _currProjInfo.planes.farZ;

			// Extract frustum planes at near plane (assuming symmetric perspective projection)
			float m11 = XMVectorGetX(projMatrix.r[0]);
			float m22 = XMVectorGetY(projMatrix.r[1]);

			float r = nearZ / m11;	// Right plane at nearZ
			float t = nearZ / m22;	// Top plane
			float l = -r;			// Left plane
			float b = -t;			// Bottom plane

#pragma warning(disable: 6993)
#pragma omp parallel for num_threads(PARALLEL_THREADS)
#pragma warning(default: 6993)
			for (int tileY = 0; tileY < LIGHT_GRID_RES; ++tileY)
			{
				for (int tileX = 0; tileX < LIGHT_GRID_RES; ++tileX)
				{
					// Calculate NDC boundaries for this tile
					float xMinNDC = -1.0f + (static_cast<float>(tileX) / LIGHT_GRID_RES) * 2.0f;
					float xMaxNDC = -1.0f + (static_cast<float>(tileX + 1) / LIGHT_GRID_RES) * 2.0f;
					float yMinNDC = -1.0f + (static_cast<float>(tileY) / LIGHT_GRID_RES) * 2.0f;
					float yMaxNDC = -1.0f + (static_cast<float>(tileY + 1) / LIGHT_GRID_RES) * 2.0f;

					// Convert NDC to parametric space [0, 1]
					float txMin = (xMinNDC + 1.0f) * 0.5f;
					float txMax = (xMaxNDC + 1.0f) * 0.5f;
					float tyMin = (yMinNDC + 1.0f) * 0.5f;
					float tyMax = (yMaxNDC + 1.0f) * 0.5f;

					// Calculate tile box planes in view space
					float tileL = l + (r - l) * txMin;
					float tileR = l + (r - l) * txMax;
					float tileB = b + (t - b) * tyMin;
					float tileT = b + (t - b) * tyMax;

					// Create tile projection matrix
					XMMATRIX projTile = XMMatrixOrthographicOffCenterLH(tileL, tileR, tileB, tileT, nearZ, farZ);

					// Store bounding frustum
					BoundingOrientedBox().Transform(_lightGrid[tileX + (size_t)tileY * LIGHT_GRID_RES].ortho, projTile);
				}
			}
		}
		else
		{
			Graphics *graphics = GetScene()->GetGraphics();
			const UINT lightTileCount = LIGHT_GRID_RES * LIGHT_GRID_RES;

			_lightGrid.resize(lightTileCount);
			_transformedLightGrid.resize(lightTileCount);

			XMMATRIX projMatrix = Load(GetProjectionMatrix());

			float nearZ = LIGHT_CULLING_NEAR_PLANE; //_currProjInfo.planes.nearZ;
			float farZ = _currProjInfo.planes.farZ;

			// Extract frustum planes at near plane (assuming symmetric perspective projection)
			float m11 = XMVectorGetX(projMatrix.r[0]);
			float m22 = XMVectorGetY(projMatrix.r[1]);

			float r = nearZ / m11;	// Right plane at nearZ
			float t = nearZ / m22;	// Top plane
			float l = -r;			// Left plane
			float b = -t;			// Bottom plane

#pragma warning(disable: 6993)
#pragma omp parallel for num_threads(PARALLEL_THREADS)
#pragma warning(default: 6993)
			for (int tileY = 0; tileY < LIGHT_GRID_RES; ++tileY)
			{
				for (int tileX = 0; tileX < LIGHT_GRID_RES; ++tileX)
				{
					// Calculate NDC boundaries for this tile
					float xMinNDC = -1.0f + (static_cast<float>(tileX) / LIGHT_GRID_RES) * 2.0f;
					float xMaxNDC = -1.0f + (static_cast<float>(tileX + 1) / LIGHT_GRID_RES) * 2.0f;
					float yMinNDC = -1.0f + (static_cast<float>(tileY) / LIGHT_GRID_RES) * 2.0f;
					float yMaxNDC = -1.0f + (static_cast<float>(tileY + 1) / LIGHT_GRID_RES) * 2.0f;

					// Convert NDC to parametric space [0, 1]
					float txMin = (xMinNDC + 1.0f) * 0.5f;
					float txMax = (xMaxNDC + 1.0f) * 0.5f;
					float tyMin = (yMinNDC + 1.0f) * 0.5f;
					float tyMax = (yMaxNDC + 1.0f) * 0.5f;

					// Calculate tile frustum planes in view space
					float tileL = l + (r - l) * txMin;
					float tileR = l + (r - l) * txMax;
					float tileB = b + (t - b) * tyMin;
					float tileT = b + (t - b) * tyMax;

					// Create tile projection matrix
					XMMATRIX projTile = XMMatrixPerspectiveOffCenterLH(tileL, tileR, tileB, tileT, nearZ, farZ);

					// Store bounding frustum
					BoundingFrustum::CreateFromMatrix(_lightGrid[tileX + (size_t)tileY * LIGHT_GRID_RES].perspective, projTile);
				}
			}
		}
	}

	XMMATRIX worldMatrix = Load(GetTransform()->GetUnscaledWorldMatrix());
	UINT tileCount = static_cast<UINT>(_lightGrid.size());

	if (_ortho)
	{
		for (UINT i = 0; i < tileCount; i++)
		{
			BoundingFrustum transformedTile;
			_lightGrid[i].ortho.Transform(_transformedLightGrid[i].ortho, worldMatrix);
		}
	}
	else
	{
		for (UINT i = 0; i < tileCount; i++)
		{
			BoundingFrustum transformedTile;
			_lightGrid[i].perspective.Transform(_transformedLightGrid[i].perspective, worldMatrix);
		}
	}
	
	return _transformedLightGrid.data();
}

void CameraBehaviour::QueueGeometry(const RenderQueueEntry &entry)
{
	if (entry.resourceGroup.overlay)
		_overlayRenderQueue.emplace_back(entry);
	else
		_geometryRenderQueue.emplace_back(entry);
}
void CameraBehaviour::QueueTransparent(const RenderQueueEntry &entry)
{
	_transparentRenderQueue.emplace_back(entry);
}
void CameraBehaviour::ResetRenderQueue()
{
	size_t geometryCount = _geometryRenderQueue.size();
	size_t transparentCount = _transparentRenderQueue.size();
	size_t overlayCount = _overlayRenderQueue.size();

	_lastCullCount = static_cast<UINT>(geometryCount + transparentCount + overlayCount);

	_geometryRenderQueue.clear();
	_transparentRenderQueue.clear();
	_overlayRenderQueue.clear();

	_geometryRenderQueue.reserve(geometryCount);
	_transparentRenderQueue.reserve(transparentCount);
	_overlayRenderQueue.reserve(overlayCount);
}

void CameraBehaviour::SortQueue(std::vector<RenderQueueEntry> &queue, bool reverse, int sortModeOverride) const
{
	ZoneScopedXC(RandomUniqueColor());

	// TODO: Implement more sophisticated sorting logic that reduces resource binding or overdraw.
	
	switch (sortModeOverride >= 0 ? sortModeOverride : _sortMode)
	{
	case 0: // Don't sort at all
		break;

	case 1: // Sort by material to minimize binding
		std::sort(queue.begin(), queue.end());
		break;

	case 2: // Sort by distance to camera to reduce overdraw
	{
		Transform *t = GetTransform();
		XMFLOAT3 camDir = t->GetForward(World);
		XMVECTOR dir = Load(camDir) * (reverse ? -1.0f : 1.0f);

		// Sort queue based on the entry position projected along the camera direction
		std::sort(std::execution::par, queue.begin(), queue.end(),
			[&dir](const RenderQueueEntry &a, const RenderQueueEntry &b) { // Calculate dot products and compare scalar results

				BoundingOrientedBox aBounds, bBounds;
				a.instance.subject->GetEntity()->StoreEntityBounds(aBounds, World);
				b.instance.subject->GetEntity()->StoreEntityBounds(bBounds, World);

				XMFLOAT3 aCorners[8]{}, bCorners[8]{};
				aBounds.GetCorners(aCorners);
				bBounds.GetCorners(bCorners);

				int aCornerIndex = 0;
				float aCurrDist = XMVectorGetX(XMVector3Dot(Load(aCorners[aCornerIndex]), dir));

				int bCornerIndex = 0;
				float bCurrDist = XMVectorGetX(XMVector3Dot(Load(bCorners[bCornerIndex]), dir));

				while (true)
				{
					if (aCurrDist < bCurrDist)
					{
						bCornerIndex++;
						if (bCornerIndex >= 8)
							return true; // a is closer than b

						bCurrDist = XMVectorGetX(XMVector3Dot(Load(bCorners[bCornerIndex]), dir));
					}
					else
					{
						aCornerIndex++;
						if (aCornerIndex >= 8)
							return false; // b is closer than a

						aCurrDist = XMVectorGetX(XMVector3Dot(Load(aCorners[aCornerIndex]), dir));
					}
				}

				return true;
			}
		);
		break;
	}

	default:
		break;
	}
}

void CameraBehaviour::SortGeometryQueue()
{
	ZoneScopedXC(RandomUniqueColor());

	SortQueue(_geometryRenderQueue);
}
void CameraBehaviour::SortTransparentQueue()
{
	ZoneScopedXC(RandomUniqueColor());

	SortQueue(_transparentRenderQueue, true, 2);
}
void CameraBehaviour::SortOverlayQueue()
{
	ZoneScopedXC(RandomUniqueColor());

	SortQueue(_overlayRenderQueue);
}

const std::vector<RenderQueueEntry> &CameraBehaviour::GetGeometryQueue() const
{
	return _geometryRenderQueue;
}
const std::vector<RenderQueueEntry> &CameraBehaviour::GetTransparentQueue() const
{
	return _transparentRenderQueue;
}
const std::vector<RenderQueueEntry> &CameraBehaviour::GetOverlayQueue() const
{
	return _overlayRenderQueue;
}

UINT CameraBehaviour::GetCullCount() const
{
	return _lastCullCount;
}

float CameraBehaviour::GetFOV() const
{
	return _currProjInfo.fovAngleY;
}
bool CameraBehaviour::GetOrtho() const
{
	return _ortho;
}
bool CameraBehaviour::GetInverted() const
{
	return _invertedDepth;
}
CameraPlanes CameraBehaviour::GetPlanes() const
{
	return _currProjInfo.planes;
}
float CameraBehaviour::GetAspectRatio() const
{
	return _currProjInfo.aspectRatio;
}

void CameraBehaviour::SetFOV(const float fov)
{
	_currProjInfo.fovAngleY = fov;

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	_isDirty = true;
	_recalculateBounds = true;
	_lightGrid.clear();
}
void CameraBehaviour::SetOrtho(bool state)
{
	_ortho = state;

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	_isDirty = true;
	_recalculateBounds = true;
	_lightGrid.clear();
}
void CameraBehaviour::SetInverted(bool state)
{
	_invertedDepth = state;
}
void CameraBehaviour::SetPlanes(CameraPlanes planes)
{
	planes.nearZ = max(planes.nearZ, 0.0001f);
	planes.farZ = max(planes.farZ, planes.nearZ + 0.0001f);

	_currProjInfo.planes = planes;

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	_isDirty = true;
	_recalculateBounds = true;
	_lightGrid.clear();
}
void CameraBehaviour::SetAspectRatio(float aspect)
{
	_currProjInfo.aspectRatio = aspect;

	if (_ortho)
	{
		const float
			nearZ = _currProjInfo.planes.nearZ,
			farZ = _currProjInfo.planes.farZ,
			width = 0.5f * _currProjInfo.fovAngleY * _currProjInfo.aspectRatio,
			height = 0.5f * _currProjInfo.fovAngleY;

		const XMFLOAT3 corners[8] = {
			XMFLOAT3(-width, -height, nearZ),
			XMFLOAT3(width, -height, nearZ),
			XMFLOAT3(-width,  height, nearZ),
			XMFLOAT3(width,  height, nearZ),
			XMFLOAT3(-width, -height, farZ),
			XMFLOAT3(width, -height, farZ),
			XMFLOAT3(-width,  height, farZ),
			XMFLOAT3(width,  height, farZ)
		};

		BoundingOrientedBox::CreateFromPoints(_bounds.ortho, 8, corners, sizeof(XMFLOAT3));
	}
	else
	{
		const XMFLOAT4X4A projMatrix = GetProjectionMatrix();
		BoundingFrustum::CreateFromMatrix(_bounds.perspective, *reinterpret_cast<const XMMATRIX *>(&projMatrix));
	}

	_isDirty = true;
	_recalculateBounds = true;
	_lightGrid.clear();
}

void CameraBehaviour::SetRendererInfo(const RendererInfo &rendererInfo)
{
	_rendererInfo = rendererInfo;
}
RendererInfo CameraBehaviour::GetRendererInfo() const
{
	return _rendererInfo;
}

ID3D11Buffer *CameraBehaviour::GetCameraVSBuffer() const
{
	return _viewProjBuffer.GetBuffer();
}
ID3D11Buffer *CameraBehaviour::GetCameraGSBuffer() const
{
	if (_viewProjPosBuffer == nullptr)
		return nullptr;

	return _viewProjPosBuffer->GetBuffer();
}
ID3D11Buffer *CameraBehaviour::GetCameraCSBuffer() const
{
	if (_posBuffer == nullptr)
		return nullptr;

	return _posBuffer->GetBuffer();
}

void CameraBehaviour::GetViewRay(const XMFLOAT2 &viewPos, XMFLOAT3 &origin, XMFLOAT3 &direction)
{
	// Wiewport to NDC coordinates
	float xNDC = (2.0f * viewPos.x) - 1.0f;
	float yNDC = 1.0f - (2.0f * viewPos.y); // can also be - 1 depending on coord-system
	float zNDC = 1.0f;	// not really needed yet (specified anyways)
	XMFLOAT3 ray_clip = XMFLOAT3(xNDC, yNDC, zNDC);

	// Wiew space -> clip space
	XMVECTOR rayClipVec = Load(ray_clip);
	XMMATRIX projMatrix = Load(GetProjectionMatrix());
	XMVECTOR rayEyeVec = XMVector4Transform(rayClipVec, XMMatrixInverse(nullptr, projMatrix));

	// Set z and w to mean forwards and not a point
	rayEyeVec = XMVectorSet(XMVectorGetX(rayEyeVec), XMVectorGetY(rayEyeVec), 1, 0.0f);

	// Clip space -> world space
	XMMATRIX viewMatrix = Load(GetViewMatrix());
	XMVECTOR rayWorldVec = XMVector4Transform(rayEyeVec, XMMatrixInverse(nullptr, viewMatrix));

	rayWorldVec = XMVector4Normalize(rayWorldVec);	// Normalize
	XMFLOAT3 dir; Store(dir, rayWorldVec);

	// Camera 
	XMFLOAT3 camPos = GetTransform()->GetPosition(World);

	// Perform raycast
	origin = { camPos.x, camPos.y, camPos.z };
	direction = { dir.x, dir.y, dir.z };
}

void CameraBehaviour::BeginScreenFade(float duration)
{
	_screenFadeRate = 1.0f / duration;
}
void CameraBehaviour::SetScreenFadeManual(float amount)
{
	_screenFadeAmount = amount;
}
float CameraBehaviour::GetScreenFadeAmount() const
{
	return _screenFadeAmount;
}
float CameraBehaviour::GetScreenFadeRate() const
{
	return _screenFadeRate;
}
