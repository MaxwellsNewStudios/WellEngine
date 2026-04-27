#pragma once

#include "Game/Behaviours/Behaviour.h"
#include "Engine/Rendering/RenderQueuer.h"

namespace WellEngine
{
	class [[register_behaviour]] B_Example final : public Behaviour
	{
	public:
		std::string_view GetName() const override { return "Example"; }
		std::string_view GetScriptPath() const override { return __FILE__; }

	private:
		bool _hasCreatedChild = false;
		bool _debugDraw = false;
		bool _overlay = false;
		bool _firstFrame = true;
		float _spinSpeed = 1.0f;

	protected:
		// Start runs once when the behaviour is created.
		[[nodiscard]] bool Start() override;

		// Update runs every frame.
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;

	#ifdef USE_IMGUI
		// RenderUI runs every frame during ImGui rendering if the entity is selected.
		[[nodiscard]] bool RenderUI() override;
	#endif

	public:
		B_Example() = default;
		~B_Example() = default;

		// Serializes the behaviour to a string.
		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		// Deserializes the behaviour from a string.
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;
	};
}
