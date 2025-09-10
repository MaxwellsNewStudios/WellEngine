#include "Tests/stdafx.h"
#include "CppUnitTest.h"
#include "Source/Game/Entity.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

class TestUtils::TestUtility
{
public:
	static bool EntityTestConstruct()
	{
		Entity ent(
			(UINT)0, 
			dx::BoundingOrientedBox({ 0,0,0 }, { 1,1,1 }, { 0,0,0,1 })
		);

		return true;
	}
};

namespace T_Game
{
	TEST_CLASS(T_Entity)
	{
	public:
		TEST_METHOD(Construct)
		{
			if (!TestUtility::EntityTestConstruct())
			{
				Assert::Fail(L"Entity construction failed.");
			}
		}
	};
}

