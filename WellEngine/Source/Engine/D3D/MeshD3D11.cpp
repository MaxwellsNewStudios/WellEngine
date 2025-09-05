#include "stdafx.h"
#include "MeshD3D11.h"
#include "Content/ContentLoader.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using Microsoft::WRL::ComPtr;


MeshD3D11::~MeshD3D11()
{
	if (_meshData)
	{
		delete _meshData;
		_meshData = nullptr;
	}
}

bool MeshD3D11::Initialize(ID3D11Device *device, MeshData **meshData)
{
	if (_meshData)
	{
		delete _meshData;
		_meshData = nullptr;
	}

	_meshData = *meshData;
	*meshData = nullptr;

	if (!_vertexBuffer.Initialize(device, _meshData->vertexInfo.sizeOfVertex, _meshData->vertexInfo.nrOfVerticesInBuffer, _meshData->vertexInfo.vertexData))
	{
		ErrMsg("Failed to initialize vertex buffer!");
		return false;
	}

	if (!_indexBuffer.Initialize(device, _meshData->indexInfo.nrOfIndicesInBuffer, _meshData->indexInfo.indexData))
	{
		ErrMsg("Failed to initialize index buffer!");
		return false;
	}

	const size_t subMeshCount = _meshData->subMeshInfo.size();
	for (size_t i = 0; i < subMeshCount; i++)
	{
		auto &subMeshInfo = _meshData->subMeshInfo[i];

		SubMeshD3D11 subMesh;
		if (!subMesh.Initialize(device,
			subMeshInfo.startIndexValue, 
			subMeshInfo.nrOfIndicesInSubMesh,
			subMeshInfo.ambientTexturePath,
			subMeshInfo.diffuseTexturePath,
			subMeshInfo.specularTexturePath,
			subMeshInfo.specularExponent))
		{
			ErrMsg("Failed to initialize sub mesh!");
			return false;
		}

		_subMeshes.emplace_back(std::move(subMesh));
	}

	UINT submeshUsed = 0;
	UINT submeshCount = _meshData->subMeshInfo.size();

	switch (MESH_COLLISION_DETAIL_REDUCTION)
	{
	case 0: // Use highest detail
		break;
		
	default:
	case 1: // Use middle detail
		submeshUsed = (UINT)std::floor((float)submeshCount / 2.0f);
		break;

	case 2: // Use lowest detail
		submeshUsed = submeshCount - 1;
		break;
	}
	
	if (!_meshCollider.Initialize(*_meshData, submeshUsed))
	{
		ErrMsg("Failed to initialize mesh collider!");
		return false;
	}

	return true;
}

void MeshD3D11::Reset()
{
	_vertexBuffer.Reset();
	_indexBuffer.Reset();
	_subMeshes.clear();
	if (_meshData)
	{
		delete _meshData;
		_meshData = nullptr;
	}
	_meshCollider = {};
}

bool MeshD3D11::BindMeshBuffers(ID3D11DeviceContext *context, UINT stride, const UINT offset) const
{
	if (stride == 0)
		stride = static_cast<UINT>(_vertexBuffer.GetVertexSize());

	ID3D11Buffer *const vertxBuffer = _vertexBuffer.GetBuffer();
	context->IASetVertexBuffers(0, 1, &vertxBuffer, &stride, &offset);

	ID3D11Buffer *const indexBuffer = _indexBuffer.GetBuffer();
	context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	return true;
}

bool MeshD3D11::PerformSubMeshDrawCall(ID3D11DeviceContext *context, const UINT subMeshIndex) const
{
	if (!_subMeshes[subMeshIndex].PerformDrawCall(context))
	{
		ErrMsgF("Failed to perform draw call for sub mesh #{}!", subMeshIndex);
		return false;
	}
	return true;
}

const dx::BoundingOrientedBox &MeshD3D11::GetBoundingOrientedBox() const
{
	return _meshData->boundingBox;
 }

const std::string &MeshD3D11::GetMaterialFile() const
{
	return _meshData->mtlFile;
}


UINT MeshD3D11::GetNrOfSubMeshes() const
{
	return static_cast<UINT>(_subMeshes.size());
}


const std::string &MeshD3D11::GetAmbientPath(const UINT subMeshIndex) const
{
	return _subMeshes[subMeshIndex].GetAmbientPath();
}

const std::string &MeshD3D11::GetDiffusePath(const UINT subMeshIndex) const
{
	return _subMeshes[subMeshIndex].GetDiffusePath();
}

const std::string &MeshD3D11::GetSpecularPath(const UINT subMeshIndex) const
{
	return _subMeshes[subMeshIndex].GetSpecularPath();
}

ID3D11Buffer *MeshD3D11::GetSpecularBuffer(const UINT subMeshIndex) const
{
	return _subMeshes[subMeshIndex].GetSpecularBuffer();
}

const MeshData *MeshD3D11::GetMeshData() const
{
	return _meshData;
}

const MeshCollider &MeshD3D11::GetMeshCollider() const
{
	return _meshCollider;
}


void MeshData::MergeWithMesh(const MeshData *other, const dx::XMFLOAT4X4A *otherMatrix)
{
	ZoneScopedXC(RandomUniqueColor());

	using namespace DirectX;

	// Transform the other mesh data using the matrix if one is provided,
	// then merge the vertex and index data into this mesh data.
	// Each index in the other mesh is incremented by the vertex count of this mesh.

	bool thisIsUninitialized = subMeshInfo.empty();

	if (subMeshInfo.size() > 1 || other->subMeshInfo.size() > 1)
	{


		ErrMsg("Merging meshes with multiple submeshes not supported!");
		return;
	}

	bool doTransform = otherMatrix;
	XMMATRIX mat{};
	if (doTransform)
	{
		// Calculate tha matrix for translating from the other meshes space to this meshes space.
		//mat = XMMatrixInverse(nullptr, Load(otherMatrix));
		mat = Load(otherMatrix);
	}

	UINT thisVertexCount = vertexInfo.nrOfVerticesInBuffer;
	UINT thisIndexCount = indexInfo.nrOfIndicesInBuffer;

	UINT otherVertexCount = other->vertexInfo.nrOfVerticesInBuffer;
	UINT otherIndexCount = other->indexInfo.nrOfIndicesInBuffer;

	UINT newVertexCount = thisVertexCount + otherVertexCount;
	UINT newIndexCount = thisIndexCount + otherIndexCount;

	// Vertex data
	{
		ContentData::FormattedVertex *thisVertexData = reinterpret_cast<ContentData::FormattedVertex *>(vertexInfo.vertexData);
		ContentData::FormattedVertex *otherVertexData = reinterpret_cast<ContentData::FormattedVertex *>(other->vertexInfo.vertexData);
		ContentData::FormattedVertex *newVertexData = new ContentData::FormattedVertex[newVertexCount];

		// Copy the current vertex data to the new buffer, no need to transform.
		memcpy(newVertexData, thisVertexData, sizeof(ContentData::FormattedVertex) * thisVertexCount);

		// Copy the other vertex data to the new buffer, transforming as we go if needed.
		for (UINT otherIter = 0; otherIter < otherVertexCount; otherIter++)
		{
			UINT newIter = thisVertexCount + otherIter;
			ContentData::FormattedVertex &newVertex = newVertexData[newIter];
			newVertex = otherVertexData[otherIter];

			if (doTransform)
			{
				XMVECTOR pos = XMVectorSet(newVertex.px, newVertex.py, newVertex.pz, 1.0f);
				XMVECTOR normal = XMVectorSet(newVertex.nx, newVertex.ny, newVertex.nz, 0.0f);
				XMVECTOR tangent = XMVectorSet(newVertex.tx, newVertex.ty, newVertex.tz, 0.0f);

				pos = XMVector3TransformCoord(pos, mat);
				normal = XMVector3TransformNormal(normal, mat);
				tangent = XMVector3TransformNormal(tangent, mat);

				Store((XMFLOAT3 *)(&(newVertex.px)), pos);
				Store((XMFLOAT3 *)(&(newVertex.nx)), normal);
				Store((XMFLOAT3 *)(&(newVertex.tx)), tangent);
			}
		}

		delete[] vertexInfo.vertexData;
		vertexInfo.vertexData = reinterpret_cast<float *>(newVertexData);
		vertexInfo.nrOfVerticesInBuffer = newVertexCount;

		if (thisIsUninitialized)
			vertexInfo.sizeOfVertex = other->vertexInfo.sizeOfVertex;
	}

	// Index Data
	{
		UINT *thisIndexData = indexInfo.indexData;
		UINT *otherIndexData = other->indexInfo.indexData;
		UINT *newIndexData = new UINT[newIndexCount];

		// Copy the current index data to the new buffer.
		memcpy(newIndexData, thisIndexData, sizeof(UINT) * thisIndexCount);

		// Copy the other index data to the new buffer.
		memcpy(&(newIndexData[thisIndexCount]), otherIndexData, sizeof(UINT) * otherIndexCount);

		// Offset the index values copied from the other mesh by the current vertex count.
		for (UINT i = thisIndexCount; i < newIndexCount; i++)
		{
			newIndexData[i] += thisVertexCount;
		}

		delete[] indexInfo.indexData;
		indexInfo.indexData = newIndexData;
		indexInfo.nrOfIndicesInBuffer = newIndexCount;
	}

	// Submesh Data
	{
		if (thisIsUninitialized)
			subMeshInfo.emplace_back();

		SubMeshInfo &newSubMesh = subMeshInfo[0];
		const SubMeshInfo &otherSubMesh = other->subMeshInfo[0];

		newSubMesh.startIndexValue = 0;
		newSubMesh.nrOfIndicesInSubMesh = newIndexCount;

		if (thisIsUninitialized)
		{
			newSubMesh.ambientTexturePath = otherSubMesh.ambientTexturePath,
			newSubMesh.diffuseTexturePath = otherSubMesh.diffuseTexturePath;
			newSubMesh.specularTexturePath = otherSubMesh.specularTexturePath;
			newSubMesh.specularExponent = otherSubMesh.specularExponent;
		}
	}

	// Bounds
	if (thisIsUninitialized)
	{
		boundingBox = other->boundingBox;

		if (doTransform)
			boundingBox.Transform(boundingBox, mat);
	}
	else
	{
		BoundingOrientedBox otherBounds = other->boundingBox;

		if (doTransform)
			otherBounds.Transform(otherBounds, mat);

		BoundingBox mergedAABB = MergeBounds(boundingBox, otherBounds);
		BoundingOrientedBox::CreateFromBoundingBox(boundingBox, mergedAABB);
	}

	if (thisIsUninitialized)
		mtlFile = other->mtlFile;
}
