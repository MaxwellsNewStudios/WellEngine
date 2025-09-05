#include "stdafx.h"
#include "CppUnitTest.h"
#include "Game/Transform.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

void TestUtility::TransformTest(void *trans)
{
	Transform &t = *static_cast<Transform *>(trans);

	t._localMatrix;
}

namespace T_Game
{
	TEST_CLASS(T_Transform)
	{
	public:
		TEST_METHOD(Construct)
		{
			Transform t{};
			TestUtility::TransformTest(&t);
		}
	};
}
