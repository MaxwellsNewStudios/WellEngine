#pragma once
#include <intsafe.h>
#include <functional>
#include <ImGui/imgui.h>

class Entity;

namespace ImGui
{
	enum class PayloadType
	{
		ENTITY,
		BEHAVIOUR,
		TRANSFORM,
		MATERIAL,
		MESH,
		TEXTURE,
		CUBEMAP,
		SHADER_VERTEX,
		SHADER_PIXEL,
		SHADER_COMPUTE,
		SAMPLER,
		BLENDSTATE,

		UNKNOWN
	};
	const std::unordered_map<PayloadType, const char*> PayloadTags = {
		{ PayloadType::ENTITY,			"HIERARCHY_ENTITY"			},
		{ PayloadType::BEHAVIOUR,		"HIERARCHY_BEHAVIOUR"		},
		{ PayloadType::TRANSFORM,		"HIERARCHY_TRANSFORM"		},
		{ PayloadType::MATERIAL,		"CONTENT_MAT"				},
		{ PayloadType::MESH,			"CONTENT_MESH"				},
		{ PayloadType::TEXTURE,			"CONTENT_TEX"				},
		{ PayloadType::CUBEMAP,			"CONTENT_CUBEMAP"			},
		{ PayloadType::SHADER_VERTEX,	"CONTENT_SHADER_VERTEX"		},
		{ PayloadType::SHADER_PIXEL,	"CONTENT_SHADER_PIXEL"		},
		{ PayloadType::SHADER_COMPUTE,	"CONTENT_SHADER_COMPUTE"	},
		{ PayloadType::SAMPLER,			"CONTENT_SAMPLER"			},
		{ PayloadType::BLENDSTATE,		"CONTENT_BLENDSTATE"		},
		{ PayloadType::UNKNOWN,			"UNKNOWN"					}
	};

	struct EntityPayload
	{
		size_t sceneUID = 0;
		UINT entID = 0;

		EntityPayload(UINT entID, size_t sceneUID) : entID(entID), sceneUID(sceneUID) {}
	};
	struct BehaviourPayload
	{
		size_t sceneUID = 0;
		UINT entID = 0;
		UINT behIndex = 0;

		BehaviourPayload(UINT behIndex, UINT entID, size_t sceneUID) : behIndex(behIndex), entID(entID), sceneUID(sceneUID) {}
	};
	struct ContentPayload
	{
		UINT id = 0;

		ContentPayload(UINT id) : id(id) {}
	};

	void EntityDragDropSource(const Entity &ent, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None);
	bool EntityDragDropTarget(std::function<bool(EntityPayload &)> onDropCallback);

	template<typename P>
	void DragDropSource(const char *tag, P &payload, std::function<void(void)> preview, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None);
	template<typename P>
	bool DragDropTarget(const char *tag, std::function<bool(P &)> onDropCallback);

	template<typename P>
	void DragDropSource(PayloadType type, P &payload, std::function<void(void)> preview, ImGuiDragDropFlags flags = ImGuiDragDropFlags_None);
	template<typename P>
	bool DragDropTarget(PayloadType type, std::function<bool(P &)> onDropCallback);
};
