#include "stdafx.h"
#include "UIDragDropHelpers.h"
#include "Entity.h"
#include "Scenes/Scene.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

template<typename P>
void ImGui::DragDropSource(const char *tag, P &payload, std::function<void(void)> preview, ImGuiDragDropFlags flags)
{
	if (BeginDragDropSource(flags))
	{
		IM_ASSERT(payload != nullptr);
		SetDragDropPayload(tag, payload, sizeof(P));

		if (preview)
			preview();

		EndDragDropSource();
	}
}
template<typename P>
bool ImGui::DragDropTarget(const char *tag, std::function<bool(P &)> onDropCallback)
{
	bool result = false;

	if (BeginDragDropTarget())
	{
		if (const ImGuiPayload *payload = AcceptDragDropPayload(tag))
		{
			IM_ASSERT(payload->DataSize == sizeof(P));
			P payload = *(const P *)payload->Data;

			result = onDropCallback(payload);
		}
		EndDragDropTarget();
	}

	return result;
}

template<typename P>
void ImGui::DragDropSource(PayloadType type, P &payload, std::function<void(void)> preview, ImGuiDragDropFlags flags)
{
	DragDropSource<P>(PayloadTags.at(type), payload, preview, flags);
}
template<typename P>
bool ImGui::DragDropTarget(PayloadType type, std::function<bool(P &)> onDropCallback)
{
	DragDropTarget<P>(PayloadTags.at(type), onDropCallback);
}

void ImGui::EntityDragDropSource(const Entity &ent, ImGuiDragDropFlags flags)
{
	if (BeginDragDropSource(flags))
	{
		EntityPayload payload = { ent.GetID(), ent.GetScene()->GetUID() };
		SetDragDropPayload(PayloadTags.at(PayloadType::ENTITY), &payload, sizeof(EntityPayload));

		Text(ent.GetName().c_str());

		EndDragDropSource();
	}
}
bool ImGui::EntityDragDropTarget(std::function<bool(EntityPayload &)> onDropCallback)
{
	bool result = false;

	if (BeginDragDropTarget())
	{
		if (const ImGuiPayload *payload = AcceptDragDropPayload(PayloadTags.at(PayloadType::ENTITY)))
		{
			IM_ASSERT(payload->DataSize == sizeof(EntityPayload));
			EntityPayload entPayload = *(const EntityPayload *)payload->Data;

			result = onDropCallback(entPayload);
		}
		EndDragDropTarget();
	}

	return result;
}
