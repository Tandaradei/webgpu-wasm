// Copyright 2019 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <vector>

pthread_t threads[5];

static void *thread_start(void *arg)
{
  // This should be long enough for threads to pile up.
  int idx = (int)arg;
  printf("Starting thread %d\n", idx);
  while (true) {
    sleep(1);
  }
  printf("Finishing thread %d\n", idx);
  pthread_exit((void*)0);
}

void CreateThread(int idx) {
  EM_ASM(out('Main: Spawning thread '+$0+'...'), idx);
  int rc = pthread_create(&threads[idx], NULL, thread_start, (void*)idx);
  assert(rc == 0);
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

  // This test should be run with a prewarmed pool of size 4. None
  // of the threads are allocated yet.
  assert(EM_ASM_INT(return PThread.unusedWorkers.length) == 4);
  assert(EM_ASM_INT(return PThread.runningWorkers.length) == 0);

  CreateThread(0);

  // We have one running thread, allocated on demand.
  assert(EM_ASM_INT(return PThread.unusedWorkers.length) == 3);
  assert(EM_ASM_INT(return PThread.runningWorkers.length) == 1);

  for (int i = 1; i < 5; ++i) {
    CreateThread(i);
  }

  // All the preallocated workers should be used.
  // We can't join the threads or we'll hang forever. The main thread
  // won't give up the thread to let the 5th thread be created. This is
  // solved in non-test cases by using PROXY_TO_PTHREAD, but we can't
  // do that here since we need to eval the length of the various pthread
  // arrays.
  assert(EM_ASM_INT(return PThread.runningWorkers.length) == 5);
  assert(EM_ASM_INT(return PThread.unusedWorkers.length) == 0);

#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
}
