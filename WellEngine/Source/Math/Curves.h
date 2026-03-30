#pragma once
#include "DirectXMath.h"
#include <vector>

namespace Curves
{
	enum class CurveType
	{
		Linear,
		BezierQuadratic,
		BezierCubic
	};

	bool IsXMonotonic(const DirectX::XMFLOAT2& p0, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2, const DirectX::XMFLOAT2& p3);

	struct Point
	{
		DirectX::XMFLOAT2 position = { };
		DirectX::XMFLOAT2 controlPoint1 = { };
		DirectX::XMFLOAT2 controlPoint2 = { };
	};

	DirectX::XMFLOAT2 Sample(const Point &lP, const Point &rP, CurveType type, float t);

	struct Curve
	{
		std::vector<Point> points;
		CurveType type = CurveType::BezierCubic;

		bool IsInjectiveAtPoint(int i) const;
		bool IsInjective() const;

		DirectX::XMFLOAT2 SamplePoint(float t) const;
		float SampleInjective(float x) const;
	};
};
