#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <DirectXMath.h>

#include "ContentManager/Common.h"

namespace ContentManager::AssetData
{
	struct Vertex
	{
		float px, py, pz;    // position
		float nx, ny, nz;    // normal
		float tx, ty, tz;    // tangent
		float bx, by, bz;    // bitangent
		float u0, v0;        // tex coords 0
		float u1, v1;        // tex coords 1
	};

	struct SubMesh
	{
		std::string name = "";
		UINT startIndex = 0;
		UINT indexCount = 0;
	};

	struct Mesh
	{
		std::vector<Vertex>     vertices;
		std::vector<UINT>       indices;
		std::vector<SubMesh>    subMeshes;
	};


	struct Bone
	{
		std::string			name;
		int					parentIndex = -1;	// -1: root
		DirectX::XMFLOAT4X4 offsetMat;			// Mesh-space to bone-space at bind pose
	};

	struct SkinnedVertex
	{
		Vertex v;
		std::array<int, 4>      boneIDs = { -1,-1,-1,-1 };
		std::array<float, 4>	weights = { 0, 0, 0, 0 };
	};

	struct SkinnedMesh
	{
		std::vector<SkinnedVertex>				vertices;
		std::vector<UINT>						indices;
		std::vector<SubMesh>					subMeshes;
		std::vector<Bone>						bones;
		std::unordered_map<std::string, int>	boneMap;
	};
}