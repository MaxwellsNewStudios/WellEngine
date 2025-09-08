#include "Tests/stdafx.h"
#include "CppUnitTest.h"
#include "Game/Transform.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

class TestUtils::TestUtility
{
public:
	static bool TransformTestConstruct()
	{
		Transform t{};

		t._localMatrix;

		return true;
	}
};

namespace T_Game
{
	TEST_CLASS(T_Transform)
	{
	public:
		TEST_METHOD(Construct)
		{
			if (!TestUtility::TransformTestConstruct())
			{
				Assert::Fail(L"Transform construction failed.");
			}
		}
	};
}
