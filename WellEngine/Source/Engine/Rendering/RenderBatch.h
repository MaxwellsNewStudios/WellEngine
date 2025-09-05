#pragma once
#include "Content/Material.h"
#include "D3D/StructuredBufferD3D11.h"

class MeshBehaviour;

// TODO: Finsih implementing the RenderBatch class.
// This class is used to batch draw calls for static meshes. It stores all instance data in structured buffers.
class RenderBatch
{
private:
	struct InstanceMatrixData
	{
		dx::XMMATRIX worldMatrix;
		dx::XMMATRIX inverseTransposeWorldMatrix;
	};

	// Stores an index for each unculled instance pointing to it in the data buffers.
	StructuredBufferD3D11 _queuedInstanceIndexBuffer;	// int[_drawCount]
	StructuredBufferD3D11 _instanceMatrixDataBuffer;	// InstanceMatrixData[_instanceCount]
	StructuredBufferD3D11 _instanceMaterialDataBuffer;	// MaterialProperties[_instanceCount]

	const Material *_material;
	const MeshBehaviour *_instances;

	const UINT _meshID;
	const UINT _instanceCount;
	UINT _drawCount;

public:
	RenderBatch() = delete;
	RenderBatch(const UINT meshID, const Material *material, const UINT _instanceCount);
	~RenderBatch();

	[[nodiscard]] bool Queue();

	[[nodiscard]] bool UpdateBuffers();
	[[nodiscard]] bool BindBuffers() const;
	[[nodiscard]] bool ResetState();

	TESTABLE()
};