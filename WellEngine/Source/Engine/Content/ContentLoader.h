#pragma once

#include <DirectXTex.h>

#include "Engine/D3D/MeshD3D11.h"
#include "Content.h"

namespace WellEngine
{
	[[nodiscard]] bool LoadMeshFromFile(const char *path, MeshData *meshData);


	[[nodiscard]] std::string GetTextureBakePath(const std::string &file);

	[[nodiscard]] bool LoadDDSTextureFromFile(ID3D11Device *device, ID3D11DeviceContext *context, 
		const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
		TexLoadInfo *info = nullptr);

	[[nodiscard]] bool LoadTextureFromFile(ID3D11Device *device, ID3D11DeviceContext *context, 
		const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
		TexLoadInfo *info = nullptr, bool bake = true);

	[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned char> &data);
	[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned short> &data);
	[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<float> &data, int nChannels, bool highPrecision);
	[[nodiscard]] bool LoadTextureFromFile(const std::string &path, std::vector<unsigned char> &data, UINT &width, UINT &height, UINT &channels, UINT &bitsPerChannel);

	[[nodiscard]] bool DownsampleTexture(std::vector<uint8_t> &data, UINT inWidth, UINT inHeight, UINT outWidth, UINT outHeight);
}
