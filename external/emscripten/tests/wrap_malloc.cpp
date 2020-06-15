// Copyright 2016 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int totalAllocated = 0;
int totalFreed = 0;

extern "C"
{

extern void* emscripten_builtin_malloc(size_t bytes);
extern void emscripten_builtin_free(void* mem);

void * __attribute__((noinline)) malloc(size_t size)
{
	++totalAllocated;
	void *ptr = emscripten_builtin_malloc(size);
	printf("Allocated %u bytes, got %p. %d pointers allocated total.\n", size, ptr, totalAllocated);
	return ptr;
}

void __attribute__((noinline)) free(void *ptr)
{
	++totalFreed;
	emscripten_builtin_free(ptr);
	printf("Freed ptr %p, %d pointers freed total.\n", ptr, totalFreed);
}

}

// Mark as used to defeat LTO which can otherwise completely elimate the
// calls to malloc below.
void *out __attribute__((used));

int main()
{
	for(int i = 0; i < 20; ++i)
	{
		void *ptr = malloc(1024 * 1024);
		out = ptr;
		free(ptr);
	}
	printf("totalAllocated: %d\n", totalAllocated);
	assert(totalAllocated == 20);
	printf("OK.\n");
	return 0;
}
