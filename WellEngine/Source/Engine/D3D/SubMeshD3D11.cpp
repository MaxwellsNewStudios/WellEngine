#include "stdafx.h"
#include "SubMeshD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;

bool SubMeshD3D11::Initialize(ID3D11Device *device, const UINT startIndexValue, const UINT nrOfIndicesInSubMesh)
{
	_startIndex = startIndexValue;
	_nrOfIndices = nrOfIndicesInSubMesh;

	return true;
}


bool SubMeshD3D11::PerformDrawCall(ID3D11DeviceContext *context) const
{
	context->DrawIndexed(static_cast<UINT>(_nrOfIndices), static_cast<UINT>(_startIndex), 0);
	return true;
}

size_t SubMeshD3D11::GetIndexCount() const
{
	return _nrOfIndices;
}
