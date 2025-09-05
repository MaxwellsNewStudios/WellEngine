#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXCollision.h>
#include <vector>

#include "SubMeshD3D11.h"
#include "VertexBufferD3D11.h"
#include "IndexBufferD3D11.h"
#include "Collision/MeshCollider.h"

struct MeshData
{
	struct VertexInfo
	{
		UINT sizeOfVertex = 0;
		UINT nrOfVerticesInBuffer = 0;
		float *vertexData = nullptr;

		~VertexInfo() 
		{
			delete[] vertexData;
		}

		void Compile(std::vector<char> &data) const
		{
			data.insert(data.end(), (char *)&sizeOfVertex, (char *)&sizeOfVertex + sizeof(UINT));
			data.insert(data.end(), (char *)&nrOfVerticesInBuffer, (char *)&nrOfVerticesInBuffer + sizeof(UINT));
			data.insert(data.end(), (char *)vertexData, (char *)vertexData + (sizeOfVertex * nrOfVerticesInBuffer));
		}
		void Decompile(const std::vector<char> &data, size_t &offset)
		{
			sizeOfVertex = *(UINT *)&data[offset];
			offset += sizeof(UINT);

			nrOfVerticesInBuffer = *(UINT *)&data[offset];
			offset += sizeof(UINT);

			delete[] vertexData;
			vertexData = new float[sizeOfVertex * nrOfVerticesInBuffer];

			memcpy(vertexData, &data[offset], sizeOfVertex * nrOfVerticesInBuffer);
			offset += sizeOfVertex * nrOfVerticesInBuffer;
		}
	} vertexInfo;

	struct IndexInfo
	{
		UINT nrOfIndicesInBuffer = 0;
		UINT *indexData = nullptr;

		~IndexInfo() 
		{
			delete[] indexData;
		}

		void Compile(std::vector<char> &data) const
		{
			data.insert(data.end(), (char *)&nrOfIndicesInBuffer, (char *)&nrOfIndicesInBuffer + sizeof(UINT));
			data.insert(data.end(), (char *)indexData, (char *)indexData + (sizeof(UINT) * nrOfIndicesInBuffer));
		}
		void Decompile(const std::vector<char> &data, size_t &offset)
		{
			nrOfIndicesInBuffer = *(UINT *)&data[offset];
			offset += sizeof(UINT);

			delete[] indexData;
			indexData = new UINT[nrOfIndicesInBuffer];

			memcpy(indexData, &data[offset], sizeof(UINT) * nrOfIndicesInBuffer);
			offset += sizeof(UINT) * nrOfIndicesInBuffer;
		}
	} indexInfo;

	struct SubMeshInfo
	{
		UINT startIndexValue = 0;
		UINT nrOfIndicesInSubMesh = 0;

		std::string ambientTexturePath = "";
		std::string diffuseTexturePath = "";
		std::string specularTexturePath = "";
		float specularExponent = 0.0f;

		void Compile(std::vector<char> &data) const
		{
			data.insert(data.end(), (char *)&startIndexValue, (char *)&startIndexValue + sizeof(UINT));
			data.insert(data.end(), (char *)&nrOfIndicesInSubMesh, (char *)&nrOfIndicesInSubMesh + sizeof(UINT));

			data.insert(data.end(), ambientTexturePath.begin(), ambientTexturePath.end());
			data.emplace_back('\0');

			data.insert(data.end(), diffuseTexturePath.begin(), diffuseTexturePath.end());
			data.emplace_back('\0');

			data.insert(data.end(), specularTexturePath.begin(), specularTexturePath.end());
			data.emplace_back('\0');

			data.insert(data.end(), (char *)&specularExponent, (char *)&specularExponent + sizeof(float));
		}
		void Decompile(const std::vector<char> &data, size_t &offset)
		{
			startIndexValue = *(UINT *)&data[offset];
			offset += sizeof(UINT);

			nrOfIndicesInSubMesh = *(UINT *)&data[offset];
			offset += sizeof(UINT);

			ambientTexturePath = &data[offset];
			offset += ambientTexturePath.size() + 1;

			diffuseTexturePath = &data[offset];
			offset += diffuseTexturePath.size() + 1;

			specularTexturePath = &data[offset];
			offset += specularTexturePath.size() + 1;

			specularExponent = *(float *)&data[offset];
			offset += sizeof(float);
		}
	};

	std::vector<SubMeshInfo> subMeshInfo = {};
	std::string mtlFile = "";
	dx::BoundingOrientedBox boundingBox = {};


	void Compile(std::vector<char> &data) const
	{
		vertexInfo.Compile(data);

		indexInfo.Compile(data);

		UINT subMeshCount = (UINT)subMeshInfo.size();
		data.insert(data.end(), (char *)&subMeshCount, (char *)&subMeshCount + sizeof(UINT));
		for (const SubMeshInfo &subMesh : subMeshInfo)
			subMesh.Compile(data);

		data.insert(data.end(), mtlFile.begin(), mtlFile.end());
		data.emplace_back('\0');

		data.insert(data.end(), (char *)&boundingBox, (char *)&boundingBox + sizeof(dx::BoundingOrientedBox));
	}
	void Decompile(const std::vector<char> &data, size_t &offset)
	{
		vertexInfo.Decompile(data, offset);

		indexInfo.Decompile(data, offset);

		UINT subMeshCount = *(UINT *)&data[offset];
		offset += sizeof(UINT);

		subMeshInfo.resize(subMeshCount);
		for (SubMeshInfo &subMesh : subMeshInfo)
			subMesh.Decompile(data, offset);

		mtlFile = &data[offset];
		offset += mtlFile.size() + 1;

		boundingBox = *(dx::BoundingOrientedBox *)&data[offset];
		offset += sizeof(dx::BoundingOrientedBox);
	}


	// Merge the vertex and index data of another mesh into this mesh. Does not work for meshes with non-trivial submesh data.
	void MergeWithMesh(const MeshData *other, const dx::XMFLOAT4X4A *otherToThisSpace);
};


class MeshD3D11
{
private:
	std::vector<SubMeshD3D11> _subMeshes;
	VertexBufferD3D11 _vertexBuffer;
	IndexBufferD3D11 _indexBuffer;

	MeshData *_meshData = nullptr;
	MeshCollider _meshCollider;

public:
	MeshD3D11() = default;
	~MeshD3D11();
	MeshD3D11(const MeshD3D11 &other) = delete;
	MeshD3D11 &operator=(const MeshD3D11 &other) = delete;
	MeshD3D11(MeshD3D11 &&other) = delete;
	MeshD3D11 &operator=(MeshD3D11 &&other) = delete;

	[[nodiscard]] bool Initialize(ID3D11Device *device, MeshData **meshInfo);
	void Reset();

	[[nodiscard]] bool BindMeshBuffers(ID3D11DeviceContext *context, UINT stride = 0, UINT offset = 0) const;
	[[nodiscard]] bool PerformSubMeshDrawCall(ID3D11DeviceContext *context, UINT subMeshIndex) const;

	[[nodiscard]] const dx::BoundingOrientedBox &GetBoundingOrientedBox() const;
	[[nodiscard]] const std::string &GetMaterialFile() const;

	[[nodiscard]] UINT GetNrOfSubMeshes() const;
	[[nodiscard]] const std::string &GetAmbientPath(UINT subMeshIndex) const;
	[[nodiscard]] const std::string &GetDiffusePath(UINT subMeshIndex) const;
	[[nodiscard]] const std::string &GetSpecularPath(UINT subMeshIndex) const;
	[[nodiscard]] ID3D11Buffer *GetSpecularBuffer(UINT subMeshIndex) const;

	[[nodiscard]] const MeshData *GetMeshData() const;
	[[nodiscard]] const MeshCollider &GetMeshCollider() const;

	TESTABLE()
};