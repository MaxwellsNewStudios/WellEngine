#include "stdafx.h"
#include "InputLayoutD3D11.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;

void InputLayoutD3D11::Reset()
{
	_elements.clear();
	_semanticNames.clear();
	_inputLayout.Reset();
}

bool InputLayoutD3D11::AddInputElement(const Semantic &semantic)
{
	const UINT alignment = _elements.empty() ? 0 : D3D11_APPEND_ALIGNED_ELEMENT;

	_semanticNames.emplace_back(semantic.name);
	_elements.emplace_back(
		semantic.name.c_str(),
		0,
		semantic.format,
		0,
		alignment,
		D3D11_INPUT_PER_VERTEX_DATA,
		0
	);

	return true;
}

bool InputLayoutD3D11::FinalizeInputLayout(ID3D11Device *device, const void *vsDataPtr, const size_t vsDataSize)
{
	if (_inputLayout != nullptr)
	{
		ErrMsg("Input layout is already finalized!");
		return false;
	}

	if (FAILED(device->CreateInputLayout(
		_elements.data(), static_cast<UINT>(_elements.size()),
		vsDataPtr,
		vsDataSize,
		_inputLayout.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to finalize input layout!");
		return false;
	}
	return true;
}


ID3D11InputLayout *InputLayoutD3D11::GetInputLayout() const
{
	return _inputLayout.Get();
}
