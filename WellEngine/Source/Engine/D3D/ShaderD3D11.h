#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <d3d11shader.h>

namespace WellEngine
{
	enum class ShaderType
	{
		VERTEX_SHADER	= 0,
		HULL_SHADER		= 1,
		DOMAIN_SHADER	= 2,
		GEOMETRY_SHADER = 3,
		PIXEL_SHADER	= 4,
		COMPUTE_SHADER	= 5,
	};

	namespace ShaderTypeUtils
	{
		inline constexpr const char *ShaderTypeToString(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::VERTEX_SHADER:		return "Vertex";
			case ShaderType::HULL_SHADER:		return "Hull";
			case ShaderType::DOMAIN_SHADER:		return "Domain";
			case ShaderType::GEOMETRY_SHADER:	return "Geometry";
			case ShaderType::PIXEL_SHADER:		return "Pixel";
			case ShaderType::COMPUTE_SHADER:	return "Compute";
			default:							return "Unknown";
			}
		}

		inline const ShaderType ShaderTypeFromString(const char *str)
		{
			// Ordered by frequency, for performance
			if (std::strcmp(str, "Pixel") == 0)		return ShaderType::PIXEL_SHADER;
			if (std::strcmp(str, "Vertex") == 0)	return ShaderType::VERTEX_SHADER;
			if (std::strcmp(str, "Compute") == 0)	return ShaderType::COMPUTE_SHADER;
			if (std::strcmp(str, "Geometry") == 0)	return ShaderType::GEOMETRY_SHADER;
			if (std::strcmp(str, "Hull") == 0)		return ShaderType::HULL_SHADER;
			if (std::strcmp(str, "Domain") == 0)	return ShaderType::DOMAIN_SHADER;
			return ShaderType::PIXEL_SHADER;
		}
	}


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
