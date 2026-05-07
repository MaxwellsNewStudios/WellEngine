#pragma once
#pragma once

#include <string_view>
#include <vector>

#include "B_Mesh.h"

namespace WellEngine
{
	class [[register_behaviour]] B_MeshSkinned : public B_Mesh
	{
	public:
		std::string_view GetName() const override { return "MeshSkinned"; }
		std::string_view GetScriptPath() const override { return __FILE__; }

	private:
		std::vector<Ref<Entity>> _bones;

	protected:
		[[nodiscard]] bool Start() override;
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
		[[nodiscard]] bool Render(RenderQueuer &queuer, const RendererInfo &rendererInfo) override;
		[[nodiscard]] bool BindBuffers(ID3D11DeviceContext *context, UINT submeshIndex) override;

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
	#endif

		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
		[[nodiscard]] bool PostDeserialize() override;

	public:
		B_MeshSkinned() = default;
		~B_MeshSkinned() = default;

		void StoreBounds(dx::BoundingOrientedBox &meshBounds) override;
	};
}
