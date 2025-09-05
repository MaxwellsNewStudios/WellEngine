#include "stdafx.h"
#include "SimpleMeshD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

bool SimpleMeshD3D11::Initialize(ID3D11Device *device, const SimpleMeshData &meshInfo)
{
	if (!_vertexBuffer.Initialize(device, meshInfo.vertexInfo.sizeOfVertex, meshInfo.vertexInfo.nrOfVerticesInBuffer, meshInfo.vertexInfo.vertexData))
	{
		ErrMsg("Failed to initialize vertex buffer!");
		return false;
	}

	return true;
}
void SimpleMeshD3D11::Reset()
{
	_vertexBuffer.Reset();
}

void SimpleMeshD3D11::BindMeshBuffers(ID3D11DeviceContext *context, UINT stride, const UINT offset) const
{
	if (stride == 0)
		stride = static_cast<UINT>(_vertexBuffer.GetVertexSize());

	ID3D11Buffer *const vertxBuffer = _vertexBuffer.GetBuffer();
	context->IASetVertexBuffers(0, 1, &vertxBuffer, &stride, &offset);
}

void SimpleMeshD3D11::PerformDrawCall(ID3D11DeviceContext *context) const
{
	context->Draw(static_cast<UINT>(_vertexBuffer.GetNrOfVertices()), 0);
}
