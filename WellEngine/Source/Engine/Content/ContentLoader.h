#pragma once
#include "D3D/MeshD3D11.h"

namespace ContentData
{
	struct RawPosition	{ float	x, y, z; };
	struct RawNormal	{ float	x, y, z; };
	struct RawTexCoord	{ float	u, v;	 };
	struct RawIndex		{ int	v, t, n; };

	struct SubMaterial
	{
		std::string
			mtlName,
			ambientPath,
			diffusePath,
			specularPath;
		float specularExponent;
	};

	struct MaterialGroup
	{
		std::string mtlName;
		std::vector<SubMaterial> subMaterials;
	};

	struct FormattedVertex {
		float 
			px, py, pz,
			nx, ny, nz,
			tx, ty, tz,
			u, v;

		FormattedVertex() :
			px(0.0f), py(0.0f), pz(0.0f),
			nx(0.0f), ny(0.0f), nz(0.0f),
			tx(0.0f), ty(0.0f), tz(0.0f),
			u(0.0f), v(0.0f)
		{ }

		FormattedVertex(
			const float px, const float py, const float pz, 
			const float nx, const float ny, const float nz, 
			const float tx, const float ty, const float tz, 
			const float u, const float v) :
			px(px), py(py), pz(pz),
			nx(nx), ny(ny), nz(nz),
			tx(tx), ty(ty), tz(tz),
			u(u), v(v)
		{ }

		bool operator==(const FormattedVertex &other) const
		{
			if (px != other.px) return false;
			if (py != other.py) return false;
			if (pz != other.pz) return false;

			if (nx != other.nx) return false;
			if (ny != other.ny) return false;
			if (nz != other.nz) return false;

			if (tx != other.tx) return false;
			if (ty != other.ty) return false;
			if (tz != other.tz) return false;

			if (u != other.u) return false;
			if (v != other.v) return false;

			return true;
		}
	};
}


[[nodiscard]] bool LoadMeshFromFile(const char *path, MeshData *meshData);
[[nodiscard]] bool WriteMeshToFile(const char *path, const MeshData *meshData);

[[nodiscard]] bool LoadDDSTextureFromFile(
	ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	dx::DDS_ALPHA_MODE *alphaMode = nullptr, bool mipmapped = false);

[[nodiscard]] bool LoadTextureFromFile(
	ID3D11Device *device, ID3D11DeviceContext *context, 
	const std::string &path, ID3D11Texture2D *&texture, ID3D11ShaderResourceView *&srv, 
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, bool mipmapped = false, UINT downsample = 0, bool flip = true);

[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned char> &data);
[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<unsigned short> &data);
[[nodiscard]] bool LoadTextureFromFile(const std::string &path, UINT &width, UINT &height, std::vector<float> &data, int nChannels, bool highPrecision);
[[nodiscard]] bool LoadTextureFromFile(const std::string &path, std::vector<unsigned char> &data, UINT &width, UINT &height, UINT &channels, UINT &bitsPerChannel);

[[nodiscard]] bool DownsampleTexture(std::vector<uint8_t> &data, UINT inWidth, UINT inHeight, UINT outWidth, UINT outHeight);
