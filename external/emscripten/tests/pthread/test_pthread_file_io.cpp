// Copyright 2015 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <pthread.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *thread1_start(void *arg)
{
  EM_ASM(out('thread1_start!'));

  FILE *handle = fopen("file1.txt", "r");
  assert(handle);
  char str[256] = {};
  fgets(str, 255, handle);
  fclose(handle);
  assert(!strcmp(str, "hello!"));

  handle = fopen("file2.txt", "w");
  fputs("hello2!", handle);
  fclose(handle);

  pthread_exit(0);
}

int main()
{
  if (!emscripten_has_threading_support())
  {
#ifdef REPORT_RESULT
    REPORT_RESULT(0);
#endif
    printf("Skipped: Threading is not supported.\n");
    return 0;
  }

  FILE *handle = fopen("file1.txt", "w");
  fputs("hello!", handle);
  fclose(handle);
  pthread_t thr;
  pthread_create(&thr, NULL, thread1_start, 0);
  pthread_join(thr, 0);

  handle = fopen("file2.txt", "r");
  char str[256] = {};
  fgets(str, 255, handle);
  fclose(handle);
  assert(!strcmp(str, "hello2!"));

#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
}
