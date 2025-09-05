#include "stdafx.h"
#include "TrackedAlloc.h"
#include <corecrt_malloc.h>
#include <new>

#ifdef TRACY_MEMORY
void *operator new(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
	{
		throw std::bad_alloc();
	}
	TracyAlloc(ptr, size);
	return ptr;
}
void *operator new(size_t size, int blockUse, char const* fileName, int lineNumber)
{
	void *ptr = malloc(size);
	if (!ptr)
	{
		throw std::bad_alloc();
	}
	TracyAlloc(ptr, size);
	return ptr;
}

void *operator new[](size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
	{
		throw std::bad_alloc();
	}
	TracyAlloc(ptr, size);
	return ptr;
}
void *operator new[](size_t size, int blockUse, char const* fileName, int lineNumber)
{
	void *ptr = malloc(size);
	if (!ptr)
	{
		throw std::bad_alloc();
	}
	TracyAlloc(ptr, size);
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	if (ptr)
	{
		TracyFree(ptr);
		free(ptr);
	}
}
void operator delete(void *ptr, size_t size) noexcept
{
	if (ptr)
	{
		TracyFree(ptr);
		free(ptr);
	}
}

void operator delete[](void *ptr) noexcept
{
	if (ptr)
	{
		TracyFree(ptr);
		free(ptr);
	}
}
void operator delete[](void *ptr, size_t size) noexcept
{
	if (ptr)
	{
		TracyFree(ptr);
		free(ptr);
	}
}
#endif
