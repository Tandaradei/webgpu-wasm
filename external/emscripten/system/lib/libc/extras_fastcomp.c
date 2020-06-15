/*
 * Copyright 2018 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 */

// With upstream we use musl's normal getenv code, but with fastcomp
// we use this constructor and stubs + getenv etc. in JS, because libc
// is a .bc file and we don't want to have a global constructor there
// for __environ, which would mean it is always included.
#ifdef __asmjs__

char** environ;

char*** _get_environ() {
  return &environ;
}

// Call JS to build the default environment.

extern void __buildEnvironment(void*);

// TODO: this needs very high priority, so user ctors that use environ do not happen first
__attribute__((constructor))
void __emscripten_environ_constructor(void) {
  __buildEnvironment((void*)&environ);
}

#endif
