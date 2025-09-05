#pragma once

#include <vector>
#include <map>
#include <d3d11.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include "Behaviour.h"
#include "Content/Content.h"
#include "Rendering/RendererInfo.h"

struct CamBounds
{
	union
	{
		dx::BoundingFrustum perspective = {};
		dx::BoundingOrientedBox ortho;
	};
};

struct CameraPlanes
{
	float nearZ = 0.1f;
	float farZ = 50.0f;
};

struct ProjectionInfo
{
	float fovAngleY = 80.0f * (dx::XM_PI / 180.0f);
	float aspectRatio = 1.0f;
	CameraPlanes planes;
};

struct CameraBufferData
{
	dx::XMFLOAT4X4 projMatrix;
	dx::XMFLOAT4X4 viewProjMatrix;
	dx::XMFLOAT4X4 invViewProjMatrix;
	dx::XMFLOAT4X4 invProjMatrix;
	dx::XMFLOAT3A position;
	dx::XMFLOAT3A direction;
	float nearZ;
	float farZ;

	float _padding[2];
};

struct GeometryBufferData
{
	dx::XMFLOAT4X4A viewMatrix;
	dx::XMFLOAT4A position;
};

struct ResourceGroup
{
	const Material *material = nullptr;
	UINT meshID = CONTENT_NULL;
	UINT blendStateID = CONTENT_NULL;
	bool shadowCaster = true;
	bool overlay = false;
	bool shadowsOnly = false;

	ResourceGroup(UINT meshID, const Material *material, bool shadowCaster, bool overlay, bool shadowsOnly = false, UINT blendStateID = CONTENT_NULL) :
		meshID(meshID), material(material), shadowCaster(shadowCaster), overlay(overlay), shadowsOnly(shadowsOnly), blendStateID(blendStateID) { }

	bool operator==(const ResourceGroup &other) const
	{
		return (material == other.material) && (meshID == other.meshID);
	}
	bool operator<(const ResourceGroup &other) const
	{
		if (material != other.material)
			return (*material) < (*other.material);

		return meshID < other.meshID;
	}
}; 

struct RenderInstance
{
	Behaviour *subject;
	size_t subjectSize;
};

struct RenderQueueEntry
{
	ResourceGroup resourceGroup;
	RenderInstance instance;

	RenderQueueEntry(const ResourceGroup &resourceGroup, RenderInstance instance) : resourceGroup(resourceGroup), instance(instance) { }
	RenderQueueEntry(const RenderQueueEntry &other) : resourceGroup(other.resourceGroup), instance(other.instance) { }

	bool operator==(const RenderQueueEntry &other) const
	{
		return resourceGroup == other.resourceGroup;
	}
	bool operator<(const RenderQueueEntry &other) const
	{
		return resourceGroup < other.resourceGroup;
	}
	bool operator>(const RenderQueueEntry &other) const
	{
		return !(resourceGroup < other.resourceGroup || resourceGroup == other.resourceGroup);
	}
};

class [[register_behaviour]] CameraBehaviour final : public Behaviour, public IRefTarget<CameraBehaviour>
{
private:
	RendererInfo _rendererInfo;
	ProjectionInfo _defaultProjInfo, _currProjInfo;
	bool 
		_ortho = false,
		_invertedDepth = false;

	int _sortMode = 1;

	CamBounds _bounds;
	CamBounds _transformedBounds;
	bool _recalculateBounds = true;
	bool _transformedWithScale = false;
	bool _debugDraw = false;
	bool _overlayDraw = false;

	std::vector<CamBounds> _lightGrid = {};
	std::vector<CamBounds> _transformedLightGrid = {};

	ConstantBufferD3D11 _viewProjBuffer;
	std::unique_ptr<ConstantBufferD3D11> _viewProjPosBuffer = nullptr;
	std::unique_ptr<ConstantBufferD3D11> _invCamBuffer = nullptr;
	std::unique_ptr<ConstantBufferD3D11> _posBuffer = nullptr;
	bool _isDirty = true;

	std::vector<RenderQueueEntry> _geometryRenderQueue; 
	std::vector<RenderQueueEntry> _transparentRenderQueue;
	std::vector<RenderQueueEntry> _overlayRenderQueue;

	UINT _lastCullCount = 0;

	float _screenFadeAmount = 0.0f; /// Screen is black at 1.0f, normal at 0.0f. Use for transition fades.
	float _screenFadeRate = 0.0f;

	void SortQueue(std::vector<RenderQueueEntry> &queue, bool reverse = false, int sortModeOverride = -1) const;

protected:
	[[nodiscard]] bool Start() override;
	[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

#ifdef USE_IMGUI
	// RenderUI runs every frame during ImGui rendering if the entity is selected.
	[[nodiscard]] bool RenderUI() override;
#endif

	// OnDirty runs when the Entity's transform is modified.
	void OnDirty() override;

public:
	CameraBehaviour() = default;
	CameraBehaviour(const ProjectionInfo &projectionInfo, bool isOrtho = false, bool invertDepth = true);
	~CameraBehaviour() = default;

	[[nodiscard]] dx::XMFLOAT4X4A GetViewMatrix();
	[[nodiscard]] dx::XMFLOAT4X4A GetProjectionMatrix(bool invert = false) const;
	[[nodiscard]] dx::XMFLOAT4X4A GetViewProjectionMatrix();
	[[nodiscard]] const ProjectionInfo &GetCurrProjectionInfo() const;

	[[nodiscard]] bool ScaleToContents(const std::vector<dx::XMFLOAT4A> &nearBounds, const std::vector<dx::XMFLOAT4A> &innerBounds);
	[[nodiscard]] bool FitPlanesToPoints(const std::vector<dx::XMFLOAT4A> &points);
	/// This must be called from the behaviour in control of the camera.
	[[nodiscard]] bool UpdateBuffers();

	[[nodiscard]] bool BindDebugDrawBuffers() const;
	[[nodiscard]] bool BindShadowCasterBuffers() const;
	[[nodiscard]] bool BindPSLightingBuffers() const;
	[[nodiscard]] bool BindCSLightingBuffers() const;
	[[nodiscard]] bool BindTransparentBuffers() const;
	[[nodiscard]] bool BindViewBuffers() const;
	[[nodiscard]] bool BindInverseBuffers() const;

	[[nodiscard]] ID3D11Buffer *const GetInverseBuffers();

	[[nodiscard]] bool StoreBounds(dx::BoundingFrustum &bounds, bool includeScale);
	[[nodiscard]] bool StoreBounds(dx::BoundingOrientedBox &bounds, bool includeScale);

	[[nodiscard]] const CamBounds *GetLightGridBounds();

	void QueueGeometry(const RenderQueueEntry &entry);
	void QueueTransparent(const RenderQueueEntry &entry);
	void ResetRenderQueue();

	void SortGeometryQueue();
	void SortTransparentQueue();
	void SortOverlayQueue();

	[[nodiscard]] const std::vector<RenderQueueEntry> &GetGeometryQueue() const;
	[[nodiscard]] const std::vector<RenderQueueEntry> &GetTransparentQueue() const;
	[[nodiscard]] const std::vector<RenderQueueEntry> &GetOverlayQueue() const;

	[[nodiscard]] UINT GetCullCount() const;

	[[nodiscard]] float GetFOV() const;
	[[nodiscard]] bool GetOrtho() const;
	[[nodiscard]] bool GetInverted() const;
	[[nodiscard]] CameraPlanes GetPlanes() const;
	[[nodiscard]] float GetAspectRatio() const;

	void SetFOV(float fov);
	void SetOrtho(bool state);
	void SetInverted(bool state);
	void SetPlanes(CameraPlanes planes);
	void SetAspectRatio(float aspect);

	void SetRendererInfo(const RendererInfo &rendererInfo);
	[[nodiscard]] RendererInfo GetRendererInfo() const;

	[[nodiscard]] ID3D11Buffer *GetCameraVSBuffer() const;
	[[nodiscard]] ID3D11Buffer *GetCameraGSBuffer() const;
	[[nodiscard]] ID3D11Buffer *GetCameraCSBuffer() const;

	// Returns a world-space ray from the camera in the direction of the screen position.
	void GetViewRay(
		/* in */ const dx::XMFLOAT2 &viewPos,
		/* out */ dx::XMFLOAT3 &origin, dx::XMFLOAT3 &direction);

	/// Begins a screen fade with the specified duration.
	/// Set to positive to fade to black.
	/// Set to negative to fade back from black.
	void BeginScreenFade(float duration);
	/// Manually sets the screen fade amount to a constant value.
	void SetScreenFadeManual(float amount);
	[[nodiscard]] float GetScreenFadeAmount() const;
	[[nodiscard]] float GetScreenFadeRate() const;

	// Serializes the behaviour to a string.
	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	// Deserializes the behaviour from a string.
	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

};
