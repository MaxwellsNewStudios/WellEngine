#include "stdafx.h"
#include "VertexBufferD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


bool VertexBufferD3D11::Initialize(ID3D11Device *device, const size_t sizeOfVertex, const size_t nrOfVerticesInBuffer, const void *vertexData)
{
	_vertexSize = sizeOfVertex;
	_nrOfVertices = nrOfVerticesInBuffer;

	D3D11_BUFFER_DESC bufferDesc = { };
	bufferDesc.ByteWidth = static_cast<UINT>(_vertexSize * _nrOfVertices);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA srData = { };
	srData.pSysMem = vertexData;
	srData.SysMemPitch = 0;
	srData.SysMemSlicePitch = 0;

	if (FAILED(device->CreateBuffer(&bufferDesc, &srData, _buffer.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create vertex buffer!");
		return false;
	}

	return true;
}
void VertexBufferD3D11::Reset()
{
	_buffer.Reset();
}

size_t VertexBufferD3D11::GetNrOfVertices() const
{
	return _nrOfVertices;
}

size_t VertexBufferD3D11::GetVertexSize() const
{
	return _vertexSize;
}

ID3D11Buffer *VertexBufferD3D11::GetBuffer() const
{
	return _buffer.Get();
}