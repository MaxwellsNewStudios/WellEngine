#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class VertexBufferD3D11
{
private:
	ComPtr<ID3D11Buffer> _buffer = nullptr;
	size_t _nrOfVertices = 0;
	size_t _vertexSize = 0;

public:
	VertexBufferD3D11() = default;
	~VertexBufferD3D11() = default;
	VertexBufferD3D11(const VertexBufferD3D11 &other) = delete;
	VertexBufferD3D11 &operator=(const VertexBufferD3D11 &other) = delete;
	VertexBufferD3D11(VertexBufferD3D11 &&other) = delete;
	VertexBufferD3D11 &operator=(VertexBufferD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, size_t sizeOfVertex,
		size_t nrOfVerticesInBuffer, const void *vertexData);

	void Reset();

	[[nodiscard]] size_t GetNrOfVertices() const;
	[[nodiscard]] size_t GetVertexSize() const;
	[[nodiscard]] ID3D11Buffer *GetBuffer() const;

	TESTABLE()
};