#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "ColliderShapes.h"
#include "Math/GameMath.h"

namespace dx = DirectX;

struct RaycastOut
{
    Entity *entity = nullptr;
    float distance = FLT_MAX;
};

static bool Raycast(
    const dx::XMFLOAT3 &origin, const dx::XMFLOAT3 &direction, 
    const dx::BoundingBox &box, float &length)
{
	if (box.Contains(XMLoadFloat3(&origin)))
	{
		length = 0.0f;
		return true;
	}

	if (box.Intersects(XMLoadFloat3(&origin), XMLoadFloat3(&direction), length))
	{
		return true;
	}

    return false;
}

static bool Raycast(
    const dx::XMFLOAT3 &origin, const dx::XMFLOAT3 &direction,
    const dx::BoundingOrientedBox &box, float &length)
{
    /*if (box.Contains(XMLoadFloat3(&origin)))
    {
        length = 0.0f;
        return true;
    }*/

	length = FLT_MAX;

	dx::XMFLOAT3 p;
	dx::XMFLOAT3 normal;
	int side;

	float minV = -1 * FLT_MAX,
		maxV = FLT_MAX;

	dx::XMVECTOR
		r0 = dx::XMLoadFloat3(&origin),
		rD = dx::XMLoadFloat3(&direction);

	dx::XMVECTOR
		orientation = dx::XMLoadFloat4(&box.Orientation),
		halfLength = dx::XMLoadFloat3(&box.Extents),
		center = dx::XMLoadFloat3(&box.Center),
		axes[3] = {
			dx::XMVector3Rotate({ 1, 0, 0 }, orientation),
			dx::XMVector3Rotate({ 0, 1, 0 }, orientation),
			dx::XMVector3Rotate({ 0, 0, 1 }, orientation)
	};

	dx::XMVECTOR
		rayToCenter = dx::XMVectorSubtract(center, r0),
		nMin = dx::XMVectorSet(0, 0, 0, 0),
		nMax = dx::XMVectorSet(0, 0, 0, 0);

	for (int a = 0; a < 3; a++)
	{
		dx::XMVECTOR axis = axes[a];
		float hl = halfLength.m128_f32[a];

		float distAlongAxis = dx::XMVector3Dot(axis, rayToCenter).m128_f32[0],
			f = dx::XMVector3Dot(axis, rD).m128_f32[0];

		if (fabsf(f) > FLT_MIN)
		{
			dx::XMVECTOR tnMin = axis,
				tnMax = dx::XMVectorScale(axis, -1);

			float t0 = (distAlongAxis + hl) / f,
				t1 = (distAlongAxis - hl) / f;

			if (t0 > t1) // Flip Intersection Order
			{
				float temp = t0;
				t0 = t1;
				t1 = temp;

				tnMin = tnMax;
				tnMax = axis;
			}

			if (t0 > minV) // Keep the longer entry-point
			{
				minV = t0;
				nMin = tnMin;
			}

			if (t1 < maxV) // Keep the shorter exit-point
			{
				maxV = t1;
				nMax = tnMax;
			}

			if (minV > maxV) return false; // Ray misses OBB
			if (maxV < 0.0f) return false; // OBB is behind Ray
		}
		else if (-distAlongAxis - hl > 0.0f || -distAlongAxis + hl < 0.0f)
		{
			return false; // Ray is orthogonal to axis but no located between the axis-planes
		}
	}

	// Find closest positive intersection
	if (minV > 0.0f)
	{
		length = minV;
		dx::XMStoreFloat3(&normal, dx::XMVector3Normalize(nMin));
		side = 1;
	}
	else
	{
		length = maxV;
		dx::XMStoreFloat3(&normal, dx::XMVectorNegate(dx::XMVector3Normalize(dx::XMVectorNegate(nMax))));
		side = -1;
	}

	dx::XMStoreFloat3(&p, dx::XMVectorAdd(r0, dx::XMVectorScale(rD, length)));
	return true;
}

static bool Raycast(
	const Shape::Ray &ray, const Shape::Tri &tri, 
	Shape::RayHit &hit)
{
	using namespace DXMath;
	constexpr float MINVAL = 0.000025f;

	Vec3 rO = Load(ray.origin);
	Vec3 rD = Load(ray.direction);

	Vec3 v0 = Load(tri.v0);
	Vec3 v1 = Load(tri.v1);
	Vec3 v2 = Load(tri.v2);

	Vec3 edge1 = v1 - v0;
	Vec3 edge2 = v2 - v0;

	// Backface-culling
	Vec3 iN = Vec3::Cross3(edge1, edge2);
	if (Vec3::Dot3(iN, rD) >= 0.0f)
		return false;

	Vec3 h = Vec3::Cross3(rD, edge2);
	float a = Vec3::Dot3(edge1, h);

	if (a > -MINVAL && a < MINVAL)
	{
		float scaledMinVal = MINVAL * min(edge1.Length3(), edge2.Length3());
		if (a > -scaledMinVal && a < scaledMinVal)
			return false;
	}

	Vec3 s = rO - v0;
	float f = 1.0f / a;
	float u = f * Vec3::Dot3(s, h);

	if (u < 0.0f || u > 1.0f)
		return false;

	Vec3 q = Vec3::Cross3(s, edge1);
	float v = f * Vec3::Dot3(rD, q);

	if (v < 0.0f || u + v > 1.0f)
		return false;

	float t = f * Vec3::Dot3(edge2, q);

	if (t <= 0.0f)
		return false;

	hit.point = (rO + rD * t).AsMem();
	hit.normal = Vec3::Cross3(edge1, edge2).Normalized3().AsMem();
	hit.length = t;

	return true;
}
