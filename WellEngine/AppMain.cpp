#include "stdafx.h"
#include "EngineCore.h"

namespace Engine
{
	int AppMain()
	{
		int ret = 0;
		{
#ifdef LEAK_DETECTION
			// Get current flag
			int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

			// Turn on leak-checking bit.
			tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

			// Set flag to the new value.
			_CrtSetDbgFlag(tmpFlag);
#endif

			EngineCore engine{};

			ret = engine.Init();
			if (ret != 0)
			{
				ErrMsgF("Engine failure during Init! Code: {}", ret);
				return ret;
			}

			ret = engine.Run();
			if (ret != 0)
			{
				ErrMsgF("Engine failure during Run! Code: {}", ret);
				return ret;
			}
		}
		return ret;
	}
}