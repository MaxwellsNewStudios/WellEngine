#include "stdafx.h"
#include "CppUnitTest.h"
#include "Math/GameMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

namespace T_Math
{
	TEST_CLASS(T_DXMath)
	{
	public:
		TEST_METHOD(Vec3_Construct)
		{
			using namespace DXMath;

			Vec3 v1;
			Vec3 v2(1.0f, 2.0f, 3.0f);
			Vec3 v3(v2);
			Vec3 v4 = v2;
			Vec3 v5 = Vec3(4.0f, 5.0f, 6.0f);
			Vec3 v6 = dx::XMVectorSet(1.0f, 0.5f, -3.0f, 0.0f);
			Vec3 v7 = dx::XMFLOAT3(7.0f, 8.0f, 9.0f);
			Vec3 v8 = std::move(v2);
		}
	};
};
