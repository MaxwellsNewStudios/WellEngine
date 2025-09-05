#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <functional>
#include "D3D/ConstantBufferD3D11.h"
#include "Utils/ReferenceHelper.h"

constexpr float MIN_SCALE = 0.0001f;

enum ReferenceSpace
{
	Local,
	World
};

enum TransformationType
{
	None,
	Translate,
	Rotate,
	Scale,
	Universal,
	Bounds
};

enum class TransformOriginMode
{
	None,				// Use world origin as pivot point
	Primary,			// Use primary selection as pivot point
	Center,				// Use bounding box middle as pivot point
	Average,			// Use average position as pivot point
	Separate			// Transform all entities using their own pivot
};

class Transform : public IRefTarget<Transform>
{
private:
	Transform *_parent = nullptr;
	std::vector<Transform*> _children;

	dx::XMFLOAT3A _localPosition =	{ 0, 0, 0    };
	dx::XMFLOAT4A _localRotation =	{ 0, 0, 0, 1 };
	dx::XMFLOAT3A _localScale =		{ 1, 1, 1    };
	dx::XMFLOAT4X4A _localMatrix =	{ 1, 0, 0, 0,
									  0, 1, 0, 0,
									  0, 0, 1, 0,
									  0, 0, 0, 1 };

	dx::XMFLOAT3A _worldPosition =	{ 0, 0, 0    };
	dx::XMFLOAT4A _worldRotation =	{ 0, 0, 0, 1 };
	dx::XMFLOAT3A _worldScale =		{ 1, 1, 1    };
	dx::XMFLOAT4X4A _worldMatrix =	{ 1, 0, 0, 0, 
									  0, 1, 0, 0, 
									  0, 0, 1, 0, 
									  0, 0, 0, 1 };

	ConstantBufferD3D11 _worldMatrixBuffer;

	std::vector<std::function<void()>> _dirtyCallbacks;

	bool _isDirty = true;
	bool _isScenePosDirty = true;
	bool _isWorldPositionDirty = true;	// Dirtied by parent position, rotation and scale.
	bool _isWorldRotationDirty = true;	// Dirtied by parent rotation.
	bool _isWorldScaleDirty = true;		// Dirtied by parent rotation and scale.
	bool _isWorldMatrixDirty = true;	// Dirtied by parent position, rotation and scale.
	bool _isLocalMatrixDirty = true;	// Never dirtied by parent.

	// Used if the parent is moved to a new memory address.
	inline void MoveParentMemory(Transform *parent) noexcept { _parent = parent; }

	inline void AddChild(Transform *child);
	inline void RemoveChild(Transform *child);

	void SetWorldPositionDirty();
	void SetWorldRotationDirty();
	void SetWorldScaleDirty();
	void SetAllDirty();

	void VerifyRotation();
	void VerifyScale();

	[[nodiscard]] dx::XMFLOAT3A *WorldPosition();
	[[nodiscard]] dx::XMFLOAT4A *WorldRotation();
	// Whatever it is you want to do with this, please consider using local scale.
	[[nodiscard]] dx::XMFLOAT3A *WorldScale();

	[[nodiscard]] dx::XMFLOAT4X4A *WorldMatrix();
	[[nodiscard]] dx::XMFLOAT4X4A *LocalMatrix();

	void InverseTransformPointInPlace(dx::XMFLOAT3A &point) const;
	[[nodiscard]] const dx::XMFLOAT3A InverseTransformPoint(dx::XMFLOAT3A point) const;
	[[nodiscard]] const dx::XMFLOAT4X4A GetWorldRotationAndScale();

public:
	Transform() = default;
	Transform(ID3D11Device *device, const dx::XMFLOAT4X4A &worldMatrix);
	~Transform();
	Transform(const Transform &other) = delete;
	Transform &operator=(const Transform &other) = delete;
	Transform &operator=(Transform &&other) noexcept; // Move assignment operator
	Transform(Transform &&other) noexcept; // Move constructor

	[[nodiscard]] bool Initialize(ID3D11Device *device);
	[[nodiscard]] bool Initialize(ID3D11Device *device, const dx::XMFLOAT4X4A &worldMatrix);

	void SetDirty();
	[[nodiscard]] bool IsDirty() const;
	bool IsScenePosDirty() const;
	void AddDirtyCallback(std::function<void(void)> callback);
	void CleanScenePos();

	[[nodiscard]] Transform *GetParent() const;
	void SetParent(Transform *parent, bool worldPositionStays = false);

	[[nodiscard]] dx::XMFLOAT3A GetRight(ReferenceSpace space = Local);
	[[nodiscard]] dx::XMFLOAT3A GetUp(ReferenceSpace space = Local);
	[[nodiscard]] dx::XMFLOAT3A GetForward(ReferenceSpace space = Local);
	void GetAxes(dx::XMFLOAT3A *right, dx::XMFLOAT3A *up, dx::XMFLOAT3A *forward, ReferenceSpace space = Local);

	[[nodiscard]] const dx::XMFLOAT3A &GetPosition(ReferenceSpace space = Local);
	[[nodiscard]] const dx::XMFLOAT4A &GetRotation(ReferenceSpace space = Local);
	[[nodiscard]] const dx::XMFLOAT3A &GetScale(ReferenceSpace space = Local);

	void SetPosition(const dx::XMFLOAT3A &position, ReferenceSpace space = Local);
	void SetPosition(const dx::XMFLOAT4A &position, ReferenceSpace space = Local);
	void SetRotation(const dx::XMFLOAT4A &rotation, ReferenceSpace space = Local);
	void SetScale(const dx::XMFLOAT3A &scale, ReferenceSpace space = Local);
	void SetScale(const dx::XMFLOAT4A &scale, ReferenceSpace space = Local);
	void SetMatrix(const dx::XMFLOAT4X4A &mat, ReferenceSpace space = Local);

	void Move(const dx::XMFLOAT3A &direction, ReferenceSpace space = Local);
	void Move(const dx::XMFLOAT4A &direction, ReferenceSpace space = Local);
	void Rotate(const dx::XMFLOAT3A &euler, ReferenceSpace space = Local);
	void Rotate(const dx::XMFLOAT4A &euler, ReferenceSpace space = Local);
	void Scale(const dx::XMFLOAT3A &scale, ReferenceSpace space = Local, bool additive = true);
	void Scale(const dx::XMFLOAT4A &scale, ReferenceSpace space = Local, bool additive = true);

	void MoveRelative(const dx::XMFLOAT3A &direction, ReferenceSpace space = Local);
	void MoveRelative(const dx::XMFLOAT4A &direction, ReferenceSpace space = Local);
	void RotateAxis(const dx::XMFLOAT3A &axis, float amount, ReferenceSpace space = Local);
	void RotateAxis(const dx::XMFLOAT4A &axis, float amount, ReferenceSpace space = Local);
	void RotateQuaternion(const dx::XMFLOAT4A &quaternion, ReferenceSpace space = Local);

	[[nodiscard]] const dx::XMFLOAT3A GetEuler(ReferenceSpace space = Local);
	void SetEuler(const dx::XMFLOAT3A &pitchYawRoll, ReferenceSpace space = Local);
	void SetEuler(const dx::XMFLOAT4A &pitchYawRoll, ReferenceSpace space = Local);

	void SetLookDir(const dx::XMFLOAT3A &forward, const dx::XMFLOAT3A &up, ReferenceSpace space = Local);
	void SetLookDir(const dx::XMFLOAT3A &forward, ReferenceSpace space = Local);
	void LookAt(const dx::XMFLOAT3A &point);
	void LookAt(const dx::XMFLOAT3A &point, const dx::XMFLOAT3A &up);

	void RotatePitch(float amount, ReferenceSpace space = Local);
	void RotateYaw(float amount, ReferenceSpace space = Local);
	void RotateRoll(float amount, ReferenceSpace space = Local);

	[[nodiscard]] bool UpdateConstantBuffer(ID3D11DeviceContext *context);
	[[nodiscard]] ID3D11Buffer *GetConstantBuffer() const;
	[[nodiscard]] const dx::XMFLOAT4X4A &GetLocalMatrix();
	[[nodiscard]] const dx::XMFLOAT4X4A &GetWorldMatrix();
	[[nodiscard]] const dx::XMFLOAT4X4A GetUnscaledWorldMatrix();
	[[nodiscard]] const dx::XMFLOAT4X4A &GetMatrix(ReferenceSpace space);

#ifdef USE_IMGUI
	[[nodiscard]] bool RenderUI(ReferenceSpace space);
#endif

	TESTABLE()
};