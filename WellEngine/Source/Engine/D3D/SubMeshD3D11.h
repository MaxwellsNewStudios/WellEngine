#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include "ConstantBufferD3D11.h"

struct SpecularBuffer
{
	float specularExponent = 0.0f;
	float padding[3] = { 0, 0, 0 };
};

class SubMeshD3D11
{
private:
	size_t _startIndex = 0;
	size_t _nrOfIndices = 0;

	std::string _ambientTexturePath;
	std::string _diffuseTexturePath;
	std::string _specularTexturePath;
	ConstantBufferD3D11 *_specularBuffer = nullptr;

public:
	SubMeshD3D11() = default;
	~SubMeshD3D11();
	SubMeshD3D11(const SubMeshD3D11 &other) = default;
	SubMeshD3D11 &operator=(const SubMeshD3D11 &other) = default;
	SubMeshD3D11(SubMeshD3D11 &&other) noexcept;
	SubMeshD3D11 &operator=(SubMeshD3D11 &&other) = default;

	[[nodiscard]] bool Initialize(ID3D11Device *device, UINT startIndexValue, UINT nrOfIndicesInSubMesh,
		const std::string &ambientPath, const std::string &diffusePath, const std::string &specularPath, float exponent);

	[[nodiscard]] bool PerformDrawCall(ID3D11DeviceContext *context) const;
	
	[[nodiscard]] const std::string &GetAmbientPath() const;
	[[nodiscard]] const std::string &GetDiffusePath() const;
	[[nodiscard]] const std::string &GetSpecularPath() const;
	[[nodiscard]] ID3D11Buffer *GetSpecularBuffer() const;

	TESTABLE()
};
