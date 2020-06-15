/*
 * Copyright 2016 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

#include <stdio.h>

int mix(int x, int y) {
  int ret;
  asm("Math.pow(2, %0+%1+1)" : "=r"(ret) : "r"(x), "r"(y));  // read and write
  return ret;
}

void mult() {
  asm("var $_$1 = Math.abs(-100); $_$1 *= 2; out($_$1)");  // multiline
  asm __volatile__("out('done')");
}

int main(int argc, char **argv) {
  printf("%d\n", mix(argc, argc / 2));
  mult();
  return 0;
}
