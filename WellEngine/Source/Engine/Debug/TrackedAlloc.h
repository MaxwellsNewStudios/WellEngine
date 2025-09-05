#pragma once

#ifdef TRACY_MEMORY
void *operator new(size_t size);
void *operator new(size_t size, int blockUse, char const* fileName, int lineNumber);
void *operator new[](size_t size);
void *operator new[](size_t size, int blockUse, char const* fileName, int lineNumber);

void operator delete(void *ptr) noexcept;
void operator delete(void *ptr, size_t size) noexcept;
void operator delete[](void *ptr) noexcept;
void operator delete[](void *ptr, size_t size) noexcept;
#endif
