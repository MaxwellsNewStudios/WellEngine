#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "Collision/Colliders.h"

namespace Collisions
{
	struct CollisionData {
		dx::XMFLOAT3 normal;
		dx::XMFLOAT3 point;
		float depth;
		const Collider* other;
	};

	struct LineSegment
	{
		dx::XMFLOAT3 start, end;
	};

	struct Circle
	{
		dx::XMFLOAT2 center;
		float radius;
	};

	struct Box
	{
		dx::XMFLOAT2 center, halfLength;
	};

	// Check if c1 intersects with c2
	bool CheckIntersection(const Collider *c1, const Collider *c2, CollisionData& data);

	dx::XMFLOAT3 GetMin(const AABB &obb);
	dx::XMFLOAT3 GetMax(const AABB &obb);

	dx::XMFLOAT3 ClosestPoint(const LineSegment &segment, const OBB &obb);
	dx::XMFLOAT3 ClosestPoint(const LineSegment &segment, const dx::XMFLOAT3 &point);
	dx::XMFLOAT3 ClosestPoint(const LineSegment &segment1, const LineSegment &segment2);
	dx::XMFLOAT3 ClosestPoint(const OBB &obb, const dx::XMFLOAT3 &point);
	dx::XMFLOAT3 ClosestFacePoint(const OBB &obb, const dx::XMFLOAT3 &point, dx::XMFLOAT3 &normal, float &depth);

	bool PointInside(const OBB &obb, const dx::XMFLOAT3 &point);

	bool CircleBoxIntersection(const Circle &c, const Box &b, float &depth, dx::XMFLOAT2 &normal);
	bool CircleTerrainWallIntersection(const Circle &c, const Terrain &t, float &depth, dx::XMFLOAT2 &normal);

	bool RaySphereIntersection(const Ray &ray, const Sphere &s, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool RayCapsuleIntersection(const Ray &ray, const Capsule &c, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool RayOBBIntersection(const Ray &ray, const OBB &obb, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool RayAABBIntersection(const Ray &ray, const AABB &aabb, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool RayTerrainIntersection(const Ray &ray, const Terrain &t, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);

	bool SphereRayIntersection(const Sphere &s, const Ray &ray, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool SphereSphereIntersection(const Sphere &s1, const Sphere &s2, dx::XMFLOAT3& normal, float &depth);
	bool SphereCapsuleIntersection(const Sphere &s, const Capsule &c, dx::XMFLOAT3 &normal, float &depth);
	bool SphereAABBIntersection(const Sphere &s, const AABB &aabb, dx::XMFLOAT3 &normal, float &depth);
	bool SphereOBBIntersection(const Sphere &s, const OBB &obb, dx::XMFLOAT3 &normal, float &depth);
	bool SphereTerrainIntersection(const Sphere &s, const Terrain &t, dx::XMFLOAT3 &normal, float &depth);

	bool CapsuleRayIntersection(const Capsule &c, const Ray &ray, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool CapsuleCapsuleIntersection(const Capsule &c1, const Capsule &c2, dx::XMFLOAT3 &normal, float &depth);
	bool CapsuleSphereIntersection(const Capsule &c, const Sphere &s, dx::XMFLOAT3 &normal, float &depth);
	bool CapsuleAABBIntersection(const Capsule &c, const AABB &aabb, dx::XMFLOAT3 &normal, float &depth);
	bool CapsuleOBBIntersection(const Capsule &c, const OBB &obb, dx::XMFLOAT3 &normal, float &depth);
	bool CapsuleTerrainIntersection(const Capsule &c, const Terrain &t, dx::XMFLOAT3 &normal, float &depth);

	bool OBBRayIntersection(const OBB &obb, const Ray &ray, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool OBBOBBIntersection(const OBB &obb1, const OBB &obb2, dx::XMFLOAT3 &normal, float &depth);
	bool OBBSphereIntersection(const OBB &obb, const Sphere &s, dx::XMFLOAT3 &normal, float &depth);
	bool OBBCapsuleIntersection(const OBB &obb, const Capsule &c, dx::XMFLOAT3 &normal, float &depth);
	bool OBBAABBIntersection(const OBB &obb, const AABB &aabb, dx::XMFLOAT3 &normal, float &depth);
	bool OBBTerrainIntersection(const OBB &obb, const Terrain &t, dx::XMFLOAT3 &normal, float &depth);

	bool AABBRayIntersection(const AABB &aabb, const Ray &ray, float &l, dx::XMFLOAT3 &p, dx::XMFLOAT3 &normal);
	bool AABBAABBIntersection(const AABB &aabb1, const AABB &aabb2, dx::XMFLOAT3 &normal, float &depth);
	bool AABBSphereIntersection(const AABB &aabb, const Sphere &s, dx::XMFLOAT3 &normal, float &depth);
	bool AABBCapsuleIntersection(const AABB &aabb, const Capsule &c, dx::XMFLOAT3 &normal, float &depth);
	bool AABBOBBIntersection(const AABB &aabb, const OBB &obb, dx::XMFLOAT3 &normal, float &depth);
	bool AABBTerrainIntersection(const AABB &aabb, const Terrain &t, dx::XMFLOAT3 &normal, float &depth);

	bool TerrainRayIntersectionVertical(const Terrain &t, const Ray &r, dx::XMFLOAT3 &normal, dx::XMFLOAT3 &point, float &depth, bool &under);
	bool TerrainRayWallIntersectionHorizontal(const Terrain &t, const Ray &r, dx::XMFLOAT3 &normal, dx::XMFLOAT3 &point, float &depth);
	bool TerrainRayIntersection(const Terrain &t, const Ray &r, dx::XMFLOAT3 &normal, dx::XMFLOAT3 &point, float &depth);
	bool TerrainSphereIntersection(const Terrain &t, const Sphere &s, dx::XMFLOAT3 &normal, float &depth);
	bool TerrainCapsuleIntersection(const Terrain &t, const Capsule &c, dx::XMFLOAT3 &normal, float &depth);
	bool TerrainOBBIntersection(const Terrain &t, const OBB &obb, dx::XMFLOAT3 &normal, float &depth);
	bool TerrainAABBIntersection(const Terrain &t, const AABB &aabb, dx::XMFLOAT3 &normal, float &depth);
}

