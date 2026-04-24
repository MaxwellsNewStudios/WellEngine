#pragma once

#include "Game/Behaviour.h"
#include "B_Mesh.h"

namespace WellEngine
{
	class [[register_behaviour]] B_MeshBillboard : public Behaviour
	{
	public:
		std::string_view GetName() const override { return "MeshBillboard"; }

	private:
		Ref<B_Mesh> _meshBehaviour = nullptr;
		Material _material;

		bool _transparent = true;
		bool _castShadows = false;
		bool _overlay = false;
		bool _gizmo = false;

		bool _keepUpright = true;
		float _rotation = 0.0f;
		float _normalOffset = 0.0f;
		float _scale = 1.0f;

	protected:
		[[nodiscard]] bool Start() override;

		[[nodiscard]] bool ParallelUpdate(const TimeUtils &time, const Input &input) override;

	#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
	#endif

		void OnEnable() override;
		void OnDisable() override;

		// Serializes the behaviour to a string.
		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		// Deserializes the behaviour from a string.
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	public:
		B_MeshBillboard() = default;
		~B_MeshBillboard();

		B_MeshBillboard(
			const Material &material, float rotation, float normalOffset, float size, 
			bool keepUpright, bool isTransparent, bool castShadows, bool isOverlay, bool isGizmo = false);

		void SetSize(float size);
		void SetRotation(float rotation);
		B_Mesh *GetMeshBehaviour() const;
	};
}
