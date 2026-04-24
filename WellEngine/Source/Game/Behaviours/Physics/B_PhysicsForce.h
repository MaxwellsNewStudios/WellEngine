#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

#include "Game/Behaviours/Behaviour.h"

namespace WellEngine
{
	class [[register_behaviour]] B_PhysicsForce final : public Behaviour
	{
	public:
		std::string_view GetName() const override { return "PhysicsForce"; }

	private:
		dx::XMFLOAT3 _force = { 0, 0, 0 };
		dx::XMFLOAT3 _point = { 0, 0, 0 };
		bool _localSpace = true;

	#ifdef USE_IMGUI
		bool _debugDraw = false;
	#endif

		dx::XMFLOAT3 GetWorldForce() const;
		dx::XMFLOAT3 GetWorldPoint() const;

	protected:
		[[nodiscard]] bool Start() override;
		[[nodiscard]] bool PhysicsUpdate(float deltaTime) override;
	#ifdef USE_IMGUI
		[[nodiscard]] bool Update(TimeUtils &time, const Input &input) override;
		[[nodiscard]] bool RenderUI() override;
	#endif

		[[nodiscard]] bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;
		[[nodiscard]] bool Deserialize(const json::Value &obj, Scene *scene) override;

	public:
		B_PhysicsForce() = default;
		B_PhysicsForce(const dx::XMFLOAT3 &force, const dx::XMFLOAT3 &point = { 0, 0, 0 }, bool localSpace = true) : _force(force), _point(point), _localSpace(localSpace) {}
		~B_PhysicsForce() = default;
	};
}
