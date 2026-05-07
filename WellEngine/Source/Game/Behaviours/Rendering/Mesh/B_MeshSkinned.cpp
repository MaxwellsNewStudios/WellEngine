#include "stdafx.h"
#include "B_MeshSkinned.h"
#include "Game/Entity.h"
#include "Game/Scene/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

using namespace DirectX;

bool B_MeshSkinned::Start()
{
	// TODO

	return B_Mesh::Start();
}

bool B_MeshSkinned::Update(TimeUtils &time, const Input &input)
{
	// TODO

	return B_Mesh::Update(time, input);
}

bool B_MeshSkinned::Render(RenderQueuer &queuer, const RendererInfo &rendererInfo)
{
	if (rendererInfo.shadowCamera && !B_Mesh::GetCastShadows())
		return true;

	const RenderQueueEntry entry = {
		B_Mesh::GetResourceGroup(),
		RenderInstance(dynamic_cast<Behaviour *>(this), sizeof(B_MeshSkinned))
	};

	if (B_Mesh::GetTransparent())
		queuer.QueueTransparent(entry);
	else
		queuer.QueueGeometry(entry);

	return true;
}

#ifdef USE_IMGUI
bool B_MeshSkinned::RenderUI()
{
	if (ImGui::CollapsingHeader("Skin"))
	{
		Content *content = GetScene()->GetContent();

		// TODO
	}

	if (ImGui::CollapsingHeader("Mesh"))
		if (!B_Mesh::RenderUI())
			return false;

	return true;
}
#endif

bool B_MeshSkinned::BindBuffers(ID3D11DeviceContext *context, UINT submeshIndex)
{
	// TODO

	return B_Mesh::BindBuffers(context, submeshIndex);
}

bool B_MeshSkinned::Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj)
{
	// TODO

	json::Value meshObj = {};
	if (!B_Mesh::Serialize(docAlloc, meshObj))
		return false;
	obj.AddMember("Mesh", meshObj, docAlloc);
	
	return true;
}
bool B_MeshSkinned::Deserialize(const json::Value &obj, Scene *scene)
{
	// TODO

	if (obj.HasMember("Mesh"))
		if (!B_Mesh::Deserialize(obj, scene))
			return false;

	return true;
}
bool B_MeshSkinned::PostDeserialize()
{
	// TODO

	return B_Mesh::PostDeserialize();
}

void B_MeshSkinned::StoreBounds(BoundingOrientedBox &meshBounds)
{
	// TODO
}
