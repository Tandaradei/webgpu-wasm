/*
 * Copyright 2020 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#pragma once

#include <emscripten/emscripten.h>

#define ASMJS_PAGE_SIZE 16777216
#define WASM_PAGE_SIZE 65536

#ifdef __wasm__
#define EMSCRIPTEN_PAGE_SIZE WASM_PAGE_SIZE
#else
#define EMSCRIPTEN_PAGE_SIZE ASMJS_PAGE_SIZE
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Returns a pointer to a memory location that contains the heap DYNAMICTOP
// variable (the end of the dynamic memory region)
intptr_t *emscripten_get_sbrk_ptr(void) EM_IMPORT(emscripten_get_sbrk_ptr);

// Attempts to geometrically or linearly increase the heap so that it
// grows by at least requested_growth_bytes new bytes. The heap size may
// be overallocated, see src/settings.js variables MEMORY_GROWTH_GEOMETRIC_STEP,
// MEMORY_GROWTH_GEOMETRIC_CAP and MEMORY_GROWTH_LINEAR_STEP. This function
// cannot be used to shrink the size of the heap.
int emscripten_resize_heap(size_t requested_size) EM_IMPORT(emscripten_resize_heap);

// Returns the current size of the WebAssembly heap.
size_t emscripten_get_heap_size(void);

#ifdef __cplusplus
}
#endif
