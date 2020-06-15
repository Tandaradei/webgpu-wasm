// Copyright 2014 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <emscripten.h>

double prevTime = -1.0;
int frame = 0;

void final(void*) {
  assert(frame == 100);
#ifdef REPORT_RESULT
  printf("Test passed.\n");
  REPORT_RESULT(0);
#endif
}

void looper() {
  frame++;
  double curTime = emscripten_get_now();
  double timeSincePrevious = (prevTime >= 0) ? (curTime - prevTime) : (1000.0/60.0);
  printf("frame: %d. dt: %g\n", frame, timeSincePrevious);
  if (timeSincePrevious <= 1.0) {
    static int timesTooSoon = 0;
    timesTooSoon++;
    if (timesTooSoon >= 10) {
      printf("Abort: main loop tick was called too quickly after the previous frame, too many times!\n");
#ifdef REPORT_RESULT
      REPORT_RESULT(1);
#endif
      emscripten_cancel_main_loop();
      exit(0);
    }
  }
  prevTime = curTime;
  if ((frame == 25 || frame == 45 || frame == 65) && timeSincePrevious < 30) {
    static int timesTooSoon = 0;
    timesTooSoon++;
    if (timesTooSoon >= 2) {
      printf("Abort: With swap interval of 4, we should be running at most 15fps! (or 30fps on 120Hz displays) but seems like swap control is not working and we are running at 60fps!\n");
#ifdef REPORT_RESULT
      REPORT_RESULT(2);
#endif
      emscripten_cancel_main_loop();
      exit(0);
    }
  }
  if (frame > 0 && frame < 90 && frame % 10 == 0) {
    emscripten_cancel_main_loop();
    emscripten_set_main_loop(looper, 0, 0);
    int ret;
    int mode,value;
    if (frame % 20 == 0) {
      ret = emscripten_set_main_loop_timing(EM_TIMING_RAF, 4);
      emscripten_get_main_loop_timing(&mode, &value);
      assert(mode == EM_TIMING_RAF && value == 4);
    } else {
      ret = emscripten_set_main_loop_timing(EM_TIMING_RAF, 0);
      emscripten_get_main_loop_timing(&mode, &value);
      assert(mode == EM_TIMING_RAF && value == 0);
    }
    assert(ret == 0);
  } else if (frame == 90) {
    emscripten_cancel_main_loop();
    emscripten_set_main_loop(looper, 100, 1);
  } else if (frame == 100) {
    emscripten_cancel_main_loop();
    emscripten_async_call(final, (void*)0, 1000);
  }
}

int main() {
  emscripten_set_main_loop(looper, 5, 1);
}
