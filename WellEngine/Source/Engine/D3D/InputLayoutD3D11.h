#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

struct Semantic
{
	std::string name;
	DXGI_FORMAT format;
};


class InputLayoutD3D11
{
private:
	std::vector<std::string> _semanticNames; // Needed to store the semantic names of the element descs
	std::vector<D3D11_INPUT_ELEMENT_DESC> _elements;
	ComPtr<ID3D11InputLayout> _inputLayout = nullptr;

public:
	InputLayoutD3D11() = default;
	~InputLayoutD3D11() = default;
	InputLayoutD3D11(const InputLayoutD3D11 &other) = delete;
	InputLayoutD3D11 &operator=(const InputLayoutD3D11 &other) = delete;
	InputLayoutD3D11(InputLayoutD3D11 &&other) = delete;
	InputLayoutD3D11 &operator=(InputLayoutD3D11 &&other) = delete;

	void Reset();

	[[nodiscard]] bool AddInputElement(const Semantic &semantic);
	[[nodiscard]] bool FinalizeInputLayout(ID3D11Device *device, const void *vsDataPtr, size_t vsDataSize);

	[[nodiscard]] ID3D11InputLayout *GetInputLayout() const;

	TESTABLE()
};