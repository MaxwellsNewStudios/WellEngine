#pragma once

#include <string>
#include <intsafe.h>
#include <vector>
#include "Content/Content.h"

namespace ShaderData
{
	enum class BindingType
	{
		Texture2D,
		Texture3D,
		TextureArray,
		TextureCube,
		SamplerState,
		StructuredBuffer,
		ConstantBuffer,
		UnorderedAccessView
	};

	struct BindingData
	{
		std::string name;
		BindingType type;
		UINT slot;
	};

	struct BoundResource
	{
		const BindingData *binding = nullptr;
		void *resourcePtr = nullptr;
	};


	class ShaderDef
	{
	public:
		ShaderDef() = default;
		~ShaderDef() = default;

		virtual const std::string &GetName() = 0;
		virtual const std::vector<BindingData> &GetBindings() = 0;
	};

	class ShaderResources
	{
	private:
		ShaderDef &_shaderDef;
		ShaderType _shaderType = ShaderType::VERTEX_SHADER;
		std::vector<BoundResource> _resources{};

	public:
		ShaderResources(ShaderDef &shaderDef) 
			: _shaderDef(shaderDef), _shaderType(Content::GetShaderTypeFromName(_shaderDef.GetName()))
		{
			const auto &bindings = _shaderDef.GetBindings();
			int bindingsCount = static_cast<int>(bindings.size());

			_resources.resize(bindingsCount);
			for (int i = 0; i < bindingsCount; i++)
				_resources[i].binding = &bindings[i];
		}

		void SetResource(const std::string &name, void *resourcePtr)
		{
			for (auto &resource : _resources)
			{
				if (resource.binding->name != name)
					continue;

				resource.resourcePtr = resourcePtr;
				break;
			}
		}

		void BindResource(ID3D11DeviceContext *context, const std::string &name)
		{
			for (auto &resource : _resources)
			{
				if (resource.binding->name != name)
					continue;

				if (resource.resourcePtr == nullptr)
					break;

				switch (_shaderType)
				{
				case ShaderType::VERTEX_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->VSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->VSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->VSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					default: break;
					}
					break;

				case ShaderType::HULL_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->HSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->HSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->HSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					default: break;
					}
					break;

				case ShaderType::DOMAIN_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->DSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->DSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->DSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					default: break;
					}
					break;

				case ShaderType::GEOMETRY_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->GSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->GSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->GSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					default: break;
					}
					break;

				case ShaderType::PIXEL_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->PSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->PSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->PSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					default: break;
					}
					break;

				case ShaderType::COMPUTE_SHADER:
					switch (resource.binding->type)
					{
					case BindingType::Texture2D:
					case BindingType::TextureCube:
					case BindingType::TextureArray:
					case BindingType::Texture3D:
					case BindingType::StructuredBuffer:
					{
						auto *toBind = static_cast<ShaderResourceTextureD3D11 *>(resource.resourcePtr)->GetSRV();
						context->CSSetShaderResources(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::SamplerState:
					{
						auto *toBind = static_cast<ID3D11SamplerState *>(resource.resourcePtr);
						context->CSSetSamplers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::ConstantBuffer:
					{
						auto *toBind = static_cast<ID3D11Buffer *>(resource.resourcePtr);
						context->CSSetConstantBuffers(resource.binding->slot, 1, &toBind);
						break;
					}

					case BindingType::UnorderedAccessView:
					{
						auto *toBind = static_cast<ID3D11UnorderedAccessView *>(resource.resourcePtr);
						context->CSSetUnorderedAccessViews(resource.binding->slot, 1, &toBind, nullptr);
						break;
					}

					default: break;
					}
					break;

				default:
					break;
				}
				
				break;
			}
		}

		TESTABLE()
	};
};
