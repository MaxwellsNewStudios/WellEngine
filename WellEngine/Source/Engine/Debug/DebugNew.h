#pragma once
#include "TrackedAlloc.h"

#ifdef LEAK_DETECTION
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include "crtdbg.h"

#define DEBUG_NEW   new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DEBUG_NEW   new
#endif

#ifdef TRACY_ENABLE
#define DEBUG_TRACE_DEF
#define DEBUG_TRACE(ptr)	TracyAlloc(ptr, sizeof(*ptr));
#define DEBUG_TRACE_ARR(ptr, count)	TracyAlloc(ptr, sizeof(*ptr) * count);
#endif

#ifndef DEBUG_TRACE_DEF
#define DEBUG_TRACE_DEF
#define DEBUG_TRACE(x);
#define DEBUG_TRACE_ARR(x, y);
#endif
