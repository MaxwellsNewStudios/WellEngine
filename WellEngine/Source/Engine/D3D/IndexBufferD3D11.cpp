#include "stdafx.h"
#include "IndexBufferD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


IndexBufferD3D11::IndexBufferD3D11(ID3D11Device *device, const size_t nrOfIndicesInBuffer, const uint32_t *indexData)
{
	if (!Initialize(device, nrOfIndicesInBuffer, indexData))
		ErrMsg("Failed to initialize index buffer!");
}

bool IndexBufferD3D11::Initialize(ID3D11Device *device, const size_t nrOfIndicesInBuffer, const uint32_t *indexData)
{
	if (_buffer != nullptr)
	{
		ErrMsg("Index buffer is not nullptr!");
		return false;
	}

	_nrOfIndices = nrOfIndicesInBuffer;

	D3D11_BUFFER_DESC bufferDesc = { };
	bufferDesc.ByteWidth = (UINT)sizeof(uint32_t) * (UINT)_nrOfIndices;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA srData = { };
	srData.pSysMem = indexData;
	srData.SysMemPitch = 0;
	srData.SysMemSlicePitch = 0;

	if (FAILED(device->CreateBuffer(&bufferDesc, &srData, _buffer.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create index buffer!");
		return false;
	}
	return true;
}

void IndexBufferD3D11::Reset()
{
	_buffer.Reset();
	_nrOfIndices = 0;
}

size_t IndexBufferD3D11::GetNrOfIndices() const
{
	return _nrOfIndices;
}

ID3D11Buffer *IndexBufferD3D11::GetBuffer() const
{
	return _buffer.Get();
}