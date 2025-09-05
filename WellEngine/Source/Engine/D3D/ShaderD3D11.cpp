#include "stdafx.h"
#include "ShaderD3D11.h"
#include <D3DCompiler.h>

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif


void ShaderD3D11::Release()
{
	switch (_type)
	{
	case ShaderType::VERTEX_SHADER:
		_vertex.Reset();
		_vertex = nullptr;
		break;

	case ShaderType::HULL_SHADER:
		_hull.Reset();
		_hull = nullptr;
		break;

	case ShaderType::DOMAIN_SHADER:
		_domain.Reset();
		_domain = nullptr;
		break;

	case ShaderType::GEOMETRY_SHADER:
		_geometry.Reset();
		_geometry = nullptr;
		break;

	case ShaderType::PIXEL_SHADER:
		_pixel.Reset();
		_pixel = nullptr;
		break;

	case ShaderType::COMPUTE_SHADER:
		_compute.Reset();
		_compute = nullptr;
		break;

	default:
		break;
	}

	_shaderBlob.Reset();
	_shaderBlob = nullptr;

	_reflector.Reset();
	_reflector = nullptr;
}

ShaderD3D11::~ShaderD3D11()
{
	Release();
}


bool ShaderD3D11::Initialize(ID3D11Device *device, ShaderType shaderType, ID3DBlob *shaderBlob)
{
	_shaderBlob.ReleaseAndGetAddressOf();
	_shaderBlob = shaderBlob;

	size_t dataSize = _shaderBlob.Get()->GetBufferSize();
	const void *shaderData = _shaderBlob->GetBufferPointer();

	_type = shaderType;
	switch (_type)
	{
	case ShaderType::VERTEX_SHADER:
		if (FAILED(device->CreateVertexShader(
			shaderData, dataSize,
			nullptr, _vertex.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create vertex shader!");
			return false;
		}
		break;

	case ShaderType::HULL_SHADER:
		if (FAILED(device->CreateHullShader(
			shaderData, dataSize,
			nullptr, _hull.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create hull shader!");
			return false;
		}
		break;

	case ShaderType::DOMAIN_SHADER:
		if (FAILED(device->CreateDomainShader(
			shaderData, dataSize,
			nullptr, _domain.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create domain shader!");
			return false;
		}
		break;

	case ShaderType::GEOMETRY_SHADER:
		if (FAILED(device->CreateGeometryShader(
			shaderData, dataSize,
			nullptr, _geometry.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create geometry shader!");
			return false;
		}
		break;

	case ShaderType::PIXEL_SHADER:
		if (FAILED(device->CreatePixelShader(
			shaderData, dataSize,
			nullptr, _pixel.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create pixel shader!");
			return false;
		}
		break;

	case ShaderType::COMPUTE_SHADER:
		if (FAILED(device->CreateComputeShader(
			shaderData, dataSize,
			nullptr, _compute.ReleaseAndGetAddressOf())))
		{
			ErrMsg("Failed to create compute shader!");
			return false;
		}
		break;
	}

	if (FAILED(D3DReflect(
		shaderData, dataSize,
		IID_ID3D11ShaderReflection,
		(void **)_reflector.ReleaseAndGetAddressOf())))
	{
		ErrMsg("Failed to create shader reflector!");
		return false;
	}

	return true;
}

bool ShaderD3D11::Initialize(ID3D11Device *device, ShaderType shaderType, const void *dataPtr, const size_t dataSize)
{
	ID3DBlob *shaderBlob = nullptr;
	D3DCreateBlob(dataSize, &shaderBlob);
	std::memcpy(shaderBlob->GetBufferPointer(), dataPtr, dataSize);
	
	if (!Initialize(device, shaderType, shaderBlob))
	{
		ErrMsg("Failed to initialize shader buffer!");
		shaderBlob->Release();
		return false;
	}
	
	return true;
}

bool ShaderD3D11::Initialize(ID3D11Device *device, ShaderType shaderType, const char *csoPath)
{
	std::string shaderFileData;
	std::ifstream reader;

	reader.open(csoPath, std::ios::binary | std::ios::ate);
	if (!reader.is_open())
	{
		ErrMsg("Failed to open shader file!");
		return false;
	}

	reader.seekg(0, std::ios::end);
	shaderFileData.reserve(static_cast<unsigned int>(reader.tellg()));
	reader.seekg(0, std::ios::beg);

	shaderFileData.assign(
		std::istreambuf_iterator<char>(reader),
		std::istreambuf_iterator<char>()
	);

	if (!Initialize(device, shaderType, shaderFileData.c_str(), shaderFileData.length()))
	{
		ErrMsg("Failed to initialize shader buffer!");
		return false;
	}

	shaderFileData.clear();
	reader.close();
	return true;
}



bool ShaderD3D11::BindShader(ID3D11DeviceContext *context) const
{
	switch (_type)
	{
		case ShaderType::VERTEX_SHADER:
			context->VSSetShader(_vertex.Get(), nullptr, 0);
			break;

		case ShaderType::HULL_SHADER:
			context->HSSetShader(_hull.Get(), nullptr, 0);
			break;

		case ShaderType::DOMAIN_SHADER:
			context->DSSetShader(_domain.Get(), nullptr, 0);
			break;

		case ShaderType::GEOMETRY_SHADER:
			context->GSSetShader(_geometry.Get(), nullptr, 0);
			break;

		case ShaderType::PIXEL_SHADER:
			context->PSSetShader(_pixel.Get(), nullptr, 0);
			break;

		case ShaderType::COMPUTE_SHADER:
			context->CSSetShader(_compute.Get(), nullptr, 0);
			break;
	}

	return true;
}


const void *ShaderD3D11::GetShaderByteData() const
{
	return _shaderBlob.Get()->GetBufferPointer();
}

size_t ShaderD3D11::GetShaderByteSize() const
{
	return _shaderBlob.Get()->GetBufferSize();
}

ShaderType ShaderD3D11::GetShaderType() const
{
	return _type;
}

ID3D11ShaderReflection *ShaderD3D11::GetReflector() const
{
	return _reflector.Get();
}
