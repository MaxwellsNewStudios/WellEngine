#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace WellEngine
{
	class MipRenderTargetD3D11
	{
	private:
		struct MipLevel
		{
			UINT width = 0;
			UINT height = 0;

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		rtv = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv = nullptr;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>	uav = nullptr;
		};

		Microsoft::WRL::ComPtr<ID3D11Texture2D>				_texture = nullptr;
		std::vector<MipLevel>				_mipLevels;

	public:
		MipRenderTargetD3D11() = default;
		~MipRenderTargetD3D11() = default;
		MipRenderTargetD3D11(const MipRenderTargetD3D11 &other) = delete;
		MipRenderTargetD3D11 &operator=(const MipRenderTargetD3D11 &other) = delete;
		MipRenderTargetD3D11(MipRenderTargetD3D11 &&other) = delete;
		MipRenderTargetD3D11 &operator=(MipRenderTargetD3D11 &&other) = delete;

		[[nodiscard]] bool Initialize(ID3D11Device *device, UINT width, UINT height, UINT mipLevels = 0, UINT skippedMips = 0,
			DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, bool hasUAV = false);
		[[nodiscard]] bool Initialize(ID3D11Device *device, D3D11_TEXTURE2D_DESC desc, UINT skippedMips = 0, bool hasUAV = false);

		void Reset();

		[[nodiscard]] ID3D11Texture2D *GetTexture() const;
		[[nodiscard]] ID3D11RenderTargetView *GetRTV(UINT mipLevel) const;
		[[nodiscard]] ID3D11ShaderResourceView *GetSRV(UINT mipLevel) const;
		[[nodiscard]] ID3D11UnorderedAccessView *GetUAV(UINT mipLevel) const;
		[[nodiscard]] UINT GetMipLevels() const { return static_cast<UINT>(_mipLevels.size()); }
		void GetMipSize(UINT mipLevel, UINT *width, UINT *height) const;
	};
}
