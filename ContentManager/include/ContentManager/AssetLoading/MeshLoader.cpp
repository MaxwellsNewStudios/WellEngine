#include "MeshLoader.h"
#include "ContentManager/Internal/Internal.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include <iostream>

using namespace ContentManager;
using namespace ContentManager::AssetData;


static DirectX::XMFLOAT4X4 ToDX(const aiMatrix4x4 &m) 
{
	return DirectX::XMFLOAT4X4(
		m.a1, m.b1, m.c1, m.d1, // column-row
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
}

static void AddBoneInfluence(SkinnedVertex &v, int boneId, float weight) 
{
	// Limited to 4 influences internally, no need to handle here
	for (int i = 0; i < 4; ++i) 
	{
		if (v.boneIDs[i] == -1) 
		{
			v.boneIDs[i] = boneId;
			v.weights[i] = weight;
			return;
		}
	}
}

static void NormalizeBoneWeights(SkinnedVertex &v) 
{
	float totalWeight = 0;
	for (float w : v.weights) 
		totalWeight += w;

	if (totalWeight > 0) 
	{
		for (float &w : v.weights) 
			w /= totalWeight;
	}
}

static void ExtractBoneHierarchy(const aiNode *node, int parentIndex, SkinnedMesh &out)
{
	std::string name = node->mName.C_Str();

	// Only register nodes that correspond to actual bones
	if (out.boneMap.count(name)) 
	{
		int idx = out.boneMap[name];
		out.bones[idx].parentIndex = parentIndex;
		parentIndex = idx;   // children's parent is now this bone
	}

	for (unsigned i = 0; i < node->mNumChildren; ++i)
	{
		ExtractBoneHierarchy(node->mChildren[i], parentIndex, out);
	}
}


Mesh AssetLoader::LoadMesh(const std::string &path)
{
	Assimp::Importer importer;

	const aiScene *scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipWindingOrder
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
	{
		std::cout << "Assimp error: " << importer.GetErrorString() << "\n";
		return {};
	}

	Mesh result;

	// Preallocation
	size_t totalVertices = 0;
	size_t totalIndices = 0;

	for (UINT m = 0; m < scene->mNumMeshes; ++m)
	{
		totalVertices += scene->mMeshes[m]->mNumVertices;
		totalIndices += scene->mMeshes[m]->mNumFaces * 3ull;
	}

	result.vertices.reserve(totalVertices);
	result.indices.reserve(totalIndices);
	result.subMeshes.reserve(scene->mNumMeshes);

	size_t currVertStart = 0;
	size_t currIndexStart = 0;

	// Iterate all meshes in the scene
	for (UINT m = 0; m < scene->mNumMeshes; ++m)
	{
		const aiMesh *mesh = scene->mMeshes[m];

		// Submesh
		SubMesh &submesh = result.subMeshes.emplace_back();
		submesh.name = mesh->mName.C_Str();
		submesh.startIndex = (UINT)currIndexStart;

		// Vertices
		for (UINT v = 0; v < mesh->mNumVertices; ++v)
		{
			Vertex &vert = result.vertices.emplace_back();

			vert.px = mesh->mVertices[v].x;
			vert.py = mesh->mVertices[v].y;
			vert.pz = mesh->mVertices[v].z;

			if (mesh->HasNormals()) 
			{
				vert.nx = mesh->mNormals[v].x;
				vert.ny = mesh->mNormals[v].y;
				vert.nz = mesh->mNormals[v].z;
			}

			if (mesh->HasTangentsAndBitangents()) 
			{
				vert.tx = mesh->mTangents[v].x;
				vert.ty = mesh->mTangents[v].y;
				vert.tz = mesh->mTangents[v].z;

				vert.bx = mesh->mBitangents[v].x;
				vert.by = mesh->mBitangents[v].y;
				vert.bz = mesh->mBitangents[v].z;
			}

			if (mesh->mTextureCoords[0]) 
			{
				vert.u0 = mesh->mTextureCoords[0][v].x;
				vert.v0 = mesh->mTextureCoords[0][v].y;
			}

			if (mesh->mTextureCoords[1]) 
			{
				vert.u1 = mesh->mTextureCoords[1][v].x;
				vert.v1 = mesh->mTextureCoords[1][v].y;
			}
		}

		// Indices
		for (UINT f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace &face = mesh->mFaces[f];

			for (UINT i = 0; i < face.mNumIndices; ++i)
			{
				result.indices.emplace_back(currVertStart + face.mIndices[i]);
			}

			currIndexStart += face.mNumIndices;
		}

		currVertStart += mesh->mNumVertices;

		submesh.indexCount = (UINT)(currIndexStart - submesh.startIndex);
	}

	return result;
}

AssetData::SkinnedMesh AssetLoader::LoadSkinnedMesh(const std::string &path)
{
	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFile(path,
		aiProcess_Triangulate |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipWindingOrder |
		aiProcess_LimitBoneWeights // Handles normalization internally
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
	{
		std::cout << "Assimp error: " << importer.GetErrorString() << "\n";
		return {};
	}

	SkinnedMesh result;

	// Preallocation
	size_t totalVertices = 0;
	size_t totalIndices = 0;

	for (UINT m = 0; m < scene->mNumMeshes; ++m)
	{
		totalVertices += scene->mMeshes[m]->mNumVertices;
		totalIndices += scene->mMeshes[m]->mNumFaces * 3ull;
	}

	result.vertices.reserve(totalVertices);
	result.indices.reserve(totalIndices);
	result.subMeshes.reserve(scene->mNumMeshes);

	size_t currVertStart = 0;
	size_t currIndexStart = 0;

	// Pass 1
	// Geometry + bone registry
	for (UINT m = 0; m < scene->mNumMeshes; ++m)
	{
		const aiMesh *mesh = scene->mMeshes[m];

		// Submesh
		SubMesh &submesh = result.subMeshes.emplace_back();
		submesh.name = mesh->mName.C_Str();
		submesh.startIndex = (UINT)currIndexStart;

		// Vertices
		for (UINT v = 0; v < mesh->mNumVertices; ++v)
		{
			SkinnedVertex &sv = result.vertices.emplace_back();

			sv.v.px = mesh->mVertices[v].x;
			sv.v.py = mesh->mVertices[v].y;
			sv.v.pz = mesh->mVertices[v].z;

			if (mesh->HasNormals()) 
			{
				sv.v.nx = mesh->mNormals[v].x;
				sv.v.ny = mesh->mNormals[v].y;
				sv.v.nz = mesh->mNormals[v].z;
			}

			if (mesh->HasTangentsAndBitangents()) 
			{
				sv.v.tx = mesh->mTangents[v].x;
				sv.v.ty = mesh->mTangents[v].y;
				sv.v.tz = mesh->mTangents[v].z;

				sv.v.bx = mesh->mBitangents[v].x;
				sv.v.by = mesh->mBitangents[v].y;
				sv.v.bz = mesh->mBitangents[v].z;
			}

			if (mesh->mTextureCoords[0]) 
			{
				sv.v.u0 = mesh->mTextureCoords[0][v].x;
				sv.v.v0 = mesh->mTextureCoords[0][v].y;
			}

			if (mesh->mTextureCoords[1]) 
			{
				sv.v.u1 = mesh->mTextureCoords[1][v].x;
				sv.v.v1 = mesh->mTextureCoords[1][v].y;
			}
		}

		// Indices
		for (UINT f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace &face = mesh->mFaces[f];

			for (UINT i = 0; i < face.mNumIndices; ++i)
			{
				result.indices.emplace_back(currVertStart + face.mIndices[i]);
			}

			currIndexStart += face.mNumIndices;
		}

		submesh.indexCount = (UINT)(currIndexStart - submesh.startIndex);

		// Register each bone, then scatter weights onto vertices
		for (unsigned b = 0; b < mesh->mNumBones; ++b) 
		{
			const aiBone *bone = mesh->mBones[b];
			std::string boneName = bone->mName.C_Str();

			// Register bone if we haven't seen it yet
			if (!result.boneMap.count(boneName)) 
			{
				int idx = static_cast<int>(result.bones.size());
				result.boneMap[boneName] = idx;

				Bone bd;
				bd.name = boneName;
				bd.offsetMat = ToDX(bone->mOffsetMatrix);
				result.bones.push_back(bd);
			}

			int boneIndex = result.boneMap[boneName];

			// Write bone influences into the vertex array
			for (UINT w = 0; w < bone->mNumWeights; ++w)
			{
				UINT vertIdx = (UINT)(currVertStart + bone->mWeights[w].mVertexId);
				float weight = bone->mWeights[w].mWeight;

				AddBoneInfluence(result.vertices[vertIdx], boneIndex, weight);
			}
		}

		currVertStart += mesh->mNumVertices;
	}

	// Pass 2
	// Reconstruct the parent-child hierarchy
	ExtractBoneHierarchy(scene->mRootNode, -1, result);

	// Pass 3
	// Normalization (in case some vertices had more than 4 influences)
	for (SkinnedVertex &sv : result.vertices)
		NormalizeBoneWeights(sv);

	return result;
}
