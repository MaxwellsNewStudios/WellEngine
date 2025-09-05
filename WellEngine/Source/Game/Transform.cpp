#include "stdafx.h"
#include "Transform.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

Transform::Transform(ID3D11Device *device, const XMFLOAT4X4A &worldMatrix)
{
	if (!Initialize(device, worldMatrix))
	{
		Warn("Failed to initialize transform from constructor!");
	}
}
Transform::~Transform()
{
	for (auto child : _children)
	{
		if (child != nullptr)
			child->SetParent(nullptr);
	}
	_children.clear();

	if (_parent)
		_parent->RemoveChild(this);
}

Transform &Transform::operator=(Transform &&other) noexcept
{
	if (_worldMatrixBuffer.GetBuffer())
		ErrMsg("Can only move transforms to transforms with default values!");

	_parent = other._parent; other._parent = nullptr;
	_children = std::move(other._children);

	_localPosition = std::move(other._localPosition);
	_localRotation = std::move(other._localRotation);
	_localScale = std::move(other._localScale);
	_localMatrix = std::move(other._localMatrix);

	_worldPosition = std::move(other._worldPosition);
	_worldRotation = std::move(other._worldRotation);
	_worldScale = std::move(other._worldScale);
	_worldMatrix = std::move(other._worldMatrix);

	_worldMatrixBuffer = std::move(other._worldMatrixBuffer);
	_dirtyCallbacks = std::move(other._dirtyCallbacks);

	_isDirty = other._isDirty;
	_isScenePosDirty = other._isScenePosDirty;
	_isWorldPositionDirty = other._isWorldPositionDirty;
	_isWorldRotationDirty = other._isWorldRotationDirty;
	_isWorldScaleDirty = other._isWorldScaleDirty;
	_isWorldMatrixDirty = other._isWorldMatrixDirty;
	_isLocalMatrixDirty = other._isLocalMatrixDirty;

	for (auto child : _children)
		child->MoveParentMemory(this);

	return *this;
}
Transform::Transform(Transform &&other) noexcept
{
	*this = std::move(other); // Use move assignment operator
}

bool Transform::Initialize(ID3D11Device *device, const XMFLOAT4X4A &worldMatrix)
{
	XMVECTOR s, r, p;

	XMMatrixDecompose(&s, &r, &p, Load(worldMatrix));

	Store(_localScale, s);
	Store(_localRotation, r);
	Store(_localPosition, p);
	VerifyRotation();
	VerifyScale();

	const XMMATRIX transposeWorldMatrix = XMMatrixTranspose(Load(*WorldMatrix()));
	const XMMATRIX worldMatrixData[2] = {
		transposeWorldMatrix,
		XMMatrixTranspose(XMMatrixInverse(nullptr, transposeWorldMatrix)),
	};

	if (!_worldMatrixBuffer.Initialize(device, sizeof(XMMATRIX) * 2, &worldMatrixData))
		Warn("Failed to initialize world matrix buffer!");

	SetDirty();
	return true;
}
bool Transform::Initialize(ID3D11Device *device)
{
	if (!Initialize(device, GetWorldMatrix()))
		Warn("Failed to initialize world matrix buffer!");

	return true;
}

inline void Transform::AddChild(Transform *child)
{
	if (!child)
		return;

	if (!_children.empty())
	{
		auto it = std::find(_children.begin(), _children.end(), child);

		if (it != _children.end())
			return;
	}

	_children.emplace_back(child);
}
inline void Transform::RemoveChild(Transform *child)
{
    if (!child)
        return;

    if (_children.empty())
        return;

    auto it = std::find(_children.begin(), _children.end(), child);

    if (it != _children.end())
        _children.erase(it);
}

void Transform::SetWorldPositionDirty()
{
	_isWorldPositionDirty = true;
	_isWorldMatrixDirty = true;
	_isScenePosDirty = true;
	_isDirty = true;

	for (auto &callback : _dirtyCallbacks)
		callback();

	for (auto child : _children)
	{
		child->SetWorldPositionDirty();
	}
}
void Transform::SetWorldRotationDirty()
{
	_isWorldRotationDirty = true;
	_isWorldMatrixDirty = true;
	_isScenePosDirty = true;
	_isDirty = true;

	for (auto &callback : _dirtyCallbacks)
		callback();

	for (auto child : _children)
	{
		child->_isWorldPositionDirty = true;
		child->_isWorldScaleDirty = true;
		child->SetWorldRotationDirty();
	}
}
void Transform::SetWorldScaleDirty()
{
	_isWorldScaleDirty = true;
	_isWorldMatrixDirty = true;
	_isScenePosDirty = true;
	_isDirty = true;

	for (auto &callback : _dirtyCallbacks)
		callback();

	for (auto child : _children)
	{
		child->_isWorldPositionDirty = true;
		child->SetWorldScaleDirty();
	}
}
void Transform::SetAllDirty()
{
	_isWorldPositionDirty = true;
	_isWorldRotationDirty = true;
	_isWorldScaleDirty = true;
	_isLocalMatrixDirty = true;
	_isWorldMatrixDirty = true;
	_isScenePosDirty = true;
	_isDirty = true;

	for (auto &callback : _dirtyCallbacks)
		callback();

	for (auto child : _children)
	{
		child->SetAllDirty();
	}
}
void Transform::SetDirty()
{
	_isDirty = true;
	_isScenePosDirty = true;

	for (auto &callback : _dirtyCallbacks)
		callback();

	for (auto child : _children)
		child->SetDirty();
}
bool Transform::IsDirty() const
{
	return _isDirty;
}
bool Transform::IsScenePosDirty() const
{
	return _isScenePosDirty;
}
void Transform::AddDirtyCallback(std::function<void(void)> callback)
{
	_dirtyCallbacks.emplace_back(callback);
}

void Transform::CleanScenePos()
{
	_isScenePosDirty = false;
}

void Transform::VerifyRotation()
{
	XMVECTOR rot = Load(_localRotation);
	rot = XMQuaternionNormalize(rot);
	Store(_localRotation, rot);
}
void Transform::VerifyScale()
{
	for (int d = 0; d < 3; d++)
	{
		float &val = ((float *)&_localScale.x)[d];
		if (std::abs(val) < MIN_SCALE)
			val = MIN_SCALE * (std::signbit(val) ? -1.0f : 1.0f);
	}
}

XMFLOAT3A *Transform::WorldPosition()
{
	if (!_isWorldPositionDirty)
		return &_worldPosition;
	
	// Recalculate
	XMVECTOR worldPos = Load(_localPosition);

	Transform *iter = _parent;
	while (iter != nullptr) 
	{
		worldPos = XMVectorMultiply(worldPos, Load(iter->_localScale));
		worldPos = XMVector3Rotate(worldPos, Load(iter->_localRotation));
		worldPos = XMVectorAdd(worldPos, Load(iter->_localPosition));
		iter = iter->_parent;
	}

	Store(_worldPosition, worldPos);

	_isWorldPositionDirty = false;
	return &_worldPosition;
}
XMFLOAT4A *Transform::WorldRotation()
{
 	if (!_isWorldRotationDirty)
		return &_worldRotation;

	if (!_parent)
	{
		_worldRotation = _localRotation;
		_isWorldRotationDirty = false;
		return &_worldRotation;
	}

	// Recalculate
	XMMATRIX worldMatrix = Load(WorldMatrix());
	XMVECTOR scale, rot, pos;
	XMMatrixDecompose(&scale, &rot, &pos, worldMatrix);
	Store(_worldRotation, rot);
	
	// More optimized(?), but doesn't work
	/*XMVECTOR rotation = Load(_localRotation);
	Transform *iterator = _parent;
	while (iterator)
	{
		rotation = XMQuaternionMultiply(Load(iterator->_localRotation), rotation);
		iterator = iterator->_parent;
	}*/

	_isWorldRotationDirty = false;
	return &_worldRotation;
}
XMFLOAT3A *Transform::WorldScale()
{
	if (!_isWorldScaleDirty)
		return &_worldScale;

	// Recalculate
	XMMATRIX worldMatrix = Load(WorldMatrix());
	Store(_worldScale.x, XMVector3Length(worldMatrix.r[0]));
	Store(_worldScale.y, XMVector3Length(worldMatrix.r[1]));
	Store(_worldScale.z, XMVector3Length(worldMatrix.r[2]));

	_isWorldScaleDirty = false;
	return &_worldScale;
}
XMFLOAT4X4A *Transform::WorldMatrix()
{
	if (!_isWorldMatrixDirty)
		return &_worldMatrix;
	
	// Recalculate
	if (!_parent)
		_worldMatrix = *LocalMatrix();
	else
		Store(_worldMatrix, XMMatrixMultiply(Load(LocalMatrix()), Load(_parent->WorldMatrix())));

	_isWorldMatrixDirty = false;
	return &_worldMatrix;
}
XMFLOAT4X4A *Transform::LocalMatrix()
{
	if (!_isLocalMatrixDirty)
		return &_localMatrix;

	// Recalculate
	XMVECTOR localRot = Load(_localRotation);

	XMVECTOR xV, yV, zV;
	xV = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), localRot);
	yV = XMVector3Rotate(XMVectorSet(0, 1, 0, 0), localRot);
	zV = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), localRot);

	XMFLOAT3A x, y, z;
	Store(x, XMVectorScale(xV, _localScale.x));
	Store(y, XMVectorScale(yV, _localScale.y));
	Store(z, XMVectorScale(zV, _localScale.z));

	const XMFLOAT3A t = _localPosition;
	_localMatrix = XMFLOAT4X4A(
		x.x, x.y, x.z, 0,
		y.x, y.y, y.z, 0,
		z.x, z.y, z.z, 0,
		t.x, t.y, t.z, 1
	);

	_isLocalMatrixDirty = false;
	return &_localMatrix;
}

Transform *Transform::GetParent() const
{
	return _parent;
}
void Transform::SetParent(Transform *newParent, bool keepWorldTransform)
{
    if (_parent == newParent)
        return;

    XMFLOAT3A oldPos;
    XMFLOAT4A oldRot;
    XMFLOAT3A oldScale;
	if (keepWorldTransform)
	{
		oldPos = GetPosition(World);
		oldRot = GetRotation(World);
		oldScale = GetScale(World);
	}

    if (_parent)
        _parent->RemoveChild(this);

    _parent = newParent;

    if (newParent)
        newParent->AddChild(this);

    if (keepWorldTransform)
	{
		SetPosition(oldPos, World);
		SetRotation(oldRot, World);
		SetScale(oldScale, World);
	}

    SetAllDirty();
}

XMFLOAT3A Transform::GetRight(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
	{
		auto *v = LocalMatrix()->m[0];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A right;
		Store(right, vec);
		return right;
	}

	case World:
	{
		auto* v = WorldMatrix()->m[0];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A right;
		Store(right, vec);
		return right;
	}

	default:
		Warn("Invalid reference space!");
		return { 0, 0, 0 };
	}
}
XMFLOAT3A Transform::GetUp(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
	{
		auto* v = LocalMatrix()->m[1];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A up;
		Store(up, vec);
		return up;
	}

	case World:
	{
		auto* v = WorldMatrix()->m[1];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A up;
		Store(up, vec);
		return up;
	}

	default:
		Warn("Invalid reference space!");
		return { 0, 0, 0 };
	}
}
XMFLOAT3A Transform::GetForward(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
	{
		auto* v = LocalMatrix()->m[2];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A forward;
		Store(forward, vec);
		return forward;
	}

	case World:
	{
		auto* v = WorldMatrix()->m[2];
		XMVECTOR vec = XMVectorSet(v[0], v[1], v[2], 0);
		vec = XMVector3Normalize(vec);

		XMFLOAT3A forward;
		Store(forward, vec);
		return forward;
	}

	default:
		Warn("Invalid reference space!");
		return { 0, 0, 0 };
	}
}
void Transform::GetAxes(XMFLOAT3A *right, XMFLOAT3A *up, XMFLOAT3A *forward, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
	{
		auto m = LocalMatrix()->m;
		
		if (right)
		{
			memcpy(right, m[0], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(right));
			Store(*right, vec);
		}

		if (up)
		{
			memcpy(up, m[1], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(up));
			Store(*up, vec);
		}

		if (forward)
		{
			memcpy(forward, m[2], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(forward));
			Store(*forward, vec);
		}
		break;
	}

	case World:
	{
		auto m = WorldMatrix()->m;

		if (right)
		{
			memcpy(right, m[0], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(right));
			Store(*right, vec);
		}

		if (up)
		{
			memcpy(up, m[1], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(up));
			Store(*up, vec);
		}

		if (forward)
		{
			memcpy(forward, m[2], sizeof(XMFLOAT3));
			XMVECTOR vec = XMVector3Normalize(Load(forward));
			Store(*forward, vec);
		}
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}
}

const XMFLOAT3A &Transform::GetPosition(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		return _localPosition;

	case World:
		return *WorldPosition();

	default:
		Warn("Invalid reference space!");
		return _localPosition;
	}
}
const XMFLOAT4A &Transform::GetRotation(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		return _localRotation;

	case World:
		return *WorldRotation();

	default:
		Warn("Invalid reference space!");
		return _localRotation;
	}
}
const XMFLOAT3A &Transform::GetScale(ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		return _localScale;

	case World:
		return *WorldScale();

	default:
		Warn("Invalid reference space!");
		return _localScale;
	}
}

void Transform::InverseTransformPointInPlace(dx::XMFLOAT3A &point) const
{
	if (_parent)
		_parent->InverseTransformPointInPlace(point);

	XMVECTOR pointVec = Load(point);
	pointVec = XMVectorSubtract(pointVec, Load(_localPosition));

	XMVECTOR invRot = XMQuaternionInverse(Load(_localRotation));
	pointVec = XMVector3Rotate(pointVec, invRot);

	Store(point, XMVectorDivide(pointVec, Load(_localScale)));
}
const XMFLOAT3A Transform::InverseTransformPoint(XMFLOAT3A point) const
{
	if (_parent)
		_parent->InverseTransformPointInPlace(point);

	XMVECTOR pointVec = Load(point);
	pointVec = XMVectorSubtract(pointVec, Load(_localPosition));

	XMVECTOR invRot = XMQuaternionInverse(Load(_localRotation));
	pointVec = XMVector3Rotate(pointVec, invRot);

	Store(point, XMVectorDivide(pointVec, Load(_localScale)));
	return point;
}
const XMFLOAT4X4A Transform::GetWorldRotationAndScale()
{
	XMMATRIX worldRS = XMMatrixMultiply(XMMatrixRotationQuaternion(Load(_localRotation)), XMMatrixScaling(_localScale.x, _localScale.y, _localScale.z));

	if (_parent) 
	{
		XMMATRIX parentRS = Load(_parent->GetWorldRotationAndScale());
		//worldRS = parentRS * worldRS;
		worldRS = XMMatrixMultiply(worldRS, parentRS);
	}

	XMFLOAT4X4A storedWorldRS;
	Store(storedWorldRS, worldRS);

	return storedWorldRS;
}

void Transform::SetPosition(const XMFLOAT3A &position, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		_localPosition = position;
		break;

	case World:
	{
		//_localPosition = InverseTransformPoint(position);
		Store(_localPosition, XMVector3TransformCoord(Load(position), XMMatrixInverse(nullptr, Load(_parent->GetWorldMatrix()))));
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	SetWorldPositionDirty();
	_isLocalMatrixDirty = true;
}
void Transform::SetPosition(const XMFLOAT4A &position, ReferenceSpace space)
{
	SetPosition(To3(position), space);
}
void Transform::SetRotation(const XMFLOAT4A &rotation, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		_localRotation = rotation;
		break;

	case World:
	{
		XMVECTOR rot = Load(rotation);
		rot = XMQuaternionNormalize(rot);
		//Store(_localRotation, XMQuaternionMultiply(XMQuaternionInverse(Load(_parent->WorldRotation())), rot));
		Store(_localRotation, XMQuaternionMultiply(rot, XMQuaternionInverse(Load(_parent->WorldRotation()))));
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	VerifyRotation();
	SetWorldRotationDirty();
	_isLocalMatrixDirty = true;
}
void Transform::SetScale(const XMFLOAT3A &scale, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		_localScale = scale;
		break;

	case World:
	{
		const XMVECTOR worldRotationQuat = Load(WorldRotation());

		XMFLOAT3A x = XMFLOAT3A(scale.x, 0, 0);
		XMFLOAT3A y = XMFLOAT3A(0, scale.y, 0);
		XMFLOAT3A z = XMFLOAT3A(0, 0, scale.z);

		Store(x, XMVector3Rotate(Load(x), worldRotationQuat));
		Store(y, XMVector3Rotate(Load(y), worldRotationQuat));
		Store(z, XMVector3Rotate(Load(z), worldRotationQuat));

		const XMMATRIX rsMat = XMMATRIX(
			x.x, x.y, x.z, 0,
			y.x, y.y, y.z, 0,
			z.x, z.y, z.z, 0,
			0,   0,   0,   1
		);

		_localScale = XMFLOAT3A(1, 1, 1);

		const XMMATRIX inverseRS = XMMatrixInverse(nullptr, Load(GetWorldRotationAndScale()));
		XMFLOAT4X4A localRS;
		Store(localRS, XMMatrixMultiply(rsMat, inverseRS));

		_localScale = XMFLOAT3A(localRS._11, localRS._22, localRS._33);
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	VerifyScale();
	SetWorldScaleDirty();
	_isLocalMatrixDirty = true;
}
void Transform::SetScale(const XMFLOAT4A &scale, ReferenceSpace space)
{
	SetScale(To3(scale), space);
}
void Transform::SetMatrix(const XMFLOAT4X4A &mat, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMVECTOR s, r, p;
	XMMatrixDecompose(&s, &r, &p, Load(mat));

	switch (space)
	{
	case Local:
		Store(_localScale, s);
		Store(_localRotation, r);
		Store(_localPosition, p);
		VerifyRotation();
		VerifyScale();
		break;

	case World:
	{
		XMFLOAT3A scale, pos;
		XMFLOAT4A rot;

		Store(scale, s);
		Store(rot, r);
		Store(pos, p);

		SetScale(scale, World);
		SetRotation(rot, World);
		SetPosition(pos, World);
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	SetWorldRotationDirty();
	_isWorldPositionDirty = true;
	_isWorldScaleDirty = true;
	_isLocalMatrixDirty = true;
}

void Transform::Move(const XMFLOAT3A &direction, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		Store(_localPosition, XMVectorAdd(Load(_localPosition), Load(direction)));
		break;

	case World:
	{
		XMFLOAT3A newPos;
		Store(newPos, XMVectorAdd(Load(WorldPosition()), Load(direction)));
		SetPosition(newPos, World);
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	SetWorldPositionDirty();
	_isLocalMatrixDirty = true;
}
void Transform::Move(const XMFLOAT4A &direction, ReferenceSpace space)
{
	Move(To3(direction), space);
}
void Transform::Rotate(const XMFLOAT3A &euler, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	switch (space)
	{
	case Local:
		Store(_localRotation, XMQuaternionMultiply(Load(_localRotation), XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z)));
		break;

	case World:
	{
		XMFLOAT4A newRot;
		Store(newRot, XMQuaternionMultiply(Load(WorldRotation()), XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z)));
		SetRotation(newRot, World);
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	VerifyRotation();
	SetWorldRotationDirty();
	_isLocalMatrixDirty = true;
}
void Transform::Rotate(const XMFLOAT4A &euler, ReferenceSpace space)
{
	Rotate(To3(euler), space);
}
void Transform::Scale(const XMFLOAT3A &scale, ReferenceSpace space, bool additive)
{
	const auto mathFunc = additive ? XMVectorAdd : XMVectorMultiply;

	switch (space)
	{
	case Local:
		Store(_localScale, mathFunc(Load(_localScale), Load(scale)));
		VerifyScale();
		break;

	case World:
	{
		XMFLOAT3A newScale;
		Store(newScale, mathFunc(Load(WorldScale()), Load(scale)));
		SetScale(newScale, World);
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	SetWorldScaleDirty();
	_isLocalMatrixDirty = true;
}
void Transform::Scale(const XMFLOAT4A &scale, ReferenceSpace space, bool additive)
{
	Scale(To3(scale), space, additive);
}

void Transform::MoveRelative(const XMFLOAT3A &direction, ReferenceSpace space)
{
	XMFLOAT3A right, up, forward;
	GetAxes(&right, &up, &forward, space);

	XMVECTOR 
		d = Load(direction),
		r = Load(right), 
		u = Load(up), 
		f = Load(forward);

	XMVECTOR relativeDirVec = 
		XMVectorMultiplyAdd(r, XMVectorSplatX(d), 
		XMVectorMultiplyAdd(u, XMVectorSplatY(d), 
			XMVectorMultiply(f, XMVectorSplatZ(d))));

	XMFLOAT3A relativeDir;
	Store(relativeDir, relativeDirVec);

	Move(relativeDir, space);
}
void Transform::MoveRelative(const XMFLOAT4A &direction, ReferenceSpace space)
{
	MoveRelative(To3(direction), space);
}
void Transform::RotateAxis(const XMFLOAT3A &axis, float amount, ReferenceSpace space)
{
	XMVECTOR axisVec = XMVector3Normalize(Load(axis));
	XMVECTOR currentRotation = Load(space == World ? *WorldRotation() : _localRotation);

	XMFLOAT4A newRotation;
	Store(newRotation, XMQuaternionMultiply(currentRotation, XMQuaternionRotationAxis(axisVec, amount)));

	SetRotation(newRotation, space);
}
void Transform::RotateAxis(const XMFLOAT4A &axis, float amount, ReferenceSpace space)
{
	RotateAxis(To3(axis), amount, space);
}
void Transform::RotateQuaternion(const XMFLOAT4A &quaternion, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMVECTOR rot = Load(quaternion);
	rot = XMQuaternionNormalize(rot);

	switch (space)
	{
	case Local:
		Store(_localRotation, XMQuaternionMultiply(Load(_localRotation), rot));
		break;

	case World:
	{
		Store(_localRotation, XMQuaternionMultiply(rot, Load(_localRotation)));

		//XMFLOAT4A newRot;
		//Store(newRot, XMQuaternionMultiply(Load(WorldRotation()), rot));
		//SetRotation(newRot, World);

		//Store(_localRotation, XMQuaternionMultiply(Load(WorldRotation()), rot));
		break;
	}

	default:
		Warn("Invalid reference space!");
		break;
	}

	VerifyRotation();
	SetWorldRotationDirty();
	_isLocalMatrixDirty = true;
}

const XMFLOAT3A Transform::GetEuler(ReferenceSpace space)
{
	XMFLOAT4A rot = GetRotation(space);

	const float xx = rot.x * rot.x;
	const float yy = rot.y * rot.y;
	const float zz = rot.z * rot.z;

	const float m31 = 2.0f * rot.x * rot.z + 2.0f * rot.y * rot.w;
	const float m32 = 2.0f * rot.y * rot.z - 2.0f * rot.x * rot.w;
	const float m33 = 1.0f - 2.0f * xx - 2.0f * yy;

	const float cy = sqrtf(m33 * m33 + m31 * m31);
	const float cx = atan2f(-m32, cy);

	if (cy > 16.0f * FLT_EPSILON)
	{
		const float m12 = 2.0f * rot.x * rot.y + 2.0f * rot.z * rot.w;
		const float m22 = 1.0f - 2.0f * xx - 2.0f * zz;

		return XMFLOAT3A(cx, atan2f(m31, m33), atan2f(m12, m22));
	}
	else
	{
		const float m11 = 1.0f - 2.0f * yy - 2.0f * zz;
		const float m21 = 2.0f * rot.x * rot.y - 2.f * rot.z * rot.w;

		return XMFLOAT3A(cx, 0.0f, atan2f(-m21, m11));
	}
}
void Transform::SetEuler(const XMFLOAT3A &pitchYawRoll, ReferenceSpace space)
{
	XMFLOAT4A newRot;
	Store(newRot, XMQuaternionRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z));
	SetRotation(newRot, space);
}
void Transform::SetEuler(const XMFLOAT4A &pitchYawRoll, ReferenceSpace space)
{
	SetEuler(To3(pitchYawRoll), space);
}

void Transform::SetLookDir(const dx::XMFLOAT3A &forward, const dx::XMFLOAT3A &up, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMVECTOR forwardVec = XMVector3Normalize(Load(forward));
	XMVECTOR upVec = XMVector3Normalize(Load(up));
	XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(upVec, forwardVec));

	upVec = XMVector3Cross(forwardVec, rightVec);

	XMVECTOR pos = Load(GetPosition(space));

	XMMATRIX newMat{};
	newMat.r[0] = XMVectorScale(rightVec, _localScale.x);
	newMat.r[1] = XMVectorScale(upVec, _localScale.y);
	newMat.r[2] = XMVectorScale(forwardVec, _localScale.z);
	newMat.r[3] = pos;
	newMat.r[3].m128_f32[3] = 1.0f;

	XMFLOAT4X4A mat;
	Store(mat, newMat);

	SetMatrix(mat, space);
}
void Transform::SetLookDir(const dx::XMFLOAT3A &forward, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMFLOAT3A up = GetUp(space);
	SetLookDir(forward, up, space);
}
void Transform::LookAt(const dx::XMFLOAT3A &point)
{
	XMVECTOR pos = Load(GetPosition(World));
	XMVECTOR target = Load(point);

	XMFLOAT3A forward;
	Store(forward, XMVectorSubtract(target, pos));

	SetLookDir(forward, World);
}
void Transform::LookAt(const dx::XMFLOAT3A &point, const dx::XMFLOAT3A &up)
{
	XMVECTOR pos = Load(GetPosition(World));
	XMVECTOR target = Load(point);

	XMFLOAT3A forward;
	Store(forward, XMVectorSubtract(target, pos));

	SetLookDir(forward, up, World);
}

void Transform::RotatePitch(const float amount, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMFLOAT3A axis = GetRight(space);
	XMVECTOR forward = Load(axis);
	XMVECTOR localAxisRotation = XMQuaternionRotationAxis(forward, amount);

	XMFLOAT4A oldRotation = GetRotation();
	XMFLOAT4A newRotation;
	Store(newRotation, XMQuaternionMultiply(Load(oldRotation), localAxisRotation));
	SetRotation(newRotation, space);
}
void Transform::RotateYaw(const float amount, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMFLOAT3A axis = GetUp(space);
	XMVECTOR forward = Load(axis);
	XMVECTOR localAxisRotation = XMQuaternionRotationAxis(forward, amount);

	XMFLOAT4A oldRotation = GetRotation();
	XMFLOAT4A newRotation;
	Store(newRotation, XMQuaternionMultiply(Load(oldRotation), localAxisRotation));

	SetRotation(newRotation, space);
}
void Transform::RotateRoll(const float amount, ReferenceSpace space)
{
	if (!_parent)
		space = Local;

	XMFLOAT3A axis = GetForward(space);
	XMVECTOR forward = Load(axis);
	XMVECTOR localAxisRotation = XMQuaternionRotationAxis(forward, amount);

	XMFLOAT4A oldRotation = GetRotation();
	XMFLOAT4A newRotation;
	Store(newRotation, XMQuaternionMultiply(Load(oldRotation), localAxisRotation));

	SetRotation(newRotation, space);
}

bool Transform::UpdateConstantBuffer(ID3D11DeviceContext *context)
{
	if (!_isDirty)
		return true;

	const XMMATRIX transposeWorldMatrix = XMMatrixTranspose(Load(WorldMatrix()));
	const XMMATRIX worldMatrixData[2] = {
		transposeWorldMatrix,
		XMMatrixTranspose(XMMatrixInverse(nullptr, transposeWorldMatrix)),
	};

	if (!_worldMatrixBuffer.UpdateBuffer(context, &worldMatrixData))
	{
		ErrMsg("Failed to update world matrix buffer!");
		return false;
	}

	_isDirty = false;
	return true;
}
ID3D11Buffer *Transform::GetConstantBuffer() const
{
	return _worldMatrixBuffer.GetBuffer();
}

const XMFLOAT4X4A &Transform::GetLocalMatrix()
{
	return *LocalMatrix();
}
const XMFLOAT4X4A &Transform::GetWorldMatrix()
{
	return *WorldMatrix();
}
const XMFLOAT4X4A Transform::GetUnscaledWorldMatrix()
{
	XMMATRIX worldMatrix = Load(WorldMatrix());

	worldMatrix.r[0] = XMVector3Normalize(worldMatrix.r[0]);
	worldMatrix.r[1] = XMVector3Normalize(worldMatrix.r[1]);
	worldMatrix.r[2] = XMVector3Normalize(worldMatrix.r[2]);

	XMFLOAT4X4A unscaledWorldMatrix;
	Store(unscaledWorldMatrix, worldMatrix);

	return unscaledWorldMatrix;
}
const XMFLOAT4X4A &Transform::GetMatrix(ReferenceSpace space)
{
	return (space == World) ? *WorldMatrix() : *LocalMatrix();
}

#ifdef USE_IMGUI
bool Transform::RenderUI(ReferenceSpace space)
{
	constexpr ImGuiInputTextFlags floatInputFlags = ImGuiInputTextFlags_CharsDecimal;
	bool isChanged = false;

	{
		XMFLOAT3A entPos = GetPosition(space);

		ImGui::Text("Position: "); ImGui::SameLine();
		if (ImGui::DragFloat3("##Position", &entPos.x, 0.01f, 0.0f, 0.0f, "%.4f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		if (isChanged)
		{
			SetPosition(entPos, space);
			isChanged = false;
		}
	}

	{
		XMFLOAT3A entRot = GetEuler(space);
		XMFLOAT3A entRotDeg = {
			entRot.x * RAD_TO_DEG,
			entRot.y * RAD_TO_DEG,
			entRot.z * RAD_TO_DEG
		};

		ImGui::Text("Rotation: "); ImGui::SameLine();
		if (ImGui::DragFloat3("##Rotation", &entRotDeg.x, 0.01f, 0.0f, 0.0f, "%.4f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		if (isChanged)
		{
			entRot = {
				entRotDeg.x * DEG_TO_RAD,
				entRotDeg.y * DEG_TO_RAD,
				entRotDeg.z * DEG_TO_RAD
			};

			SetEuler(entRot, space);
			isChanged = false;
		}
	}

	{
		XMFLOAT3A entScale = GetScale(space);

		ImGui::Text("Scale:    "); ImGui::SameLine();
		if (ImGui::DragFloat3("##Scale", &entScale.x, 0.01f, 0.0f, 0.0f, "%.4f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		if (isChanged)
		{
			SetScale(entScale, space);
			isChanged = false;
		}
	}

	ImGui::Separator();

	if (ImGui::TreeNode("Matrix##ShowMatrix"))
	{
		XMFLOAT4X4A entMat = GetMatrix(space);

		ImGui::Text("R1: "); ImGui::SameLine();
		if (ImGui::DragFloat4("##MatrixR1", entMat.m[0], 0.01f, 0.0f, 0.0f, "%.3f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		ImGui::Text("R2: "); ImGui::SameLine();
		if (ImGui::DragFloat4("##MatrixR2", entMat.m[1], 0.01f, 0.0f, 0.0f, "%.3f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		ImGui::Text("R3: "); ImGui::SameLine();
		if (ImGui::DragFloat4("##MatrixR3", entMat.m[2], 0.01f, 0.0f, 0.0f, "%.3f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		ImGui::Text("R4: "); ImGui::SameLine();
		if (ImGui::DragFloat4("##MatrixR4", entMat.m[3], 0.01f, 0.0f, 0.0f, "%.3f", floatInputFlags))
			isChanged = true;
		ImGuiUtils::LockMouseOnActive();

		if (isChanged)
		{
			SetMatrix(entMat, space);
			isChanged = false;
		}
		
		ImGui::TreePop();
	}
	ImGui::Dummy(ImVec2(0, 4));

	ImGui::PushID("Transform Catalogue");
	if (ImGui::CollapsingHeader("Transform Catalogue"))
	{
		ImGuiChildFlags childFlags = 0;
		childFlags |= ImGuiChildFlags_Border;
		childFlags |= ImGuiChildFlags_ResizeY;

		ImGui::BeginChild("Transform Catalogue", ImVec2(0, 150), childFlags);
		ImGui::Text("Note: Getters write to input. Setters read from input.");
		ImGui::Text("Methods with an additional float parameter use the w-value.");
		ImGui::Text("The reference space selected above is used for all methods.");

		static XMFLOAT4A parameter = { 0, 0, 0, 0 };
		ImGui::Text("Input: "); ImGui::SameLine();
		ImGui::DragFloat4("", &parameter.x, 0.01f, 0.0f, 0.0f, "%.4f", floatInputFlags);
		ImGuiUtils::LockMouseOnActive();

		XMFLOAT4A *vec4 = &parameter;
		XMFLOAT3A *vec3 = reinterpret_cast<XMFLOAT3A *>(&parameter);
		float *vec1 = &parameter.w;

		if (ImGui::TreeNode("Getters"))
		{
			if (ImGui::Button("Vec3 GetRight()"))
			{
				XMFLOAT3A vec = GetRight(space);
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			if (ImGui::Button("Vec3 GetUp()"))
			{
				XMFLOAT3A vec = GetUp(space);
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			if (ImGui::Button("Vec3 GetForward()"))
			{
				XMFLOAT3A vec = GetForward(space);
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			if (ImGui::Button("Vec3 GetPosition()"))
			{
				XMFLOAT3A vec = GetPosition(space);
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			if (ImGui::Button("Quat GetRotation()"))
			{
				XMFLOAT4A vec = GetRotation(space);
				std::memcpy(vec4, &vec, sizeof(XMFLOAT4));
			}

			if (ImGui::Button("Vec3 GetScale()"))
			{
				XMFLOAT3A vec = GetScale(space);
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			if (ImGui::Button("Vec3 GetEuler()"))
			{
				XMFLOAT3A vec = GetEuler(space);
				vec = {
					vec.x * RAD_TO_DEG,
					vec.y * RAD_TO_DEG,
					vec.z * RAD_TO_DEG
				};
				std::memcpy(vec3, &vec, sizeof(XMFLOAT3));
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Setters"))
		{
			if (ImGui::Button("SetPosition(Vec3)"))
				SetPosition(*vec3, space);

			if (ImGui::Button("SetRotation(Quat)"))
				SetRotation(*vec4, space);

			if (ImGui::Button("SetScale(Vec3)"))
				SetScale(*vec3, space);

			if (ImGui::Button("Move(Vec3)"))
				Move(*vec3, space);

			if (ImGui::Button("Rotate(Vec3)"))
			{
				XMFLOAT3A vec = {
					vec3->x * DEG_TO_RAD,
					vec3->y * DEG_TO_RAD,
					vec3->z * DEG_TO_RAD
				};
				Rotate(vec, space);
			}

			if (ImGui::Button("Scale(Vec3)"))
				Scale(*vec3, space);

			if (ImGui::Button("MoveRelative(Vec3)"))
				MoveRelative(*vec3, space);

			if (ImGui::Button("RotateAxis(Vec3, float)"))
				RotateAxis(*vec3, *vec1 * DEG_TO_RAD, space);

			if (ImGui::Button("RotateQuaternion(Quat)"))
				RotateQuaternion(*vec4, space);

			if (ImGui::Button("SetEuler(Vec3)"))
			{
				XMFLOAT3A vec = {
					vec3->x * DEG_TO_RAD,
					vec3->y * DEG_TO_RAD,
					vec3->z * DEG_TO_RAD
				};
				SetEuler(vec, space);
			}

			if (ImGui::Button("SetLookDir(Vec3)"))
			{
				XMFLOAT3A vec = {
					vec3->x,
					vec3->y,
					vec3->z
				};
				SetLookDir(vec, space);
			}

			if (ImGui::Button("RotatePitch(float)"))
				RotatePitch(*vec1 * DEG_TO_RAD);

			if (ImGui::Button("RotateYaw(float)"))
				RotateYaw(*vec1 * DEG_TO_RAD);

			if (ImGui::Button("RotateRoll(float)"))
				RotateRoll(*vec1 * DEG_TO_RAD);

			ImGui::TreePop();
		}

		ImGui::EndChild();
	}
	ImGui::PopID();

	ImGui::Separator();

	return true;
}
#endif
