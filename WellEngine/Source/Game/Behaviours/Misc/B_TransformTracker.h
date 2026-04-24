#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <wrl/client.h>

#include "Game/Entity.h"
#include "Game/Behaviour.h"
#include "Engine/Content/Content.h"

namespace WellEngine
{
	class [[register_behaviour]] B_TransformTracker final : public Behaviour
	{
	public:
		std::string_view GetName() const override { return "TransformTracker"; }

	private:
		UINT _tempTrackedEntityID = CONTENT_NULL;
		Ref<Entity> _trackedEntity = nullptr;
		dx::XMFLOAT3 _offset = { 0, 0, 0 };

	protected:
		[[nodiscard]] bool Start() override;
		[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

	#ifdef USE_IMGUI
		// RenderUI runs every frame during ImGui rendering if the entity is selected.
		[[nodiscard]] bool RenderUI() override;
	#endif

	public:
		B_TransformTracker() = default;
		B_TransformTracker(Entity *trackedEnt, dx::XMFLOAT3 offset = {0, 0, 0})
			: _trackedEntity(*trackedEnt), _offset(offset) {}
		~B_TransformTracker() = default;

		[[nodiscard]] Entity *GetTrackedEntity() const;
		[[nodiscard]] const dx::XMFLOAT3 &GetOffset();

		void SetTrackedEntity(Entity *ent);
		void GetOffset(dx::XMFLOAT3 offset);

		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
		[[nodiscard]] bool PostDeserialize() override;
	};
}
