#pragma once
#include <string>

namespace ContentData
{
	struct RawPosition { float	x, y, z; };
	struct RawNormal { float	x, y, z; };
	struct RawTexCoord { float	u, v; };
	struct RawIndex { int	v, t, n; };

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

	struct FormattedVertex 
	{
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
		{
		}

		FormattedVertex(
			const float px, const float py, const float pz,
			const float nx, const float ny, const float nz,
			const float tx, const float ty, const float tz,
			const float u, const float v) :
			px(px), py(py), pz(pz),
			nx(nx), ny(ny), nz(nz),
			tx(tx), ty(ty), tz(tz),
			u(u), v(v)
		{
		}

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
};
