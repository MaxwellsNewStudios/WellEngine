#include "stdafx.h"
#include "CppUnitTest.h"
#include "Game/Entity.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

void TestUtility::EntityTest(void *ent)
{
	Entity &e = *static_cast<Entity *>(ent);

}

namespace T_Game
{
	TEST_CLASS(T_Entity)
	{
	public:
		TEST_METHOD(Construct)
		{
			Entity ent((UINT)0, dx::BoundingOrientedBox({ 0,0,0 }, { 1,1,1 }, {0,0,0,1}));

			TestUtility::EntityTest(&ent);
		}
	};
}

