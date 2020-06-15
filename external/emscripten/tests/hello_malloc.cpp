// Copyright 2011 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
  // Check that a real malloc is used by allocating, freeing, then seeing that we did actually free by a new allocation going to the original place
  int N = 100;
  void *allocs[N];
  for (int i = 0; i < N; i++)
    allocs[i] = malloc((i+1)*1024);
  for (int i = 0; i < N; i++)
    free(allocs[i]);
  void *another = malloc(1024);
  assert(another == allocs[0]);
  printf("hello, world!\n");
  return 0;
}

