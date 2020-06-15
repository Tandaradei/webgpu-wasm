/*
 * Copyright 2016 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((noinline)) int no_stack_usage(void) {
  return 6;
}

__attribute__((noinline)) int alloca_gets_restored(int count) {
  char *buf = (char*)alloca(count);
  sprintf(buf, "%d", 6444);
  return strlen(buf);
}

__attribute__((noinline)) int stack_usage(void) {
  char buf[1024];
  sprintf(buf, "%d", 60);
  return strlen(buf);
}

int main(int argc, char **argv) {
  printf("%d\n", no_stack_usage());
  printf("%d\n", alloca_gets_restored(200));
  printf("%d\n", stack_usage());
  return 0;
}
