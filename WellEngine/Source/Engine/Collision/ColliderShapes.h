#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>

namespace Shape
{
	struct Ray
	{
		dx::XMFLOAT3 origin;
		dx::XMFLOAT3 direction;
		float length;

		Ray() : origin({ 0,0,0 }), direction({ 0,0,1 }), length(FLT_MAX) { }
		Ray(const dx::XMFLOAT3 &o, const dx::XMFLOAT3 &d, const float l = FLT_MAX)
			: origin(o), direction(d), length(l <= 0.0f ? FLT_MAX : l) { }

		Ray &Transform(const dx::XMFLOAT4X4 &matrix)
		{
			// Convert to math types
			dx::XMMATRIX mat = dx::XMLoadFloat4x4(&matrix);
			dx::XMVECTOR originVec = dx::XMLoadFloat3(&origin);
			dx::XMVECTOR directionVec = dx::XMLoadFloat3(&direction);

			// If a valid length is given, it must also be transformed.
			// We do this by scaling the direction vector by the length
			// and reading it back after transformation.
			bool validLength = (length > FLT_MIN && length < FLT_MAX);

			if (validLength)
			{
				directionVec = dx::XMVector3Normalize(directionVec);
				directionVec = dx::XMVectorScale(directionVec, length);
			}

			// Transform the values
			originVec = dx::XMVector3TransformCoord(originVec, mat);
			directionVec = dx::XMVector3TransformNormal(directionVec, mat);

			// Read back the transformed values
			if (validLength)
			{
				length = dx::XMVectorGetX(dx::XMVector3Length(directionVec));
				directionVec = dx::XMVectorScale(directionVec, 1.0f / length);
			}
			else
			{
				directionVec = dx::XMVector3Normalize(directionVec);
			}

			dx::XMStoreFloat3(&origin, originVec);
			dx::XMStoreFloat3(&direction, directionVec);

			return *this;
		}

		Ray Transformed(const dx::XMFLOAT4X4 &matrix) const
		{
			Ray newRay(*this);
			return newRay.Transform(matrix);
		}

		static inline const Ray &Transformed(const Ray &ray, const dx::XMFLOAT4X4 &matrix)
		{
			Ray newRay(ray);
			return newRay.Transform(matrix);
		}
	};

	struct RayHit
	{
		dx::XMFLOAT3 point;
		dx::XMFLOAT3 normal;
		float length;

		RayHit() : point({ 0,0,0 }), normal({ 0,0,1 }), length(0) { }
		RayHit(const dx::XMFLOAT3 &p, const dx::XMFLOAT3 &n, const float l)
			: point(p), normal(n), length(l) { }

		RayHit &Transform(const dx::XMFLOAT4X4 &matrix)
		{
			// Convert to math types
			dx::XMMATRIX mat = dx::XMLoadFloat4x4(&matrix);
			dx::XMVECTOR pointVec = dx::XMLoadFloat3(&point);
			dx::XMVECTOR normalVec = dx::XMLoadFloat3(&normal);

			// If a valid length is given, it must also be transformed.
			// We do this by scaling the normal vector by the length
			// and reading it back after transformation.
			bool validLength = (length > FLT_MIN && length < FLT_MAX);

			if (validLength)
			{
				normalVec = dx::XMVector3Normalize(normalVec);
				normalVec = dx::XMVectorScale(normalVec, length);
			}

			// Transform the values
			pointVec = dx::XMVector3TransformCoord(pointVec, mat);
			normalVec = dx::XMVector3TransformNormal(normalVec, mat);

			// Read back the transformed values
			if (validLength)
			{
				length = dx::XMVectorGetX(dx::XMVector3Length(normalVec));
				normalVec = dx::XMVectorScale(normalVec, 1.0f / length);
			}
			else
			{
				normalVec = dx::XMVector3Normalize(normalVec);
			}

			dx::XMStoreFloat3(&point, pointVec);
			dx::XMStoreFloat3(&normal, normalVec);

			return *this;
		}

		RayHit Transformed(const dx::XMFLOAT4X4 &matrix) const
		{
			RayHit newRayHit(*this);
			return newRayHit.Transform(matrix);
		}

		static inline const RayHit &Transformed(const RayHit &rayHit, const dx::XMFLOAT4X4 &matrix)
		{
			RayHit newRayHit(rayHit);
			return newRayHit.Transform(matrix);
		}
	};

	struct Tri
	{
		dx::XMFLOAT3 v0, v1, v2;

		Tri() = default;
		Tri(const dx::XMFLOAT3 &v0, const dx::XMFLOAT3 &v1, const dx::XMFLOAT3 &v2)
			: v0(v0), v1(v1), v2(v2) { }

		dx::XMFLOAT3 Normal() const
		{
			dx::XMVECTOR vec0 = dx::XMLoadFloat3(&v0);
			dx::XMVECTOR vec1 = dx::XMLoadFloat3(&v1);
			dx::XMVECTOR vec2 = dx::XMLoadFloat3(&v2);

			dx::XMVECTOR edge1 = dx::XMVectorSubtract(vec1, vec0);
			dx::XMVECTOR edge2 = dx::XMVectorSubtract(vec2, vec0);

			dx::XMVECTOR normalVec = dx::XMVector3Cross(edge1, edge2);
			normalVec = dx::XMVector3Normalize(normalVec);

			dx::XMFLOAT3 normal;
			dx::XMStoreFloat3(&normal, normalVec);
			return normal;
		}

		Tri &Transform(const dx::XMFLOAT4X4 &matrix)
		{
			// Convert to math types
			dx::XMMATRIX mat = dx::XMLoadFloat4x4(&matrix);
			dx::XMVECTOR v0Vec = dx::XMLoadFloat3(&v0);
			dx::XMVECTOR v1Vec = dx::XMLoadFloat3(&v1);
			dx::XMVECTOR v2Vec = dx::XMLoadFloat3(&v2);

			// Transform the values
			v0Vec = dx::XMVector3TransformCoord(v0Vec, mat);
			v1Vec = dx::XMVector3TransformCoord(v1Vec, mat);
			v2Vec = dx::XMVector3TransformCoord(v2Vec, mat);

			// Read back the transformed values
			dx::XMStoreFloat3(&v0, v0Vec);
			dx::XMStoreFloat3(&v1, v1Vec);
			dx::XMStoreFloat3(&v2, v2Vec);

			return *this;
		}

		Tri Transformed(const dx::XMFLOAT4X4 &matrix) const
		{
			Tri newTri(*this);
			return newTri.Transform(matrix);
		}

		static inline const Tri &Transformed(const Tri &tri, const dx::XMFLOAT4X4 &matrix)
		{
			Tri newTri(tri);
			return newTri.Transform(matrix);
		}
	};
};
