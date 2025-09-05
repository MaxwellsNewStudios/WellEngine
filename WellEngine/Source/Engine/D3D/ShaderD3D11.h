#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

enum class ShaderType
{
	VERTEX_SHADER	= 0,
	HULL_SHADER		= 1,
	DOMAIN_SHADER	= 2,
	GEOMETRY_SHADER = 3,
	PIXEL_SHADER	= 4,
	COMPUTE_SHADER	= 5,
};

/*namespace ShaderReflection
{
	struct BindingData
	{
		std::string name;
		UINT registerIndex = 0;
		D3D_SHADER_INPUT_TYPE type = D3D_SIT_TEXTURE; // Default to texture
		D3D_RESOURCE_RETURN_TYPE returnType = D3D_RETURN_TYPE_UNORM; // Default to UNORM
		D3D_SRV_DIMENSION dimension = D3D_SRV_DIMENSION_TEXTURE2D; // Default to 2D texture
	};

	struct ShaderData
	{
		std::vector<BindingData> bindings;
	};
};*/


class ShaderD3D11
{
private:
	ShaderType _type = ShaderType::VERTEX_SHADER;
	ComPtr<ID3DBlob> _shaderBlob = nullptr;
	ComPtr<ID3D11ShaderReflection> _reflector = nullptr;

	ComPtr<ID3D11VertexShader> _vertex = nullptr;
	ComPtr<ID3D11HullShader> _hull = nullptr;
	ComPtr<ID3D11DomainShader> _domain = nullptr;
	ComPtr<ID3D11GeometryShader> _geometry = nullptr;
	ComPtr<ID3D11PixelShader> _pixel = nullptr;
	ComPtr<ID3D11ComputeShader> _compute = nullptr;

	void Release();

public:
	ShaderD3D11() = default;
	~ShaderD3D11();

	ShaderD3D11(const ShaderD3D11 &other) = delete;
	ShaderD3D11 &operator=(const ShaderD3D11 &other) = delete;
	ShaderD3D11(ShaderD3D11 &&other) = delete;
	ShaderD3D11 &operator=(ShaderD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, ShaderType shaderType, ID3DBlob *shaderBlob);
	[[nodiscard]] bool Initialize(ID3D11Device *device, ShaderType shaderType, const void *dataPtr, size_t dataSize);
	[[nodiscard]] bool Initialize(ID3D11Device *device, ShaderType shaderType, const char *csoPath);

	[[nodiscard]] bool BindShader(ID3D11DeviceContext *context) const;

	[[nodiscard]] const void *GetShaderByteData() const;
	[[nodiscard]] size_t GetShaderByteSize() const;
	[[nodiscard]] ShaderType GetShaderType() const;
	[[nodiscard]] ID3D11ShaderReflection *GetReflector() const;

	TESTABLE()
};