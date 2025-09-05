#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class ConstantBufferD3D11
{
private:
	ComPtr<ID3D11Buffer> _buffer = nullptr;
	size_t _bufferSize = 0;

public:
	ConstantBufferD3D11() = default;
	ConstantBufferD3D11(ID3D11Device *device, size_t byteSize, const void *initialData = nullptr);
	~ConstantBufferD3D11();
	ConstantBufferD3D11(const ConstantBufferD3D11 &other) = delete;
	ConstantBufferD3D11 &operator=(const ConstantBufferD3D11 &other) = delete;
	ConstantBufferD3D11(ConstantBufferD3D11 &&other) noexcept; // Move constructor
	ConstantBufferD3D11 &operator=(ConstantBufferD3D11 &&other) noexcept; // Move assignment operator

	[[nodiscard]] bool Initialize(ID3D11Device *device, size_t byteSize, const void *initialData = nullptr);
	void Reset();

	[[nodiscard]] size_t GetSize() const;
	[[nodiscard]] ID3D11Buffer *GetBuffer() const;

	[[nodiscard]] bool UpdateBuffer(ID3D11DeviceContext *context, const void *data) const;

	TESTABLE()
};