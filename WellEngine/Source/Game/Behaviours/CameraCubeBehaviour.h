#pragma once

#include <vector>

#include "Behaviour.h"
#include "Behaviours/CameraBehaviour.h"
#include "Content/Content.h"
#include "Rendering/RendererInfo.h"

class RenderQueuer;

struct CameraCubeBufferData
{
	dx::XMFLOAT3A position;
	float nearZ;
	float farZ;
	float padding[2];
};

class [[register_behaviour]] CameraCubeBehaviour final : public Behaviour
{
private:
	std::vector<RenderQueueEntry> _geometryRenderQueue;
	std::vector<RenderQueueEntry> _transparentRenderQueue;

	ConstantBufferD3D11 _viewProjArrayBuffer;
	std::unique_ptr<ConstantBufferD3D11> _posBuffer = nullptr;

	RendererInfo _rendererInfo;
	CameraPlanes _cameraPlanes;

	dx::BoundingBox _bounds, _transformedBounds;

	bool _hasCSBuffer = false;
	bool _recalculateFrustumBounds = true;
	bool _recalculateBounds = true;
	bool _isDirty = true;

	UINT _lastCullCount = 0;

	void GetAxes(UINT cameraIndex, dx::XMFLOAT3A *right, dx::XMFLOAT3A *up, dx::XMFLOAT3A *forward);
	[[nodiscard]] dx::XMFLOAT4A GetRotation(UINT cameraIndex);

protected:
	[[nodiscard]] bool Start() override;

	void OnDirty() override;

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI() override;
#endif

	[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

	[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

public:
	CameraCubeBehaviour() = default;
	CameraCubeBehaviour(const CameraPlanes &planes, bool hasCSBuffer);
	~CameraCubeBehaviour() = default;

	[[nodiscard]] dx::XMFLOAT4X4A GetViewMatrix(UINT cameraIndex);
	[[nodiscard]] dx::XMFLOAT4X4A GetProjectionMatrix() const;
	[[nodiscard]] dx::XMFLOAT4X4A GetViewProjectionMatrix(UINT cameraIndex);
	
	/// This must be called from the behaviour in control of the camera.
	[[nodiscard]] bool UpdateBuffers();

	[[nodiscard]] bool BindShadowCasterBuffers() const;
	[[nodiscard]] bool BindGeometryBuffers() const;
	[[nodiscard]] bool BindLightingBuffers() const;
	[[nodiscard]] bool BindTransparentBuffers() const;
	[[nodiscard]] bool BindViewBuffers() const;

	[[nodiscard]] bool StoreBounds(dx::BoundingBox &bounds);

	void QueueGeometry(const RenderQueueEntry &entry);
	void QueueTransparent(const RenderQueueEntry &entry);
	void ResetRenderQueue();

	void SortGeometryQueue();
	void SortTransparentQueue();

	[[nodiscard]] const std::vector<RenderQueueEntry> &GetGeometryQueue() const;
	[[nodiscard]] const std::vector<RenderQueueEntry> &GetTransparentQueue() const;

	[[nodiscard]] UINT GetCullCount() const;
	void SetCullCount(UINT cullCount);

	void SetRendererInfo(const RendererInfo &rendererInfo);
	void SetFarZ(float farZ);
	[[nodiscard]] RendererInfo GetRendererInfo() const;
	[[nodiscard]] float GetNearZ() const;
	[[nodiscard]] float GetFarZ() const;
	
	[[nodiscard]] ID3D11Buffer *GetCameraGSBuffer() const;
	[[nodiscard]] ID3D11Buffer *GetCameraCSBuffer() const;
};
