#pragma once

#ifdef _TESTING


namespace TestUtils
{
	class TestUtility;
};

#define TESTABLE() friend class TestUtils::TestUtility;

#undef TRACY_ENABLE


#else


#define TESTABLE() 


#endif // _TESTING
