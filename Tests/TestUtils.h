#pragma once

#ifdef _TESTING


namespace TestUtils
{
	class TestUtility;

#ifdef _TEST_PROJ
	class TestUtility
	{
	public:
		static void TransformTest(void *t);
		static void EntityTest(void *e);
		static void BehaviourTest(void *b);
		// Add more test functions as needed...
	};
#endif // _TEST_PROJ
};

#define TESTABLE() friend class TestUtils::TestUtility;


#else


#define TESTABLE() 


#endif // _TESTING
