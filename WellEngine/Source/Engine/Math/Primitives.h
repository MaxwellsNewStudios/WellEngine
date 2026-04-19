#pragma once

#include <vector>
#include <DirectXMath.h>

namespace WellEngine::Primitives
{
	namespace dx = DirectX;

	void GenerateIcoSphere(int subdivisions, std::vector<dx::XMFLOAT3> &outVertices, std::vector<int> &outIndices);
}
