#include "Tests/stdafx.h"
#include "CppUnitTest.h"
#include "Source/Game/Behaviours/Behaviour.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace TestUtils;

class TestUtils::TestUtility
{
public:
	static bool BehaviourTestConstruct()
	{
		//Scene scene{"TEST"};
		//scene.InitializeNull(nullptr, nullptr, nullptr, nullptr, nullptr);
		
		//Behaviour beh;

		return true;
	}
};

namespace T_Game
{
	TEST_CLASS(T_Behaviour)
	{
	public:
		TEST_METHOD(Construct)
		{
			if (!TestUtility::BehaviourTestConstruct())
			{
				Assert::Fail(L"Behaviour construction failed.");
			}
		}
	};
}

