#pragma once

#include <vector>
#include <DirectXMath.h>

namespace WellEngine::Curves
{
	namespace dx = DirectX;

	enum class CurveType
	{
		Linear,
		BezierQuadratic,
		BezierCubic
	};

	bool IsXMonotonic(const dx::XMFLOAT2& p0, const dx::XMFLOAT2& p1, const dx::XMFLOAT2& p2, const dx::XMFLOAT2& p3);

	struct Point
	{
		dx::XMFLOAT2 position = { };
		dx::XMFLOAT2 controlPoint1 = { };
		dx::XMFLOAT2 controlPoint2 = { };
	};

	dx::XMFLOAT2 Sample(const Point &lP, const Point &rP, CurveType type, float t);

	struct Curve
	{
		std::vector<Point> points;
		CurveType type = CurveType::BezierCubic;

		bool IsInjectiveAtPoint(int i) const;
		bool IsInjective() const;

		dx::XMFLOAT2 SamplePoint(float t) const;
		float SampleInjective(float x) const;
	};
};
