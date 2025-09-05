#pragma once

#include "EngineSettings.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <string>
#include <functional>

#include "Timing/TimeUtils.h"
#include "Input/Input.h"
#include "Rendering/RenderQueuer.h"
#include "Rendering/RendererInfo.h"

#include "Content/HeightMap.h"

class Entity;
class Scene;
class Content;

namespace Collisions
{
	struct CollisionData;

	enum ColliderTypes
	{
		RAY_COLLIDER,
		SPHERE_COLLIDER,
		CAPSULE_COLLIDER,
		AABB_COLLIDER,
		OBB_COLLIDER,
		TERRAIN_COLLIDER,
		NULL_COLLIDER
	};

	enum ColliderTags
	{
		NULL_TAG = 0x0,
		GROUND_TAG = 0x1,
		OBJECT_TAG = 0x2,
		SKIP_TERRAIN_TAG = 0x4,
		STATIC_TAG = 0x8,
		TEMP4_TAG = 0x10,
		TEMP5_TAG = 0x20,
		TEMP6_TAG = 0x40,
		TEMP7_TAG = 0x80
	};

	constexpr ColliderTags MAP_COLLIDER_TAGS = static_cast<Collisions::ColliderTags>(Collisions::SKIP_TERRAIN_TAG | Collisions::STATIC_TAG);

	struct Collider 
	{
		Collider() = default;
		Collider(ColliderTypes type, const std::string &debugMeshName, ColliderTags tag);
		 
		virtual ~Collider();

		virtual void Transform(dx::XMFLOAT4X4A M, Collider *transformed) = 0;

#ifdef USE_IMGUI
		[[nodiscard]] virtual bool RenderUI();
#endif
#ifdef DEBUG_BUILD
		[[nodiscard]] bool SetDebugEnt(Scene* scene, const Content* content);
		[[nodiscard]] virtual bool RenderDebug(Scene* scene, const RenderQueuer &queuer, const RendererInfo &renderInfo);

		virtual bool TransformDebug(ID3D11DeviceContext *context, TimeUtils &time, const Input &input) = 0;
#endif

		void AddIntersectionCallback(std::function<void(const CollisionData&)> callback);
		void AddOnCollisionEnterCallback(std::function<void(const CollisionData&)> callback);
		void AddOnCollisionExitCallback(std::function<void(const CollisionData&)> callback);

		void Intersection(const CollisionData &data) const;
		void OnCollisionEnter(const CollisionData &data) const;
		void OnCollisionExit(const CollisionData &data) const;

		dx::XMFLOAT3 GetMin() const;
		dx::XMFLOAT3 GetMax() const;

		void SetTag(ColliderTags tag);
		void RemoveTag(ColliderTags tag);
		bool HasTag(ColliderTags tag) const;
		uint8_t GetTag() const;

		void SetActive(bool status);
		bool GetActive() const;

		bool GetDirty() const;

#ifdef DEBUG_BUILD
		bool debug = true;
		bool haveDebugEnt = false;
#endif
		ColliderTypes colliderType = NULL_COLLIDER;

	protected:
#ifdef DEBUG_BUILD
		Entity* _debugEnt = nullptr;
		std::string _debugMeshName;
#endif
		bool _isDirty{};
		bool _active{};

		dx::XMFLOAT3 _min{}, _max{};

		uint8_t _tagFlag = 0;

		// Serializes the behaviour to a string.
		[[nodiscard]] virtual bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj);

		// Deserializes the behaviour from a string.
		[[nodiscard]] virtual bool Deserialize(const json::Value &obj, Scene *scene);

	private:
		std::vector<std::function<void(const CollisionData&)>> _onCollisionEnterCallbacks;
		std::vector<std::function<void(const CollisionData&)>> _onCollisionExitCallbacks;
		std::vector<std::function<void(const CollisionData&)>> _intersectionCallbacks;

		TESTABLE()
	};

	struct Ray: public Collider
	{
		dx::XMFLOAT3 origin{};
		dx::XMFLOAT3 dir{};
		float length{};

		Ray() = default;
		Ray(const Ray &other);
		Ray(const dx::XMFLOAT3 &o, const dx::XMFLOAT3 &d, float l, ColliderTags tag = NULL_TAG);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif


#ifdef DEBUG_BUILD
		[[nodiscard]] bool RenderDebug(Scene* scene, const RenderQueuer &queuer, const RendererInfo &renderInfo) override;
		void DebugRayRender(Scene *scene, dx::XMFLOAT4 color = {0, 1, 0, 1}) const;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

		TESTABLE()
	};

	struct Sphere: public Collider 
	{
		dx::XMFLOAT3 center;
		float radius;

		Sphere() = default;
		Sphere(const Sphere &other);
		Sphere(const dx::XMFLOAT3 &c, float r, ColliderTags tag = NULL_TAG);
		Sphere(const dx::BoundingSphere &sphere, ColliderTags tag = NULL_TAG);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

		TESTABLE()
	};


	struct Capsule: public Collider
	{
		dx::XMFLOAT3 center, upDir;
		float radius, height;

		Capsule() = default;
		Capsule(const Capsule &other);
		Capsule(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &u, float r, float h, ColliderTags tag = NULL_TAG);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

		TESTABLE()
	};

	struct AABB: public Collider
	{
		dx::XMFLOAT3 center, halfLength;

		AABB() = default;
		AABB(const AABB &other);
		AABB(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, ColliderTags tag = NULL_TAG);
		AABB(const dx::BoundingBox &aabb, ColliderTags tag = NULL_TAG);
		AABB(const dx::BoundingOrientedBox &obb, ColliderTags tag = NULL_TAG);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

		TESTABLE()
	};

	struct OBB: public Collider 
	{
		dx::XMFLOAT3 center, halfLength, axes[3];

		OBB() = default;
		OBB(const OBB &other);
		OBB(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const dx::XMFLOAT3 a[3], ColliderTags tag = NULL_TAG);
		OBB(const dx::BoundingOrientedBox &obb, ColliderTags tag = NULL_TAG);
		OBB(const AABB &aabb, ColliderTags tag = NULL_TAG);

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

		TESTABLE()
	};


	struct Terrain : public Collider
	{
		dx::XMFLOAT3 center, halfLength;
		dx::XMINT2 tileSize;
		std::vector<float> heightValues;
		std::vector<bool> walls;
		std::string map;
		
		Terrain() = default;
		Terrain(const Terrain &other);

		Terrain(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const HeightMap *heightMap, ColliderTags tag = NULL_TAG, bool invertHeight = false, bool isWallCollider = false);
		Terrain(const dx::BoundingBox &aabb, const HeightMap *heightMap, ColliderTags tag = NULL_TAG, bool invertHeight = false, bool isWallCollider = false);

		void Init(const dx::XMFLOAT3 &c, const dx::XMFLOAT3 &hl, const std::vector<float> &heightMap, UINT width, UINT height, ColliderTags tag = NULL_TAG);

		bool GetHeight(UINT x, UINT y, float &height) const;
		bool GetHeight(const dx::XMFLOAT2 &pos, float &height) const;
		bool GetHeight(const dx::XMFLOAT2 &pos, float &height, dx::XMFLOAT3 &normal) const;

		bool GetNormal(UINT x, UINT y, dx::XMFLOAT3 &normal) const;
		bool GetNormal(const dx::XMFLOAT2 &pos, dx::XMFLOAT3 &normal) const;

		bool IsWall(const dx::XMFLOAT2 &pos) const;
		bool IsWall(UINT x, UINT y) const;

		dx::XMFLOAT2 GetTileWorldSize() const;

		bool IsInverted() const;
		bool IsWallCollider() const;

#ifdef DEBUG_BUILD
		[[nodiscard]] bool RenderDebug(Scene* scene, const RenderQueuer &queuer, const RendererInfo &renderInfo) override;
#endif

		// Serializes the behaviour to a string.
		bool Serialize(json::Document::AllocatorType &docAlloc, json::Value &obj) override;

		~Terrain() = default;

#ifdef USE_IMGUI
		[[nodiscard]] bool RenderUI() override;
#endif

		void Transform(dx::XMFLOAT4X4A M, Collider *transformed) override;
#ifdef DEBUG_BUILD
		bool TransformDebug(ID3D11DeviceContext* context, TimeUtils &time, const Input &input) override;
#endif

	private:
		int _selected[2] = { 0, 0 };
		float _uv[2] = { 0.209f, 0.077f };
		int _selectedSize[2] = { 1, 1 };
		int _selectedSkip = 4;
		float _debugLineSize = 1;
		dx::XMFLOAT4A _debugColor = { 0, 1, 0, 0.5f };

		UINT _minIndex, _maxIndex;
		float _heightScale;
		bool _invertHeight;
		bool _isWallCollider;
		bool _debugFlip = false;

		TESTABLE()
	};
}
