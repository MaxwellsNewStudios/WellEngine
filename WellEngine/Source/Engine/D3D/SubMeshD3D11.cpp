#include "stdafx.h"
#include "SubMeshD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


SubMeshD3D11::~SubMeshD3D11()
{
	delete _specularBuffer;
	_specularBuffer = nullptr;
}

SubMeshD3D11::SubMeshD3D11(SubMeshD3D11 &&other) noexcept
{
	_startIndex = other._startIndex;
	_nrOfIndices = other._nrOfIndices;

	_ambientTexturePath = std::move(other._ambientTexturePath);
	_diffuseTexturePath = std::move(other._diffuseTexturePath);
	_specularTexturePath = std::move(other._specularTexturePath);

	delete _specularBuffer;

	_specularBuffer = other._specularBuffer;
	other._specularBuffer = nullptr;
}

bool SubMeshD3D11::Initialize(ID3D11Device *device, const UINT startIndexValue, const UINT nrOfIndicesInSubMesh,
	const std::string &ambientPath, const std::string &diffusePath, const std::string &specularPath, const float exponent)
{
	_startIndex = startIndexValue;
	_nrOfIndices = nrOfIndicesInSubMesh;

	_ambientTexturePath = ambientPath;
	_diffuseTexturePath = diffusePath;
	_specularTexturePath = specularPath;

	delete _specularBuffer;
	_specularBuffer = new ConstantBufferD3D11();

	const SpecularBuffer specBuf = { exponent, { 0, 0, 0 } };
	if (!_specularBuffer->Initialize(device, sizeof(SpecularBuffer), &specBuf))
		Warn("Failed to initialize specular buffer!");

	return true;
}


bool SubMeshD3D11::PerformDrawCall(ID3D11DeviceContext *context) const
{
	context->DrawIndexed(static_cast<UINT>(_nrOfIndices), static_cast<UINT>(_startIndex), 0);
	return true;
}


const std::string &SubMeshD3D11::GetAmbientPath() const
{
	return _ambientTexturePath;
}

const std::string &SubMeshD3D11::GetDiffusePath() const
{
	return _diffuseTexturePath;
}

const std::string &SubMeshD3D11::GetSpecularPath() const
{
	return _specularTexturePath;
}

ID3D11Buffer *SubMeshD3D11::GetSpecularBuffer() const
{
	return _specularBuffer->GetBuffer();
}

