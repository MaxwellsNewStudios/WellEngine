#pragma once

#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

class IndexBufferD3D11
{
private:
	ComPtr<ID3D11Buffer> _buffer = nullptr;
	size_t _nrOfIndices = 0;

public:
	IndexBufferD3D11() = default;
	IndexBufferD3D11(ID3D11Device *device, size_t nrOfIndicesInBuffer, const uint32_t *indexData);
	~IndexBufferD3D11() = default;
	IndexBufferD3D11(const IndexBufferD3D11 &other) = delete;
	IndexBufferD3D11 &operator=(const IndexBufferD3D11 &other) = delete;
	IndexBufferD3D11(IndexBufferD3D11 &&other) = delete;
	IndexBufferD3D11 &operator=(IndexBufferD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, size_t nrOfIndicesInBuffer, const uint32_t *indexData);
	void Reset();

	[[nodiscard]] size_t GetNrOfIndices() const;
	[[nodiscard]] ID3D11Buffer *GetBuffer() const;

	TESTABLE()
};