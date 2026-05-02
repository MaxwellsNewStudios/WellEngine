#pragma once

#include <string>
#include <memory>
#include <d3d11.h>

#include "ConstantBufferD3D11.h"

namespace WellEngine
{
	class SubMeshD3D11
	{
	private:
		size_t _startIndex = 0;
		size_t _nrOfIndices = 0;

	public:
		[[nodiscard]] bool Initialize(ID3D11Device *device, UINT startIndexValue, UINT nrOfIndicesInSubMesh);

		[[nodiscard]] bool PerformDrawCall(ID3D11DeviceContext *context) const;
	
		[[nodiscard]] size_t GetIndexCount() const;
	};
}
