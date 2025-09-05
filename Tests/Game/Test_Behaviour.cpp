#include "stdafx.h"
#include "CppUnitTest.h"
#include "Game/Behaviour.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

void TestUtility::BehaviourTest(void *beh)
{
	Behaviour &b = *static_cast<Behaviour *>(beh);

}

namespace T_Game
{
	TEST_CLASS(T_Behaviour)
	{
	public:
		TEST_METHOD(Construct)
		{
			//Behaviour beh;

			//TestUtility::BehaviourTest(&beh);
		}
	};
}

