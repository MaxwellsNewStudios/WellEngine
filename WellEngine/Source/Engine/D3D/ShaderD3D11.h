#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <d3d11shader.h>
#include "ContentManager/Common.h"

namespace WellEngine
{
	typedef ContentManager::ShaderType ShaderType;

	class ShaderD3D11
	{
	private:
		ShaderType _type = ShaderType::VERTEX_SHADER;
		Microsoft::WRL::ComPtr<ID3DBlob> _shaderBlob = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> _reflector = nullptr;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertex = nullptr;
		Microsoft::WRL::ComPtr<ID3D11HullShader> _hull = nullptr;
		Microsoft::WRL::ComPtr<ID3D11DomainShader> _domain = nullptr;
		Microsoft::WRL::ComPtr<ID3D11GeometryShader> _geometry = nullptr;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixel = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ComputeShader> _compute = nullptr;

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
	};
}
