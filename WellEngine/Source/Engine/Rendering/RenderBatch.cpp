#include "stdafx.h"
#include "RenderBatch.h"
#include "Behaviours/MeshBehaviour.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

RenderBatch::RenderBatch(const UINT meshID, const Material *material, const UINT _instanceCount) : _meshID(meshID), _material(material), _instanceCount(_instanceCount)
{
	_drawCount = 0;
	_instances = new MeshBehaviour[_instanceCount];
}

RenderBatch::~RenderBatch()
{
	if (_instances)
	{
		delete[] _instances;
		_instances = nullptr;
	}
}
