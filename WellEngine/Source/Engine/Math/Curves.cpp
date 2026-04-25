#include "stdafx.h"
#include "Curves.h"
#include "GameMath.h"
#include "Bezier.h"

#ifdef LEAK_DETECTION
#define new			DEBUG_NEW
#endif

namespace WellEngine::Curves
{
	using namespace DirectX;

	bool IsXMonotonic(const XMFLOAT2& p0, const XMFLOAT2& p1, const XMFLOAT2& p2, const XMFLOAT2& p3)
	{
		// Compute coefficients of the quadratic derivative x'(t)/3
		float a = 3 * (p0.x - 3 * p1.x + 3 * p2.x - p3.x);
		float b = 6 * (p1.x - 2 * p2.x + p3.x);
		float c = 3 * (p1.x - p0.x);

		// Standard expanded quadratic for dx/dt:
		float A = -3 * p0.x + 9 * p1.x - 9 * p2.x + 3 * p3.x;
		float B = 6 * p0.x - 12 * p1.x + 6 * p2.x;
		float C = -3 * p0.x + 3 * p1.x;

		// Discriminant
		float disc = B * B - 4 * A * C;
		if (disc < 0)
			return true; // No real roots = quadratic doesn't cross zero

		// Roots
		float sqrtD = sqrtf(disc);
		float t1 = (-B - sqrtD) / (2 * A);
		float t2 = (-B + sqrtD) / (2 * A);

		// Check if any root lies strictly inside (0,1)
		bool hasInteriorRoot = false;

		if (A != 0)
		{
			// Quadratic case
			if (t1 > 0 && t1 < 1) hasInteriorRoot = true;
			if (t2 > 0 && t2 < 1) hasInteriorRoot = true;
		}
		else if (B != 0)
		{
			// Linear case (degenerate)
			float t = -C / B;
			if (t > 0 && t < 1) hasInteriorRoot = true;
		}

		return !hasInteriorRoot;
	}

	XMFLOAT2 Sample(const Point& lP, const Point& rP, CurveType type, float t)
	{
		switch (type)
		{
			default:
			case CurveType::Linear:
				return Lerp(lP.position, rP.position, t);

			case CurveType::BezierQuadratic:
			{
				XMVECTOR p0 = Load(lP.position);
				XMVECTOR p1 = Load(lP.controlPoint1);
				XMVECTOR p2 = Load(rP.position);

				XMVECTOR result = Bezier::QuadraticInterpolate(p0, p1, p2, t);
				XMFLOAT2 resultFloat2;
				Store(resultFloat2, result);
				return resultFloat2;
			}

			case CurveType::BezierCubic:
			{
				XMVECTOR p0 = Load(lP.position);
				XMVECTOR p1 = Load(lP.controlPoint2);
				XMVECTOR p2 = Load(rP.controlPoint1);
				XMVECTOR p3 = Load(rP.position);

				XMVECTOR result = Bezier::CubicInterpolate(p0, p1, p2, p3, t);
				XMFLOAT2 resultFloat2;
				Store(resultFloat2, result);
				return resultFloat2;
			}
		}
	}

	bool Curve::IsInjectiveAtPoint(int i) const
	{
		const Point& lP = points[i];
		const Point& rP = points[i + 1ll];

		if (lP.position.x >= rP.position.x)
			return false;

		switch (type)
		{
			default:
			case CurveType::Linear:
				return true;

			case CurveType::BezierQuadratic: 
			{
				// For a quadratic Bezier curve defined by points P0, P1, P2, 
				// the curve is non-injective if P1.x is not between P0.x and P2.x.
				XMFLOAT2 p0 = lP.position;
				XMFLOAT2 p1 = lP.controlPoint1;
				XMFLOAT2 p2 = rP.position;

				if (p1.x < p0.x)
					return false;

				if (p1.x > p2.x)
					return false;
				break;
			}

			case CurveType::BezierCubic: 
			{
				// For a cubic Bezier curve defined by points P0, P1, P2, P3,
				// the curve is non-injective if P1.x is less than P0.x or P2.x is greater than P3.x.
				// The curve may be non-injective if P1.x is greater than P3.x or P2.x is less than P0.x, depending on the x-value of the other control point.
				XMFLOAT2 p0 = lP.position;
				XMFLOAT2 p1 = lP.controlPoint2;
				XMFLOAT2 p2 = rP.controlPoint1;
				XMFLOAT2 p3 = rP.position;

				if (p1.x < p0.x)
					return false;

				if (p2.x > p3.x)
					return false;

				if (p1.x > p3.x || p2.x < p0.x)
				{
					// Possibly non-injective
					if (!IsXMonotonic(p0, p1, p2, p3))
						return false;
				}
				break;
			}
		}

		return true;
	}

	bool Curve::IsInjective() const
	{
		int c = points.size();

		if (c <= 1)
			return true;

		for (int i = 0; i < c - 1; i++)
		{
			if (!IsInjectiveAtPoint(i))
				return false;
		}

		return true;
	}

	XMFLOAT2 Curve::SamplePoint(float t) const
	{
		int pointCount = points.size();
		t *= (float)(pointCount - 1); // Scale t to the number of segments

		int segment = (int)t;
		t -= segment; // Get fractional part for interpolation

		return Sample(points[segment], points[segment + 1ll], type, t);
	}

	float Curve::SampleInjective(float x) const
	{
		int c = points.size();
		if (c <= 1)
			return 0;

		// Find the segment that contains the specified x-value
		int i = -1;
		const Point *lPoint = nullptr, *rPoint = nullptr;

		do
		{
			i++;
			lPoint = &(points[i]);
			rPoint = &(points[i + 1ll]);
		}
		while (i < c - 2 && rPoint->position.x < x);

		if (i >= c - 1)
			return 0; // x is out of bounds

		// Sample the curve at the specified x-value using binary search
		float tMin = 0, tMax = 1;
		float y = 0;

		int iterations = 0;
		while (tMax - tMin > 1e-4f) // Precision threshold
		{
			float tMid = (tMin + tMax) * 0.5f;
			XMFLOAT2 sample = Sample(*lPoint, *rPoint, type, tMid);
			y = sample.y;

			if (sample.x < x)
			{
				tMin = tMid;
			}
			else
			{
				tMax = tMid;
			}

			if (iterations++ > 20) // Safety check to prevent infinite loop
				break;
		}

		return y;
	}
};
