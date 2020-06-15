/**
 * @license
 * Copyright 2010 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

//"use strict";

// An implementation of basic necessary libraries for the web. This integrates
// with a compiled libc and with the rest of the JS runtime.
//
// We search the Library object when there is an external function. If the
// entry in the Library is a function, we insert it. If it is a string, we
// do another lookup in the library (a simple way to write a function once,
// if it can be called by different names). We also allow dependencies,
// using __deps. Initialization code to be run after allocating all
// global constants can be defined by __postset.
//
// Note that the full function name will be '_' + the name in the Library
// object. For convenience, the short name appears here. Note that if you add a
// new function with an '_', it will not be found.

// Memory allocated during startup, in postsets, should only be static
// (using makeStaticAlloc)

LibraryManager.library = {
#if !WASM_BACKEND
  __dso_handle: '{{{ makeStaticAlloc(1) }}}',
#endif

  // ==========================================================================
  // getTempRet0/setTempRet0: scratch space handling i64 return
  // ==========================================================================

  getTempRet0__sig: 'i',
  getTempRet0: function() {
    return {{{ makeGetTempRet0() }}};
  },

  setTempRet0__sig: 'vi',
  setTempRet0: function($i) {
    {{{ makeSetTempRet0('$i') }}};
  },

  // ==========================================================================
  // JavaScript <-> C string interop
  // ==========================================================================

  $stringToNewUTF8: function(jsString) {
    var length = lengthBytesUTF8(jsString)+1;
    var cString = _malloc(length);
    stringToUTF8(jsString, cString, length);
    return cString;
  },

  // ==========================================================================
  // utime.h
  // ==========================================================================

  utime__deps: ['$FS', '$setErrNo'],
  utime__proxy: 'sync',
  utime__sig: 'iii',
  utime: function(path, times) {
    // int utime(const char *path, const struct utimbuf *times);
    // http://pubs.opengroup.org/onlinepubs/009695399/basedefs/utime.h.html
    var time;
    if (times) {
      // NOTE: We don't keep track of access timestamps.
      var offset = {{{ C_STRUCTS.utimbuf.modtime }}};
      time = {{{ makeGetValue('times', 'offset', 'i32') }}};
      time *= 1000;
    } else {
      time = Date.now();
    }
    path = UTF8ToString(path);
    try {
      FS.utime(path, time, time);
      return 0;
    } catch (e) {
      FS.handleFSError(e);
      return -1;
    }
  },

  utimes__deps: ['$FS', '$setErrNo'],
  utimes__proxy: 'sync',
  utimes__sig: 'iii',
  utimes: function(path, times) {
    var time;
    if (times) {
      var offset = {{{ C_STRUCTS.timeval.__size__ }}} + {{{ C_STRUCTS.timeval.tv_sec }}};
      time = {{{ makeGetValue('times', 'offset', 'i32') }}} * 1000;
      offset = {{{ C_STRUCTS.timeval.__size__ }}} + {{{ C_STRUCTS.timeval.tv_usec }}};
      time += {{{ makeGetValue('times', 'offset', 'i32') }}} / 1000;
    } else {
      time = Date.now();
    }
    path = UTF8ToString(path);
    try {
      FS.utime(path, time, time);
      return 0;
    } catch (e) {
      FS.handleFSError(e);
      return -1;
    }
  },

  // ==========================================================================
  // sys/file.h
  // ==========================================================================

  flock: function(fd, operation) {
    // int flock(int fd, int operation);
    // Pretend to succeed
    return 0;
  },

  chroot__deps: ['$setErrNo'],
  chroot__proxy: 'sync',
  chroot__sig: 'ii',
  chroot: function(path) {
    // int chroot(const char *path);
    // http://pubs.opengroup.org/onlinepubs/7908799/xsh/chroot.html
    setErrNo({{{ cDefine('EACCES') }}});
    return -1;
  },

  fpathconf__deps: ['$setErrNo'],
  fpathconf__proxy: 'sync',
  fpathconf__sig: 'iii',
  fpathconf: function(fildes, name) {
    // long fpathconf(int fildes, int name);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/encrypt.html
    // NOTE: The first parameter is ignored, so pathconf == fpathconf.
    // The constants here aren't real values. Just mimicking glibc.
    switch (name) {
      case {{{ cDefine('_PC_LINK_MAX') }}}:
        return 32000;
      case {{{ cDefine('_PC_MAX_CANON') }}}:
      case {{{ cDefine('_PC_MAX_INPUT') }}}:
      case {{{ cDefine('_PC_NAME_MAX') }}}:
        return 255;
      case {{{ cDefine('_PC_PATH_MAX') }}}:
      case {{{ cDefine('_PC_PIPE_BUF') }}}:
      case {{{ cDefine('_PC_REC_MIN_XFER_SIZE') }}}:
      case {{{ cDefine('_PC_REC_XFER_ALIGN') }}}:
      case {{{ cDefine('_PC_ALLOC_SIZE_MIN') }}}:
        return 4096;
      case {{{ cDefine('_PC_CHOWN_RESTRICTED') }}}:
      case {{{ cDefine('_PC_NO_TRUNC') }}}:
      case {{{ cDefine('_PC_2_SYMLINKS') }}}:
        return 1;
      case {{{ cDefine('_PC_VDISABLE') }}}:
        return 0;
      case {{{ cDefine('_PC_SYNC_IO') }}}:
      case {{{ cDefine('_PC_ASYNC_IO') }}}:
      case {{{ cDefine('_PC_PRIO_IO') }}}:
      case {{{ cDefine('_PC_SOCK_MAXBUF') }}}:
      case {{{ cDefine('_PC_REC_INCR_XFER_SIZE') }}}:
      case {{{ cDefine('_PC_REC_MAX_XFER_SIZE') }}}:
      case {{{ cDefine('_PC_SYMLINK_MAX') }}}:
        return -1;
      case {{{ cDefine('_PC_FILESIZEBITS') }}}:
        return 64;
    }
    setErrNo({{{ cDefine('EINVAL') }}});
    return -1;
  },
  pathconf: 'fpathconf',

  confstr__deps: ['$setErrNo', '$ENV'],
  confstr__proxy: 'sync',
  confstr__sig: 'iiii',
  confstr: function(name, buf, len) {
    // size_t confstr(int name, char *buf, size_t len);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/confstr.html
    var value;
    switch (name) {
      case {{{ cDefine('_CS_PATH') }}}:
        value = ENV['PATH'] || '/';
        break;
      case {{{ cDefine('_CS_POSIX_V6_WIDTH_RESTRICTED_ENVS') }}}:
        // Mimicking glibc.
        value = 'POSIX_V6_ILP32_OFF32\nPOSIX_V6_ILP32_OFFBIG';
        break;
      case {{{ cDefine('_CS_GNU_LIBC_VERSION') }}}:
        // This JS implementation was tested against this glibc version.
        value = 'glibc 2.14';
        break;
      case {{{ cDefine('_CS_GNU_LIBPTHREAD_VERSION') }}}:
        // We don't support pthreads.
        value = '';
        break;
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFF32_LIBS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFFBIG_LIBS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LP64_OFF64_CFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LP64_OFF64_LDFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LP64_OFF64_LIBS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LPBIG_OFFBIG_CFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LPBIG_OFFBIG_LDFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_LPBIG_OFFBIG_LIBS') }}}:
        value = '';
        break;
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFF32_CFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFF32_LDFLAGS') }}}:
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFFBIG_LDFLAGS') }}}:
        value = '-m32';
        break;
      case {{{ cDefine('_CS_POSIX_V6_ILP32_OFFBIG_CFLAGS') }}}:
        value = '-m32 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64';
        break;
      default:
        setErrNo({{{ cDefine('EINVAL') }}});
        return 0;
    }
    if (len == 0 || buf == 0) {
      return value.length + 1;
    } else {
      var length = Math.min(len, value.length);
      for (var i = 0; i < length; i++) {
        {{{ makeSetValue('buf', 'i', 'value.charCodeAt(i)', 'i8') }}};
      }
      if (len > length) {{{ makeSetValue('buf', 'i++', '0', 'i8') }}};
      return i;
    }
  },

  execl__deps: ['$setErrNo'],
  execl__sig: 'iiii',
  execl: function(path, arg0, varArgs) {
    // int execl(const char *path, const char *arg0, ... /*, (char *)0 */);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
    // We don't support executing external code.
    setErrNo({{{ cDefine('ENOEXEC') }}});
    return -1;
  },
  execle: 'execl',
  execlp: 'execl',
  execv: 'execl',
  execve: 'execl',
  execvp: 'execl',
  __execvpe: 'execl',
  fexecve: 'execl',

  exit__sig: 'vi',
  exit: function(status) {
#if MINIMAL_RUNTIME
    throw 'exit(' + status + ')';
#else
    // void _exit(int status);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/exit.html
    exit(status);
#endif
  },

  _exit__sig: 'vi',
  _exit: 'exit',

  _Exit__sig: 'vi',
  _Exit: 'exit',

#if MINIMAL_RUNTIME
  $exit: function(status) {
    throw 'exit(' + status + ')';
  },
#endif

  fork__deps: ['$setErrNo'],
  fork__sig: 'i',
  fork: function() {
    // pid_t fork(void);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/fork.html
    // We don't support multiple processes.
    setErrNo({{{ cDefine('EAGAIN') }}});
    return -1;
  },
  vfork: 'fork',
  posix_spawn: 'fork',
  posix_spawnp: 'fork',

  setgroups__deps: ['$setErrNo', 'sysconf'],
  setgroups: function(ngroups, gidset) {
    // int setgroups(int ngroups, const gid_t *gidset);
    // https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man2/setgroups.2.html
    if (ngroups < 1 || ngroups > _sysconf({{{ cDefine('_SC_NGROUPS_MAX') }}})) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    } else {
      // We have just one process/user/group, so it makes no sense to set groups.
      setErrNo({{{ cDefine('EPERM') }}});
      return -1;
    }
  },

  sysconf__deps: ['$setErrNo'],
  sysconf__proxy: 'sync',
  sysconf__sig: 'ii',
  sysconf: function(name) {
    // long sysconf(int name);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/sysconf.html
    switch(name) {
      case {{{ cDefine('_SC_PAGE_SIZE') }}}: return {{{ POSIX_PAGE_SIZE }}};
      case {{{ cDefine('_SC_PHYS_PAGES') }}}:
#if ALLOW_MEMORY_GROWTH
#if MAXIMUM_MEMORY == -1 // no maximum set, assume the best
        var maxHeapSize = 4*1024*1024*1024;
#else
        var maxHeapSize = {{{ MAXIMUM_MEMORY }}};
#endif
#else // no growth
        var maxHeapSize = HEAPU8.length;
#endif
        return maxHeapSize / {{{ POSIX_PAGE_SIZE }}};
      case {{{ cDefine('_SC_ADVISORY_INFO') }}}:
      case {{{ cDefine('_SC_BARRIERS') }}}:
      case {{{ cDefine('_SC_ASYNCHRONOUS_IO') }}}:
      case {{{ cDefine('_SC_CLOCK_SELECTION') }}}:
      case {{{ cDefine('_SC_CPUTIME') }}}:
      case {{{ cDefine('_SC_FSYNC') }}}:
      case {{{ cDefine('_SC_IPV6') }}}:
      case {{{ cDefine('_SC_MAPPED_FILES') }}}:
      case {{{ cDefine('_SC_MEMLOCK') }}}:
      case {{{ cDefine('_SC_MEMLOCK_RANGE') }}}:
      case {{{ cDefine('_SC_MEMORY_PROTECTION') }}}:
      case {{{ cDefine('_SC_MESSAGE_PASSING') }}}:
      case {{{ cDefine('_SC_MONOTONIC_CLOCK') }}}:
      case {{{ cDefine('_SC_PRIORITIZED_IO') }}}:
      case {{{ cDefine('_SC_PRIORITY_SCHEDULING') }}}:
      case {{{ cDefine('_SC_RAW_SOCKETS') }}}:
      case {{{ cDefine('_SC_READER_WRITER_LOCKS') }}}:
      case {{{ cDefine('_SC_REALTIME_SIGNALS') }}}:
      case {{{ cDefine('_SC_SEMAPHORES') }}}:
      case {{{ cDefine('_SC_SHARED_MEMORY_OBJECTS') }}}:
      case {{{ cDefine('_SC_SPAWN') }}}:
      case {{{ cDefine('_SC_SPIN_LOCKS') }}}:
      case {{{ cDefine('_SC_SYNCHRONIZED_IO') }}}:
      case {{{ cDefine('_SC_THREAD_ATTR_STACKADDR') }}}:
      case {{{ cDefine('_SC_THREAD_ATTR_STACKSIZE') }}}:
      case {{{ cDefine('_SC_THREAD_CPUTIME') }}}:
      case {{{ cDefine('_SC_THREAD_PRIO_INHERIT') }}}:
      case {{{ cDefine('_SC_THREAD_PRIO_PROTECT') }}}:
      case {{{ cDefine('_SC_THREAD_PROCESS_SHARED') }}}:
      case {{{ cDefine('_SC_THREAD_SAFE_FUNCTIONS') }}}:
      case {{{ cDefine('_SC_THREADS') }}}:
      case {{{ cDefine('_SC_TIMEOUTS') }}}:
      case {{{ cDefine('_SC_TIMERS') }}}:
      case {{{ cDefine('_SC_VERSION') }}}:
      case {{{ cDefine('_SC_2_C_BIND') }}}:
      case {{{ cDefine('_SC_2_C_DEV') }}}:
      case {{{ cDefine('_SC_2_CHAR_TERM') }}}:
      case {{{ cDefine('_SC_2_LOCALEDEF') }}}:
      case {{{ cDefine('_SC_2_SW_DEV') }}}:
      case {{{ cDefine('_SC_2_VERSION') }}}:
      case {{{ cDefine('_SC_THREAD_PRIORITY_SCHEDULING') }}}:
        return 200809;
      case {{{ cDefine('_SC_MQ_OPEN_MAX') }}}:
      case {{{ cDefine('_SC_XOPEN_STREAMS') }}}:
      case {{{ cDefine('_SC_XBS5_LP64_OFF64') }}}:
      case {{{ cDefine('_SC_XBS5_LPBIG_OFFBIG') }}}:
      case {{{ cDefine('_SC_AIO_LISTIO_MAX') }}}:
      case {{{ cDefine('_SC_AIO_MAX') }}}:
      case {{{ cDefine('_SC_SPORADIC_SERVER') }}}:
      case {{{ cDefine('_SC_THREAD_SPORADIC_SERVER') }}}:
      case {{{ cDefine('_SC_TRACE') }}}:
      case {{{ cDefine('_SC_TRACE_EVENT_FILTER') }}}:
      case {{{ cDefine('_SC_TRACE_EVENT_NAME_MAX') }}}:
      case {{{ cDefine('_SC_TRACE_INHERIT') }}}:
      case {{{ cDefine('_SC_TRACE_LOG') }}}:
      case {{{ cDefine('_SC_TRACE_NAME_MAX') }}}:
      case {{{ cDefine('_SC_TRACE_SYS_MAX') }}}:
      case {{{ cDefine('_SC_TRACE_USER_EVENT_MAX') }}}:
      case {{{ cDefine('_SC_TYPED_MEMORY_OBJECTS') }}}:
      case {{{ cDefine('_SC_V6_LP64_OFF64') }}}:
      case {{{ cDefine('_SC_V6_LPBIG_OFFBIG') }}}:
      case {{{ cDefine('_SC_2_FORT_DEV') }}}:
      case {{{ cDefine('_SC_2_FORT_RUN') }}}:
      case {{{ cDefine('_SC_2_PBS') }}}:
      case {{{ cDefine('_SC_2_PBS_ACCOUNTING') }}}:
      case {{{ cDefine('_SC_2_PBS_CHECKPOINT') }}}:
      case {{{ cDefine('_SC_2_PBS_LOCATE') }}}:
      case {{{ cDefine('_SC_2_PBS_MESSAGE') }}}:
      case {{{ cDefine('_SC_2_PBS_TRACK') }}}:
      case {{{ cDefine('_SC_2_UPE') }}}:
      case {{{ cDefine('_SC_THREAD_THREADS_MAX') }}}:
      case {{{ cDefine('_SC_SEM_NSEMS_MAX') }}}:
      case {{{ cDefine('_SC_SYMLOOP_MAX') }}}:
      case {{{ cDefine('_SC_TIMER_MAX') }}}:
        return -1;
      case {{{ cDefine('_SC_V6_ILP32_OFF32') }}}:
      case {{{ cDefine('_SC_V6_ILP32_OFFBIG') }}}:
      case {{{ cDefine('_SC_JOB_CONTROL') }}}:
      case {{{ cDefine('_SC_REGEXP') }}}:
      case {{{ cDefine('_SC_SAVED_IDS') }}}:
      case {{{ cDefine('_SC_SHELL') }}}:
      case {{{ cDefine('_SC_XBS5_ILP32_OFF32') }}}:
      case {{{ cDefine('_SC_XBS5_ILP32_OFFBIG') }}}:
      case {{{ cDefine('_SC_XOPEN_CRYPT') }}}:
      case {{{ cDefine('_SC_XOPEN_ENH_I18N') }}}:
      case {{{ cDefine('_SC_XOPEN_LEGACY') }}}:
      case {{{ cDefine('_SC_XOPEN_REALTIME') }}}:
      case {{{ cDefine('_SC_XOPEN_REALTIME_THREADS') }}}:
      case {{{ cDefine('_SC_XOPEN_SHM') }}}:
      case {{{ cDefine('_SC_XOPEN_UNIX') }}}:
        return 1;
      case {{{ cDefine('_SC_THREAD_KEYS_MAX') }}}:
      case {{{ cDefine('_SC_IOV_MAX') }}}:
      case {{{ cDefine('_SC_GETGR_R_SIZE_MAX') }}}:
      case {{{ cDefine('_SC_GETPW_R_SIZE_MAX') }}}:
      case {{{ cDefine('_SC_OPEN_MAX') }}}:
        return 1024;
      case {{{ cDefine('_SC_RTSIG_MAX') }}}:
      case {{{ cDefine('_SC_EXPR_NEST_MAX') }}}:
      case {{{ cDefine('_SC_TTY_NAME_MAX') }}}:
        return 32;
      case {{{ cDefine('_SC_ATEXIT_MAX') }}}:
      case {{{ cDefine('_SC_DELAYTIMER_MAX') }}}:
      case {{{ cDefine('_SC_SEM_VALUE_MAX') }}}:
        return 2147483647;
      case {{{ cDefine('_SC_SIGQUEUE_MAX') }}}:
      case {{{ cDefine('_SC_CHILD_MAX') }}}:
        return 47839;
      case {{{ cDefine('_SC_BC_SCALE_MAX') }}}:
      case {{{ cDefine('_SC_BC_BASE_MAX') }}}:
        return 99;
      case {{{ cDefine('_SC_LINE_MAX') }}}:
      case {{{ cDefine('_SC_BC_DIM_MAX') }}}:
        return 2048;
      case {{{ cDefine('_SC_ARG_MAX') }}}: return 2097152;
      case {{{ cDefine('_SC_NGROUPS_MAX') }}}: return 65536;
      case {{{ cDefine('_SC_MQ_PRIO_MAX') }}}: return 32768;
      case {{{ cDefine('_SC_RE_DUP_MAX') }}}: return 32767;
      case {{{ cDefine('_SC_THREAD_STACK_MIN') }}}: return 16384;
      case {{{ cDefine('_SC_BC_STRING_MAX') }}}: return 1000;
      case {{{ cDefine('_SC_XOPEN_VERSION') }}}: return 700;
      case {{{ cDefine('_SC_LOGIN_NAME_MAX') }}}: return 256;
      case {{{ cDefine('_SC_COLL_WEIGHTS_MAX') }}}: return 255;
      case {{{ cDefine('_SC_CLK_TCK') }}}: return 100;
      case {{{ cDefine('_SC_HOST_NAME_MAX') }}}: return 64;
      case {{{ cDefine('_SC_AIO_PRIO_DELTA_MAX') }}}: return 20;
      case {{{ cDefine('_SC_STREAM_MAX') }}}: return 16;
      case {{{ cDefine('_SC_TZNAME_MAX') }}}: return 6;
      case {{{ cDefine('_SC_THREAD_DESTRUCTOR_ITERATIONS') }}}: return 4;
      case {{{ cDefine('_SC_NPROCESSORS_ONLN') }}}: {
        if (typeof navigator === 'object') return navigator['hardwareConcurrency'] || 1;
        return 1;
      }
    }
    setErrNo({{{ cDefine('EINVAL') }}});
    return -1;
  },

  emscripten_get_heap_size: function() {
    return HEAPU8.length;
  },

  emscripten_get_sbrk_ptr__asm: true,
  emscripten_get_sbrk_ptr__sig: 'i',
  emscripten_get_sbrk_ptr: function() {
    return {{{ DYNAMICTOP_PTR }}};
  },

#if ABORTING_MALLOC
  $abortOnCannotGrowMemory: function(requestedSize) {
#if ASSERTIONS
#if ALLOW_MEMORY_GROWTH
    abort('Cannot enlarge memory arrays to size ' + requestedSize + ' bytes (OOM). If you want malloc to return NULL (0) instead of this abort, do not link with -s ABORTING_MALLOC=1 (that is, the default when growth is enabled is to not abort, but you have overridden that)');
#else // ALLOW_MEMORY_GROWTH
    abort('Cannot enlarge memory arrays to size ' + requestedSize + ' bytes (OOM). Either (1) compile with  -s INITIAL_MEMORY=X  with X higher than the current value ' + HEAP8.length + ', (2) compile with  -s ALLOW_MEMORY_GROWTH=1  which allows increasing the size at runtime, or (3) if you want malloc to return NULL (0) instead of this abort, compile with  -s ABORTING_MALLOC=0 ');
#endif // ALLOW_MEMORY_GROWTH
#else // ASSERTIONS
    abort('OOM');
#endif // ASSERTIONS
  },
#endif // ABORTING_MALLOC

#if TEST_MEMORY_GROWTH_FAILS
  $emscripten_realloc_buffer: function(size) {
    return false;
  },
#else

  // Grows the asm.js/wasm heap to the given byte size, and updates both JS and asm.js/wasm side views to the buffer.
  // Returns 1 on success, or undefined if growing failed.
  $emscripten_realloc_buffer: function(size) {
#if MEMORYPROFILER
    var oldHeapSize = buffer.byteLength;
#endif
    try {
#if WASM
      // round size grow request up to wasm page size (fixed 64KB per spec)
      wasmMemory.grow((size - buffer.byteLength + 65535) >>> 16); // .grow() takes a delta compared to the previous size
      updateGlobalBufferAndViews(wasmMemory.buffer);
#else // asm.js:
      var newBuffer = new ArrayBuffer(size);
      if (newBuffer.byteLength != size) return /*undefined, allocation did not succeed*/;
      new Int8Array(newBuffer).set(/**@type{!Int8Array}*/(HEAP8));
      _emscripten_replace_memory(newBuffer);
      updateGlobalBufferAndViews(newBuffer);
#endif
#if MEMORYPROFILER
      if (typeof emscriptenMemoryProfiler !== 'undefined') {
        emscriptenMemoryProfiler.onMemoryResize(oldHeapSize, buffer.byteLength);
      }
#endif
      return 1 /*success*/;
    } catch(e) {
#if ASSERTIONS
      console.error('emscripten_realloc_buffer: Attempted to grow heap from ' + buffer.byteLength  + ' bytes to ' + size + ' bytes, but got error: ' + e);
#endif
    }
  },
#endif // ~TEST_MEMORY_GROWTH_FAILS

  emscripten_resize_heap__deps: ['emscripten_get_heap_size'
#if ASSERTIONS == 2
  , 'emscripten_get_now'
#endif
#if ABORTING_MALLOC
  , '$abortOnCannotGrowMemory'
#endif
#if ALLOW_MEMORY_GROWTH
  , '$emscripten_realloc_buffer'
#endif
  ],
  emscripten_resize_heap: function(requestedSize) {
    requestedSize = requestedSize >>> 0;
#if ALLOW_MEMORY_GROWTH == 0
#if ABORTING_MALLOC
    abortOnCannotGrowMemory(requestedSize);
#else
    return false; // malloc will report failure
#endif // ABORTING_MALLOC
#else // ALLOW_MEMORY_GROWTH == 0
    var oldSize = _emscripten_get_heap_size();
    // With pthreads, races can happen (another thread might increase the size in between), so return a failure, and let the caller retry.
#if USE_PTHREADS
    if (requestedSize <= oldSize) {
      return false;
    }
#endif // USE_PTHREADS
#if ASSERTIONS && !USE_PTHREADS
    assert(requestedSize > oldSize);
#endif

#if EMSCRIPTEN_TRACING
    // Report old layout one last time
    _emscripten_trace_report_memory_layout();
#endif

    var PAGE_MULTIPLE = {{{ getMemoryPageSize() }}};

    // Memory resize rules:
    // 1. When resizing, always produce a resized heap that is at least 16MB (to avoid tiny heap sizes receiving lots of repeated resizes at startup)
    // 2. Always increase heap size to at least the requested size, rounded up to next page multiple.
    // 3a. If MEMORY_GROWTH_LINEAR_STEP == -1, excessively resize the heap geometrically: increase the heap size according to 
    //                                         MEMORY_GROWTH_GEOMETRIC_STEP factor (default +20%),
    //                                         At most overreserve by MEMORY_GROWTH_GEOMETRIC_CAP bytes (default 96MB).
    // 3b. If MEMORY_GROWTH_LINEAR_STEP != -1, excessively resize the heap linearly: increase the heap size by at least MEMORY_GROWTH_LINEAR_STEP bytes.
    // 4. Max size for the heap is capped at 2048MB-PAGE_MULTIPLE, or by MAXIMUM_MEMORY, or by ASAN limit, depending on which is smallest
    // 5. If we were unable to allocate as much memory, it may be due to over-eager decision to excessively reserve due to (3) above.
    //    Hence if an allocation fails, cut down on the amount of excess growth, in an attempt to succeed to perform a smaller allocation.

#if MAXIMUM_MEMORY != -1
    // A limit was set for how much we can grow. We should not exceed that
    // (the wasm binary specifies it, so if we tried, we'd fail anyhow).
    var maxHeapSize = {{{ MAXIMUM_MEMORY }}};
#else
    var maxHeapSize = {{{ CAN_ADDRESS_2GB ? 4294967296 : 2147483648 }}} - PAGE_MULTIPLE;
#endif
    if (requestedSize > maxHeapSize) {
#if ASSERTIONS
      err('Cannot enlarge memory, asked to go up to ' + requestedSize + ' bytes, but the limit is ' + maxHeapSize + ' bytes!');
#endif
#if ABORTING_MALLOC
      abortOnCannotGrowMemory(requestedSize);
#else
      return false;
#endif
    }
#if USE_ASAN
    // One byte of ASan's shadow memory shadows 8 bytes of real memory. Shadow memory area has a fixed size,
    // so do not allow resizing past that limit.
    maxHeapSize = Math.min(maxHeapSize, {{{ 8 * ASAN_SHADOW_SIZE }}});
    if (requestedSize > maxHeapSize) {
#if ASSERTIONS
      err('Failed to grow the heap from ' + oldSize + ', as we reached the limit of our shadow memory. Increase ASAN_SHADOW_SIZE.');
#endif
#if ABORTING_MALLOC
      abortOnCannotGrowMemory(requestedSize);
#else
      return false;
#endif
    }
#endif

    var minHeapSize = 16777216;

    // Loop through potential heap size increases. If we attempt a too eager reservation that fails, cut down on the
    // attempted size and reserve a smaller bump instead. (max 3 times, chosen somewhat arbitrarily)
    for(var cutDown = 1; cutDown <= 4; cutDown *= 2) {
#if MEMORY_GROWTH_LINEAR_STEP == -1
      var overGrownHeapSize = oldSize * (1 + {{{ MEMORY_GROWTH_GEOMETRIC_STEP }}} / cutDown); // ensure geometric growth
#if MEMORY_GROWTH_GEOMETRIC_CAP
      // but limit overreserving (default to capping at +96MB overgrowth at most)
      overGrownHeapSize = Math.min(overGrownHeapSize, requestedSize + {{{ MEMORY_GROWTH_GEOMETRIC_CAP }}} );
#endif

#else
      var overGrownHeapSize = oldSize + {{{ MEMORY_GROWTH_LINEAR_STEP }}} / cutDown; // ensure linear growth
#endif

      var newSize = Math.min(maxHeapSize, alignUp(Math.max(minHeapSize, requestedSize, overGrownHeapSize), PAGE_MULTIPLE));

#if ASSERTIONS == 2
      var t0 = _emscripten_get_now();
#endif
      var replacement = emscripten_realloc_buffer(newSize);
#if ASSERTIONS == 2
      var t1 = _emscripten_get_now();
      console.log('Heap resize call from ' + oldSize + ' to ' + newSize + ' took ' + (t1 - t0) + ' msecs. Success: ' + !!replacement);
#endif
      if (replacement) {
#if ASSERTIONS && (!WASM || WASM2JS)
        err('Warning: Enlarging memory arrays, this is not fast! ' + [oldSize, newSize]);
#endif

#if EMSCRIPTEN_TRACING
        _emscripten_trace_js_log_message("Emscripten", "Enlarging memory arrays from " + oldSize + " to " + newSize);
        // And now report the new layout
        _emscripten_trace_report_memory_layout();
#endif
        return true;
      }
    }
#if ASSERTIONS
    err('Failed to grow the heap from ' + oldSize + ' bytes to ' + newSize + ' bytes, not enough memory!');
#endif
#if ABORTING_MALLOC
    abortOnCannotGrowMemory(requestedSize);
#else
    return false;
#endif
#endif // ALLOW_MEMORY_GROWTH
  },

  // Called after wasm grows memory. At that time we need to update the views.
  // Without this notification, we'd need to check the buffer in JS every time
  // we return from any wasm, which adds overhead. See
  // https://github.com/WebAssembly/WASI/issues/82
  emscripten_notify_memory_growth: function(memoryIndex) {
#if ASSERTIONS
    assert(memoryIndex == 0);
#endif
    updateGlobalBufferAndViews(wasmMemory.buffer);
  },

  system__deps: ['$setErrNo'],
  system: function(command) {
#if ENVIRONMENT_MAY_BE_NODE
    if (ENVIRONMENT_IS_NODE) {
      if (!command) return 1; // shell is available

      var cmdstr = UTF8ToString(command);
      if (!cmdstr.length) return 0; // this is what glibc seems to do (shell works test?)

      var cp = require('child_process');
      var ret = cp.spawnSync(cmdstr, [], {shell:true, stdio:'inherit'});

      var _W_EXITCODE = function(ret, sig) {
        return ((ret) << 8 | (sig));
      }

      // this really only can happen if process is killed by signal
      if (ret.status === null) {
        // sadly node doesn't expose such function
        var signalToNumber = function(sig) {
          // implement only the most common ones, and fallback to SIGINT
          switch (sig) {
            case 'SIGHUP': return 1;
            case 'SIGINT': return 2;
            case 'SIGQUIT': return 3;
            case 'SIGFPE': return 8;
            case 'SIGKILL': return 9;
            case 'SIGALRM': return 14;
            case 'SIGTERM': return 15;
          }
          return 2; // SIGINT
        }
        return _W_EXITCODE(0, signalToNumber(ret.signal));
      }

      return _W_EXITCODE(ret.status, 0);
    }
#endif // ENVIRONMENT_MAY_BE_NODE
    // int system(const char *command);
    // http://pubs.opengroup.org/onlinepubs/000095399/functions/system.html
    // Can't call external programs.
    if (!command) return 0; // no shell available
    setErrNo({{{ cDefine('EAGAIN') }}});
    return -1;
  },

  // ==========================================================================
  // stdlib.h
  // ==========================================================================

  abs: 'Math_abs',
  labs: 'Math_abs',

  _ZSt9terminatev__deps: ['exit'],
  _ZSt9terminatev: function() {
    _exit(-1234);
  },

#if MINIMAL_RUNTIME && !EXIT_RUNTIME
  atexit: function(){},
  __cxa_atexit: function(){},
  __cxa_thread_atexit: function(){},
  __cxa_thread_atexit_impl: function(){},
#else
  atexit__proxy: 'sync',
  atexit__sig: 'iii',
  atexit: function(func, arg) {
#if ASSERTIONS
#if EXIT_RUNTIME == 0
    warnOnce('atexit() called, but EXIT_RUNTIME is not set, so atexits() will not be called. set EXIT_RUNTIME to 1 (see the FAQ)');
#endif
#endif
    __ATEXIT__.unshift({ func: func, arg: arg });
  },
  __cxa_atexit: 'atexit',

  // used in rust, clang when doing thread_local statics
  __cxa_thread_atexit: 'atexit',
  __cxa_thread_atexit_impl: 'atexit',
#endif

  // TODO: There are currently two abort() functions that get imported to asm module scope: the built-in runtime function abort(),
  // and this function _abort(). Remove one of these, importing two functions for the same purpose is wasteful.
  abort: function() {
#if MINIMAL_RUNTIME
    // In MINIMAL_RUNTIME the module object does not exist, so its behavior to abort is to throw directly.
    throw 'abort';
#else
    abort();
#endif
  },

  // This object can be modified by the user during startup, which affects
  // the initial values of the environment accessible by getenv. (This works
  // in both fastcomp and upstream.
  $ENV: {},

#if !WASM_BACKEND
  // This implementation of environ/getenv/etc. is used for fastcomp, due
  // to limitations in the system libraries (we can't easily add a global
  // ctor to create the environment without it always being linked in with
  // libc).
  __buildEnvironment__deps: ['$ENV', '_getExecutableName'
#if MINIMAL_RUNTIME
    , '$writeAsciiToMemory'
#endif
  ],
  __buildEnvironment: function(environ) {
    // WARNING: Arbitrary limit!
    var MAX_ENV_VALUES = 64;
    var TOTAL_ENV_SIZE = 1024;

    // Statically allocate memory for the environment.
    var poolPtr;
    var envPtr;
    if (!___buildEnvironment.called) {
      ___buildEnvironment.called = true;
      // Set default values. Use string keys for Closure Compiler compatibility.
      ENV['USER'] = 'web_user';
      ENV['LOGNAME'] = 'web_user';
      ENV['PATH'] = '/';
      ENV['PWD'] = '/';
      ENV['HOME'] = '/home/web_user';
      // Browser language detection #8751
      ENV['LANG'] = ((typeof navigator === 'object' && navigator.languages && navigator.languages[0]) || 'C').replace('-', '_') + '.UTF-8';
      ENV['_'] = __getExecutableName();
      // Allocate memory.
#if !MINIMAL_RUNTIME // TODO: environment support in MINIMAL_RUNTIME
      poolPtr = getMemory(TOTAL_ENV_SIZE);
      envPtr = getMemory(MAX_ENV_VALUES * {{{ Runtime.POINTER_SIZE }}});
      {{{ makeSetValue('envPtr', '0', 'poolPtr', 'i8*') }}};
      {{{ makeSetValue('environ', 0, 'envPtr', 'i8*') }}};
#endif
    } else {
      envPtr = {{{ makeGetValue('environ', '0', 'i8**') }}};
      poolPtr = {{{ makeGetValue('envPtr', '0', 'i8*') }}};
    }

    // Collect key=value lines.
    var strings = [];
    var totalSize = 0;
    for (var key in ENV) {
      if (typeof ENV[key] === 'string') {
        var line = key + '=' + ENV[key];
        strings.push(line);
        totalSize += line.length;
      }
    }
    if (totalSize > TOTAL_ENV_SIZE) {
      throw new Error('Environment size exceeded TOTAL_ENV_SIZE!');
    }

    // Make new.
    var ptrSize = {{{ Runtime.getNativeTypeSize('i8*') }}};
    for (var i = 0; i < strings.length; i++) {
      var line = strings[i];
      writeAsciiToMemory(line, poolPtr);
      {{{ makeSetValue('envPtr', 'i * ptrSize', 'poolPtr', 'i8*') }}};
      poolPtr += line.length + 1;
    }
    {{{ makeSetValue('envPtr', 'strings.length * ptrSize', '0', 'i8*') }}};
  },
  getenv__deps: ['$ENV',
#if MINIMAL_RUNTIME
    '$allocateUTF8',
#endif
  ],
  getenv__proxy: 'sync',
  getenv__sig: 'ii',
  getenv: function(name) {
    // char *getenv(const char *name);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/getenv.html
    if (name === 0) return 0;
    name = UTF8ToString(name);
    if (!ENV.hasOwnProperty(name)) return 0;

    if (_getenv.ret) _free(_getenv.ret);
    _getenv.ret = allocateUTF8(ENV[name]);
    return _getenv.ret;
  },
  clearenv__deps: ['$ENV', '__buildEnvironment'],
  clearenv__proxy: 'sync',
  clearenv__sig: 'i',
  clearenv: function() {
    // int clearenv (void);
    // http://www.gnu.org/s/hello/manual/libc/Environment-Access.html#index-clearenv-3107
    ENV = {};
    ___buildEnvironment(__get_environ());
    return 0;
  },
  setenv__deps: ['$ENV', '__buildEnvironment', '$setErrNo'],
  setenv__proxy: 'sync',
  setenv__sig: 'iiii',
  setenv: function(envname, envval, overwrite) {
    // int setenv(const char *envname, const char *envval, int overwrite);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/setenv.html
    if (envname === 0) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    var name = UTF8ToString(envname);
    var val = UTF8ToString(envval);
    if (name === '' || name.indexOf('=') !== -1) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    if (ENV.hasOwnProperty(name) && !overwrite) return 0;
    ENV[name] = val;
    ___buildEnvironment(__get_environ());
    return 0;
  },
  unsetenv__deps: ['$ENV', '__buildEnvironment', '$setErrNo'],
  unsetenv__proxy: 'sync',
  unsetenv__sig: 'ii',
  unsetenv: function(name) {
    // int unsetenv(const char *name);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/unsetenv.html
    if (name === 0) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    name = UTF8ToString(name);
    if (name === '' || name.indexOf('=') !== -1) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    if (ENV.hasOwnProperty(name)) {
      delete ENV[name];
      ___buildEnvironment(__get_environ());
    }
    return 0;
  },
  putenv__deps: ['$ENV', '__buildEnvironment', '$setErrNo'],
  putenv__proxy: 'sync',
  putenv__sig: 'ii',
  putenv: function(string) {
    // int putenv(char *string);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/putenv.html
    // WARNING: According to the standard (and the glibc implementation), the
    //          string is taken by reference so future changes are reflected.
    //          We copy it instead, possibly breaking some uses.
    if (string === 0) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    string = UTF8ToString(string);
    var splitPoint = string.indexOf('=')
    if (string === '' || string.indexOf('=') === -1) {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    var name = string.slice(0, splitPoint);
    var value = string.slice(splitPoint + 1);
    if (!(name in ENV) || ENV[name] !== value) {
      ENV[name] = value;
      ___buildEnvironment(__get_environ());
    }
    return 0;
  },
#endif

  getloadavg: function(loadavg, nelem) {
    // int getloadavg(double loadavg[], int nelem);
    // http://linux.die.net/man/3/getloadavg
    var limit = Math.min(nelem, 3);
    var doubleSize = {{{ Runtime.getNativeTypeSize('double') }}};
    for (var i = 0; i < limit; i++) {
      {{{ makeSetValue('loadavg', 'i * doubleSize', '0.1', 'double') }}};
    }
    return limit;
  },

  // ==========================================================================
  // string.h
  // ==========================================================================

  memcpy__inline: function(dest, src, num, align) {
    var ret = '';
    ret += makeCopyValues(dest, src, num, 'null', null, align);
    return ret;
  },

#if MIN_CHROME_VERSION < 45 || MIN_EDGE_VERSION < 14 || MIN_FIREFOX_VERSION < 34 || MIN_IE_VERSION != TARGET_NOT_SUPPORTED || MIN_SAFARI_VERSION < 100101 || STANDALONE_WASM
  // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/copyWithin lists browsers that support TypedArray.prototype.copyWithin, but it
  // has outdated information for Safari, saying it would not support it.
  // https://github.com/WebKit/webkit/commit/24a800eea4d82d6d595cdfec69d0f68e733b5c52#diff-c484911d8df319ba75fce0d8e7296333R1 suggests support was added on Aug 28, 2015.
  // Manual testing suggests:
  //   Safari/601.1 Version/9.0 on iPhone 4s with iOS 9.3.6 (released September 30, 2015) does not support copyWithin.
  // but the following systems do:
  //   AppleWebKit/602.2.14 Safari/602.1 Version/10.0 Mobile/14B100 iPhone OS 10_1_1 on iPhone 5s with iOS 10.1.1 (released October 31, 2016)
  //   AppleWebKit/603.3.8 Safari/602.1 Version/10.0 on iPhone 5 with iOS 10.3.4 (released July 22, 2019)
  //   AppleWebKit/605.1.15 iPhone OS 12_3_1 Version/12.1.1 Safari/604.1 on iPhone SE with iOS 12.3.1
  //   AppleWebKit/605.1.15 Safari/604.1 Version/13.0.4 iPhone OS 13_3 on iPhone 6s with iOS 13.3
  //   AppleWebKit/605.1.15 Version/13.0.3 Intel Mac OS X 10_15_1 on Safari 13.0.3 (15608.3.10.1.4) on macOS Catalina 10.15.1
  // Hence the support status of .copyWithin() for Safari version range [10.0.0, 10.1.0] is unknown.
  emscripten_memcpy_big__import: true,
  emscripten_memcpy_big: '= Uint8Array.prototype.copyWithin\n' +
    '  ? function(dest, src, num) { HEAPU8.copyWithin(dest, src, src + num); }\n' +
    '  : function(dest, src, num) { HEAPU8.set(HEAPU8.subarray(src, src+num), dest); }\n',
#else
  emscripten_memcpy_big: function(dest, src, num) {
    HEAPU8.copyWithin(dest, src, src + num);
  },
#endif

#if !WASM_BACKEND
  memcpy__asm: true,
  memcpy__sig: 'iiii',
  memcpy__deps: ['emscripten_memcpy_big', 'Int8Array', 'Int32Array'],
  memcpy: function(dest, src, num) {
    dest = dest|0; src = src|0; num = num|0;
    var ret = 0;
    var aligned_dest_end = 0;
    var block_aligned_dest_end = 0;
    var dest_end = 0;
    // Test against a benchmarked cutoff limit for when HEAPU8.copyWithin() becomes faster to use.
    if ((num|0) >= 512) {
      _emscripten_memcpy_big(dest|0, src|0, num|0)|0;
      return dest|0;
    }

    ret = dest|0;
    dest_end = (dest + num)|0;
    if ((dest&3) == (src&3)) {
      // The initial unaligned < 4-byte front.
      while (dest & 3) {
        if ((num|0) == 0) return ret|0;
        {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i8'), 'i8') }}};
        dest = (dest+1)|0;
        src = (src+1)|0;
        num = (num-1)|0;
      }
      aligned_dest_end = (dest_end & -4)|0;
#if FAST_UNROLLED_MEMCPY_AND_MEMSET
      block_aligned_dest_end = (aligned_dest_end - 64)|0;
      while ((dest|0) <= (block_aligned_dest_end|0) ) {
        {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 4, makeGetValueAsm('src', 4, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 8, makeGetValueAsm('src', 8, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 12, makeGetValueAsm('src', 12, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 16, makeGetValueAsm('src', 16, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 20, makeGetValueAsm('src', 20, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 24, makeGetValueAsm('src', 24, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 28, makeGetValueAsm('src', 28, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 32, makeGetValueAsm('src', 32, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 36, makeGetValueAsm('src', 36, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 40, makeGetValueAsm('src', 40, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 44, makeGetValueAsm('src', 44, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 48, makeGetValueAsm('src', 48, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 52, makeGetValueAsm('src', 52, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 56, makeGetValueAsm('src', 56, 'i32'), 'i32') }}};
        {{{ makeSetValueAsm('dest', 60, makeGetValueAsm('src', 60, 'i32'), 'i32') }}};
        dest = (dest+64)|0;
        src = (src+64)|0;
      }
#endif
      while ((dest|0) < (aligned_dest_end|0) ) {
        {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i32'), 'i32') }}};
        dest = (dest+4)|0;
        src = (src+4)|0;
      }
    } else {
      // In the unaligned copy case, unroll a bit as well.
      aligned_dest_end = (dest_end - 4)|0;
      while ((dest|0) < (aligned_dest_end|0) ) {
        {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i8'), 'i8') }}};
        {{{ makeSetValueAsm('dest', 1, makeGetValueAsm('src', 1, 'i8'), 'i8') }}};
        {{{ makeSetValueAsm('dest', 2, makeGetValueAsm('src', 2, 'i8'), 'i8') }}};
        {{{ makeSetValueAsm('dest', 3, makeGetValueAsm('src', 3, 'i8'), 'i8') }}};
        dest = (dest+4)|0;
        src = (src+4)|0;
      }
    }
    // The remaining unaligned < 4 byte tail.
    while ((dest|0) < (dest_end|0)) {
      {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i8'), 'i8') }}};
      dest = (dest+1)|0;
      src = (src+1)|0;
    }
    return ret|0;
  },

  memmove__sig: 'iiii',
  memmove__asm: true,
  memmove__deps: ['memcpy'],
  memmove: function(dest, src, num) {
    dest = dest|0; src = src|0; num = num|0;
    var ret = 0;
    if (((src|0) < (dest|0)) & ((dest|0) < ((src + num)|0))) {
      // Unlikely case: Copy backwards in a safe manner
      ret = dest;
      src = (src + num)|0;
      dest = (dest + num)|0;
      while ((num|0) > 0) {
        dest = (dest - 1)|0;
        src = (src - 1)|0;
        num = (num - 1)|0;
        {{{ makeSetValueAsm('dest', 0, makeGetValueAsm('src', 0, 'i8'), 'i8') }}};
      }
      dest = ret;
    } else {
      _memcpy(dest, src, num) | 0;
    }
    return dest | 0;
  },

  memset__inline: function(ptr, value, num, align) {
    return makeSetValues(ptr, 0, value, 'null', num, align);
  },
  memset__sig: 'iiii',
  memset__asm: true,
  memset__deps: ['Int8Array', 'Int32Array'],
  memset: function(ptr, value, num) {
    ptr = ptr|0; value = value|0; num = num|0;
    var end = 0, aligned_end = 0, block_aligned_end = 0, value4 = 0;
    end = (ptr + num)|0;

    value = value & 0xff;
    if ((num|0) >= 67 /* 64 bytes for an unrolled loop + 3 bytes for unaligned head*/) {
      while ((ptr&3) != 0) {
        {{{ makeSetValueAsm('ptr', 0, 'value', 'i8') }}};
        ptr = (ptr+1)|0;
      }

      aligned_end = (end & -4)|0;
      value4 = value | (value << 8) | (value << 16) | (value << 24);

#if FAST_UNROLLED_MEMCPY_AND_MEMSET
      block_aligned_end = (aligned_end - 64)|0;

      while((ptr|0) <= (block_aligned_end|0)) {
        {{{ makeSetValueAsm('ptr', 0, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 4, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 8, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 12, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 16, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 20, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 24, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 28, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 32, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 36, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 40, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 44, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 48, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 52, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 56, 'value4', 'i32') }}};
        {{{ makeSetValueAsm('ptr', 60, 'value4', 'i32') }}};
        ptr = (ptr + 64)|0;
      }
#endif

      while ((ptr|0) < (aligned_end|0) ) {
        {{{ makeSetValueAsm('ptr', 0, 'value4', 'i32') }}};
        ptr = (ptr+4)|0;
      }
    }
    // The remaining bytes.
    while ((ptr|0) < (end|0)) {
      {{{ makeSetValueAsm('ptr', 0, 'value', 'i8') }}};
      ptr = (ptr+1)|0;
    }
    return (end-num)|0;
  },
#endif // !WASM_BACKEND

  memcpy__sig: 'iiii',
  memmove__sig: 'iiii',
  memset__sig: 'iiii',

#if DECLARE_ASM_MODULE_EXPORTS
  llvm_memcpy_i32: 'memcpy',
  llvm_memcpy_i64: 'memcpy',
  llvm_memcpy_p0i8_p0i8_i32: 'memcpy',
  llvm_memcpy_p0i8_p0i8_i64: 'memcpy',

  llvm_memmove_i32: 'memmove',
  llvm_memmove_i64: 'memmove',
  llvm_memmove_p0i8_p0i8_i32: 'memmove',
  llvm_memmove_p0i8_p0i8_i64: 'memmove',

  llvm_memset_i32: 'memset',
  llvm_memset_p0i8_i32: 'memset',
  llvm_memset_p0i8_i64: 'memset',
#else
  // When DECLARE_ASM_MODULE_EXPORTS==0, cannot alias asm.js functions from non-asm.js
  // functions, so use an intermediate function as a pass-through.
  _memcpy_js__deps: ['memcpy'],
  _memcpy_js__sig: 'iiii',
  _memcpy_js: function(dst, src, num) {
    return _memcpy(dst, src, num);
  },

  _memmove_js__deps: ['memmove'],
  _memmove_js__sig: 'iiii',
  _memmove_js: function(dst, src, num) {
    return _memmove(dst, src, num);
  },

  _memset_js__deps: ['memset'],
  _memset_js__sig: 'iiii',
  _memset_js: function(ptr, value, num) {
    return _memset(ptr, value, num);
  },

  llvm_memcpy_i32: '_memcpy_js',
  llvm_memcpy_i64: '_memcpy_js',
  llvm_memcpy_p0i8_p0i8_i32: '_memcpy_js',
  llvm_memcpy_p0i8_p0i8_i64: '_memcpy_js',

  llvm_memmove_i32: '_memmove_js',
  llvm_memmove_i64: '_memmove_js',
  llvm_memmove_p0i8_p0i8_i32: '_memmove_js',
  llvm_memmove_p0i8_p0i8_i64: '_memmove_js',

  llvm_memset_i32: '_memset_js',
  llvm_memset_p0i8_i32: '_memset_js',
  llvm_memset_p0i8_i64: '_memset_js',
#endif // ~DECLARE_ASM_MODULE_EXPORTS
  // ==========================================================================
  // GCC/LLVM specifics
  // ==========================================================================
  __builtin_prefetch: function(){},

  // ==========================================================================
  // LLVM specifics
  // ==========================================================================

  llvm_va_start__inline: function(ptr) {
    // varargs - we received a pointer to the varargs as a final 'extra' parameter called 'varrp'
    // 2-word structure: struct { void* start; void* currentOffset; }
    return makeSetValue(ptr, 0, 'varrp', 'void*') + ';' + makeSetValue(ptr, Runtime.QUANTUM_SIZE, 0, 'void*');
  },

  llvm_va_end: function() {},

  llvm_va_copy: function(ppdest, ppsrc) {
    // copy the list start
    {{{ makeCopyValues('ppdest', 'ppsrc', Runtime.QUANTUM_SIZE, 'null', null, 1) }}};

    // copy the list's current offset (will be advanced with each call to va_arg)
    {{{ makeCopyValues('(ppdest+'+Runtime.QUANTUM_SIZE+')', '(ppsrc+'+Runtime.QUANTUM_SIZE+')', Runtime.QUANTUM_SIZE, 'null', null, 1) }}};
  },

  llvm_bswap_i16__asm: true,
  llvm_bswap_i16__sig: 'ii',
  llvm_bswap_i16: function(x) {
    x = x|0;
    return (((x&0xff)<<8) | ((x>>8)&0xff))|0;
  },

  llvm_bswap_i32__asm: true,
  llvm_bswap_i32__sig: 'ii',
  llvm_bswap_i32: function(x) {
    x = x|0;
    return (((x&0xff)<<24) | (((x>>8)&0xff)<<16) | (((x>>16)&0xff)<<8) | (x>>>24))|0;
  },

  llvm_bswap_i64__deps: ['llvm_bswap_i32'],
  llvm_bswap_i64: function(l, h) {
    var retl = _llvm_bswap_i32(h)>>>0;
    var reth = _llvm_bswap_i32(l)>>>0;
    {{{ makeStructuralReturn(['retl', 'reth']) }}};
  },

  llvm_ctlz_i8__asm: true,
  llvm_ctlz_i8__sig: 'ii',
  llvm_ctlz_i8__deps: ['Math_clz32'],
  llvm_ctlz_i8: function(x, isZeroUndef) {
    x = x | 0;
    isZeroUndef = isZeroUndef | 0;
    return (Math_clz32(x & 0xff) | 0) - 24 | 0;
  },

  llvm_ctlz_i16__asm: true,
  llvm_ctlz_i16__sig: 'ii',
  llvm_ctlz_i16__deps: ['Math_clz32'],
  llvm_ctlz_i16: function(x, isZeroUndef) {
    x = x | 0;
    isZeroUndef = isZeroUndef | 0;
    return (Math_clz32(x & 0xffff) | 0) - 16 | 0
  },

  llvm_ctlz_i64__asm: true,
  llvm_ctlz_i64__sig: 'iii',
  llvm_ctlz_i64__deps: ['Math_clz32'],
  llvm_ctlz_i64: function(l, h, isZeroUndef) {
    l = l | 0;
    h = h | 0;
    isZeroUndef = isZeroUndef | 0;
    var ret = 0;
    ret = Math_clz32(h) | 0;
    if ((ret | 0) == 32) ret = ret + (Math_clz32(l) | 0) | 0;
    {{{ makeSetTempRet0('0') }}};
    return ret | 0;
  },

#if WASM == 0 // binaryen will convert these calls to wasm anyhow
  llvm_cttz_i32__asm: true,
#endif
  llvm_cttz_i32__sig: 'ii',
  llvm_cttz_i32__deps: ['Math_clz32'],
  llvm_cttz_i32: function(x) { // Note: Currently doesn't take isZeroUndef()
    x = x | 0;
    return (x ? (31 - (Math_clz32((x ^ (x - 1))) | 0) | 0) : 32) | 0;
  },

  llvm_cttz_i64__deps: ['llvm_cttz_i32'],
  llvm_cttz_i64: function(l, h) {
    var ret = _llvm_cttz_i32(l);
    if (ret == 32) ret += _llvm_cttz_i32(h);
    {{{ makeStructuralReturn(['ret', '0']) }}};
  },

  llvm_ctpop_i32__asm: true,
  llvm_ctpop_i32__sig: 'ii',
  llvm_ctpop_i32__deps: ['Math_imul'],
  llvm_ctpop_i32: function(x) {
    // http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
    // http://bits.stephan-brumme.com/countBits.html
    x = x | 0;
    x = x - ((x >>> 1) & 0x55555555) | 0;
    x = (x & 0x33333333) + ((x >>> 2) & 0x33333333) | 0;
    return (Math_imul((x + (x >>> 4) & 252645135 /* 0xF0F0F0F, but hits uglify parse bug? */), 0x1010101) >>> 24) | 0;
  },

  llvm_ctpop_i64__deps: ['llvm_ctpop_i32'],
  llvm_ctpop_i64__asm: true,
  llvm_ctpop_i64__sig: 'iii',
  llvm_ctpop_i64: function(l, h) {
    l = l | 0;
    h = h | 0;
    return (_llvm_ctpop_i32(l) | 0) + (_llvm_ctpop_i32(h) | 0) | 0;
  },

  llvm_trap: function() {
    abort('trap!');
  },

  llvm_prefetch: function(){},

  __assert_fail: function(condition, filename, line, func) {
    abort('Assertion failed: ' + UTF8ToString(condition) + ', at: ' + [filename ? UTF8ToString(filename) : 'unknown filename', line, func ? UTF8ToString(func) : 'unknown function']);
  },

  __assert_func: function(filename, line, func, condition) {
    abort('Assertion failed: ' + (condition ? UTF8ToString(condition) : 'unknown condition') + ', at: ' + [filename ? UTF8ToString(filename) : 'unknown filename', line, func ? UTF8ToString(func) : 'unknown function']);
  },

#if WASM_BACKEND == 0
  setThrew__asm: true,
  setThrew__sig: 'vii',
  setThrew: function(threw, value) {
    threw = threw|0;
    value = value|0;
    if ((__THREW__|0) == 0) {
      __THREW__ = threw;
      threwValue = value;
    }
  },
#endif

  terminate__sig: 'vi',
  terminate: '__cxa_call_unexpected',

  __gxx_personality_v0: function() {
  },

  __gcc_personality_v0: function() {
  },

#if MINIMAL_RUNTIME && !WASM_BACKEND
  llvm_stacksave__deps: ['$stackSave'],
#endif
  llvm_stacksave: function() {
    var self = _llvm_stacksave;
    if (!self.LLVM_SAVEDSTACKS) {
      self.LLVM_SAVEDSTACKS = [];
    }
    self.LLVM_SAVEDSTACKS.push(stackSave());
    return self.LLVM_SAVEDSTACKS.length-1;
  },

#if MINIMAL_RUNTIME && !WASM_BACKEND
  llvm_stackrestore__deps: ['$stackRestore'],
#endif
  llvm_stackrestore: function(p) {
    var self = _llvm_stacksave;
    var ret = self.LLVM_SAVEDSTACKS[p];
    self.LLVM_SAVEDSTACKS.splice(p, 1);
    stackRestore(ret);
  },

#if MINIMAL_RUNTIME
#if WASM_BACKEND == 0
  $abortStackOverflow__deps: ['$stackSave'],
#endif
  $abortStackOverflow__import: true,
  $abortStackOverflow: function(allocSize) {
    abort('Stack overflow! Attempted to allocate ' + allocSize + ' bytes on the stack, but stack has only ' + (STACK_MAX - stackSave() + allocSize) + ' bytes available!');
  },

#if WASM_BACKEND == 0
  $stackAlloc__asm: true,
  $stackAlloc__sig: 'ii',
  $stackAlloc__deps: ['$abortStackOverflow'],
  $stackAlloc: function(size) {
    size = size|0;
    var ret = 0;
    ret = STACKTOP;
    STACKTOP = (STACKTOP + size)|0;
    STACKTOP = (STACKTOP + 15)&-16;
#if ASSERTIONS || STACK_OVERFLOW_CHECK >= 2
    if ((STACKTOP|0) >= (STACK_MAX|0)) abortStackOverflow(size|0);
#endif
    return ret|0;
  },

  $stackSave__asm: true,
  $stackSave__sig: 'i',
  $stackSave: function() {
    return STACKTOP|0;
  },

  $stackRestore__asm: true,
  $stackRestore__sig: 'vi',
  $stackRestore: function(top) {
    top = top|0;
    STACKTOP = top;
  },
#endif
#endif

  llvm_flt_rounds: function() {
    return -1; // 'indeterminable' for FLT_ROUNDS
  },

  llvm_expect_i32__inline: function(val, expected) {
    return '(' + val + ')';
  },

  llvm_objectsize_i32: function() { return -1 }, // TODO: support this

  llvm_dbg_declare__inline: function() { throw 'llvm_debug_declare' }, // avoid warning

  llvm_bitreverse_i32__asm: true,
  llvm_bitreverse_i32__sig: 'ii',
  llvm_bitreverse_i32: function(x) {
    x = x|0;
    x = ((x & 0xaaaaaaaa) >>> 1) | ((x & 0x55555555) << 1);
    x = ((x & 0xcccccccc) >>> 2) | ((x & 0x33333333) << 2);
    x = ((x & 0xf0f0f0f0) >>> 4) | ((x & 0x0f0f0f0f) << 4);
    x = ((x & 0xff00ff00) >>> 8) | ((x & 0x00ff00ff) << 8);
    return (x >>> 16) | (x << 16);
  },

  // llvm-nacl

  llvm_nacl_atomic_store_i32__inline: true,

  llvm_nacl_atomic_cmpxchg_i8__inline: true,
  llvm_nacl_atomic_cmpxchg_i16__inline: true,
  llvm_nacl_atomic_cmpxchg_i32__inline: true,

  // ==========================================================================
  // llvm-mono integration
  // ==========================================================================

  llvm_mono_load_i8_p0i8: function(ptr) {
    return {{{ makeGetValue('ptr', 0, 'i8') }}};
  },

  llvm_mono_store_i8_p0i8: function(value, ptr) {
    {{{ makeSetValue('ptr', 0, 'value', 'i8') }}};
  },

  llvm_mono_load_i16_p0i16: function(ptr) {
    return {{{ makeGetValue('ptr', 0, 'i16') }}};
  },

  llvm_mono_store_i16_p0i16: function(value, ptr) {
    {{{ makeSetValue('ptr', 0, 'value', 'i16') }}};
  },

  llvm_mono_load_i32_p0i32: function(ptr) {
    return {{{ makeGetValue('ptr', 0, 'i32') }}};
  },

  llvm_mono_store_i32_p0i32: function(value, ptr) {
    {{{ makeSetValue('ptr', 0, 'value', 'i32') }}};
  },

  // ==========================================================================
  // math.h
  // ==========================================================================

  cos: 'Math_cos',
  cosf: 'Math_cos',
  cosl: 'Math_cos',
  sin: 'Math_sin',
  sinf: 'Math_sin',
  sinl: 'Math_sin',
  tan: 'Math_tan',
  tanf: 'Math_tan',
  tanl: 'Math_tan',
  acos: 'Math_acos',
  acosf: 'Math_acos',
  acosl: 'Math_acos',
  asin: 'Math_asin',
  asinf: 'Math_asin',
  asinl: 'Math_asin',
  atan: 'Math_atan',
  atanf: 'Math_atan',
  atanl: 'Math_atan',
  atan2: 'Math_atan2',
  atan2f: 'Math_atan2',
  atan2l: 'Math_atan2',
  exp: 'Math_exp',
  expf: 'Math_exp',
  expl: 'Math_exp',
  log: 'Math_log',
  logf: 'Math_log',
  logl: 'Math_log',
  sqrt: 'Math_sqrt',
  sqrtf: 'Math_sqrt',
  sqrtl: 'Math_sqrt',
  fabs: 'Math_abs',
  fabsf: 'Math_abs',
  fabsl: 'Math_abs',
  llvm_fabs_f32: 'Math_abs',
  llvm_fabs_f64: 'Math_abs',
  ceil: 'Math_ceil',
  ceilf: 'Math_ceil',
  ceill: 'Math_ceil',
  floor: 'Math_floor',
  floorf: 'Math_floor',
  floorl: 'Math_floor',
  pow: 'Math_pow',
  powf: 'Math_pow',
  powl: 'Math_pow',
  llvm_sqrt_f32: 'Math_sqrt',
  llvm_sqrt_f64: 'Math_sqrt',
  llvm_pow_f32: 'Math_pow',
  llvm_pow_f64: 'Math_pow',
  llvm_powi_f32: 'Math_pow',
  llvm_powi_f64: 'Math_pow',
  llvm_log_f32: 'Math_log',
  llvm_log_f64: 'Math_log',
  llvm_exp_f32: 'Math_exp',
  llvm_exp_f64: 'Math_exp',
  llvm_cos_f32: 'Math_cos',
  llvm_cos_f64: 'Math_cos',
  llvm_sin_f32: 'Math_sin',
  llvm_sin_f64: 'Math_sin',
  llvm_trunc_f32: 'Math_trunc',
  llvm_trunc_f64: 'Math_trunc',
  llvm_ceil_f32: 'Math_ceil',
  llvm_ceil_f64: 'Math_ceil',
  llvm_floor_f32: 'Math_floor',
  llvm_floor_f64: 'Math_floor',

  llvm_exp2_f32: function(x) {
    return Math.pow(2, x);
  },
  llvm_exp2_f64__sig: 'dd',
  llvm_exp2_f64: 'llvm_exp2_f32',

  llvm_log2_f32: function(x) {
    return Math.log(x) / Math.LN2; // TODO: Math.log2, when browser support is there
  },
  llvm_log2_f64__sig: 'dd',
  llvm_log2_f64: 'llvm_log2_f32',

  llvm_log10_f32: function(x) {
    return Math.log(x) / Math.LN10; // TODO: Math.log10, when browser support is there
  },
  llvm_log10_f64__sig: 'dd',
  llvm_log10_f64: 'llvm_log10_f32',

  llvm_copysign_f32: function(x, y) {
    return y < 0 || (y === 0 && 1/y < 0) ? -Math_abs(x) : Math_abs(x);
  },

  llvm_copysign_f64: function(x, y) {
    return y < 0 || (y === 0 && 1/y < 0) ? -Math_abs(x) : Math_abs(x);
  },

  round__asm: true,
  round__sig: 'dd',
  round__deps: ['Math_floor', 'Math_ceil'],
  round: function(d) {
    d = +d;
    return d >= +0 ? +Math_floor(d + +0.5) : +Math_ceil(d - +0.5);
  },

  roundf__asm: true,
  roundf__sig: 'ff',
  roundf__deps: ['Math_floor', 'Math_ceil'],
  roundf: function(d) {
    d = +d;
    return d >= +0 ? +Math_floor(d + +0.5) : +Math_ceil(d - +0.5);
  },

  llvm_round_f64__asm: true,
  llvm_round_f64__sig: 'dd',
  llvm_round_f64__deps: ['Math_floor', 'Math_ceil'],
  llvm_round_f64: function(d) {
    d = +d;
    return d >= +0 ? +Math_floor(d + +0.5) : +Math_ceil(d - +0.5);
  },

  llvm_round_f32__asm: true,
  llvm_round_f32__sig: 'ff',
  llvm_round_f32__deps: ['Math_floor', 'Math_ceil'],
  llvm_round_f32: function(f) {
    f = +f;
    return f >= +0 ? +Math_floor(f + +0.5) : +Math_ceil(f - +0.5); // TODO: use fround?
  },

  rintf__asm: true,
  rintf__sig: 'ff',
  rintf__deps: ['round', 'Math_floor'],
  rintf: function(f) {
    f = +f;
    return (f - +Math_floor(f) != .5) ? +_round(f) : +_round(f / +2) * +2;
  },

  // TODO: fround?
  llvm_rint_f32__asm: true,
  llvm_rint_f32__sig: 'ff',
  llvm_rint_f32__deps: ['roundf', 'Math_floor'],
  llvm_rint_f32: function(f) {
    f = +f;
    return (f - +Math_floor(f) != .5) ? +_roundf(f) : +_roundf(f / +2) * +2;
  },

  llvm_rint_f64__asm: true,
  llvm_rint_f64__sig: 'dd',
  llvm_rint_f64__deps: ['round', 'Math_floor'],
  llvm_rint_f64: function(f) {
    f = +f;
    return (f - +Math_floor(f) != .5) ? +_round(f) : +_round(f / +2) * +2;
  },

  // TODO: fround?
  llvm_nearbyint_f32__asm: true,
  llvm_nearbyint_f32__sig: 'ff',
  llvm_nearbyint_f32__deps: ['roundf', 'Math_floor'],
  llvm_nearbyint_f32: function(f) {
    f = +f;
    return (f - +Math_floor(f) != .5) ? +_roundf(f) : +_roundf(f / +2) * +2;
  },

  llvm_nearbyint_f64__asm: true,
  llvm_nearbyint_f64__sig: 'dd',
  llvm_nearbyint_f64__deps: ['round', 'Math_floor'],
  llvm_nearbyint_f64: function(f) {
    f = +f;
    return (f - +Math_floor(f) != .5) ? +_round(f) : +_round(f / +2) * +2;
  },

  // min/max num do not quite match the behavior of JS and wasm min/max:
  // llvm and libc return the non-NaN if one is NaN, while JS and wasm
  // return the NaN :(
  // see also https://github.com/WebAssembly/design/issues/214
  llvm_minnum_f32__asm: true,
  llvm_minnum_f32__sig: 'ff',
  llvm_minnum_f32__deps: ['Math_min'],
  llvm_minnum_f32: function(x, y) {
    x = +x;
    y = +y;
    if (x != x) return +y;
    if (y != y) return +x;
    return +Math_min(+x, +y);
  },

  llvm_minnum_f64__asm: true,
  llvm_minnum_f64__sig: 'dd',
  llvm_minnum_f64__deps: ['Math_min'],
  llvm_minnum_f64: function(x, y) {
    x = +x;
    y = +y;
    if (x != x) return +y;
    if (y != y) return +x;
    return +Math_min(+x, +y);
  },

  llvm_maxnum_f32__asm: true,
  llvm_maxnum_f32__sig: 'ff',
  llvm_maxnum_f32__deps: ['Math_max'],
  llvm_maxnum_f32: function(x, y) {
    x = +x;
    y = +y;
    if (x != x) return +y;
    if (y != y) return +x;
    return +Math_max(+x, +y);
  },

  llvm_maxnum_f64__asm: true,
  llvm_maxnum_f64__sig: 'dd',
  llvm_maxnum_f64__deps: ['Math_max'],
  llvm_maxnum_f64: function(x, y) {
    x = +x;
    y = +y;
    if (x != x) return +y;
    if (y != y) return +x;
    return +Math_max(+x, +y);
  },

  // ==========================================================================
  // dlfcn.h - Dynamic library loading
  //
  // Some limitations:
  //
  //  * Minification on each file separately may not work, as they will
  //    have different shortened names. You can in theory combine them, then
  //    minify, then split... perhaps.
  //
  //  * LLVM optimizations may fail. If the child wants to access a function
  //    in the parent, LLVM opts may remove it from the parent when it is
  //    being compiled. Not sure how to tell LLVM to not do so.
  // ==========================================================================

#if MAIN_MODULE == 0
  dlopen: function(filename, flag) {
    abort("To use dlopen, you need to use Emscripten's linking support, see https://github.com/emscripten-core/emscripten/wiki/Linking");
  },
  dlclose: function(handle) {
    abort("To use dlopen, you need to use Emscripten's linking support, see https://github.com/emscripten-core/emscripten/wiki/Linking");
  },
  dlsym: function(handle, symbol) {
    abort("To use dlopen, you need to use Emscripten's linking support, see https://github.com/emscripten-core/emscripten/wiki/Linking");
  },
  dlerror: function() {
    abort("To use dlopen, you need to use Emscripten's linking support, see https://github.com/emscripten-core/emscripten/wiki/Linking");
  },
  dladdr: function(address, info) {
    abort("To use dlopen, you need to use Emscripten's linking support, see https://github.com/emscripten-core/emscripten/wiki/Linking");
  },
#else // MAIN_MODULE != 0

  $DLFCN: {
    error: null,
    errorMsg: null,
  },

  // void* dlopen(const char* filename, int flag);
  dlopen__deps: ['$DLFCN', '$FS', '$ENV'],
  dlopen__proxy: 'sync',
  dlopen__sig: 'iii',
  dlopen: function(filenameAddr, flag) {
    // void *dlopen(const char *file, int mode);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/dlopen.html
    var searchpaths = [];
    var filename;
    if (filenameAddr === 0) {
      filename = '__self__';
    } else {
      filename = UTF8ToString(filenameAddr);

      var isValidFile = function (filename) {
        var target = FS.findObject(filename);
        return target && !target.isFolder && !target.isDevice;
      };

      if (!isValidFile(filename)) {
        if (ENV['LD_LIBRARY_PATH']) {
          searchpaths = ENV['LD_LIBRARY_PATH'].split(':');
        }

        for (var ident in searchpaths) {
          var searchfile = PATH.join2(searchpaths[ident], filename);
          if (isValidFile(searchfile)) {
            filename = searchfile;
            break;
          }
        }
      }
    }

    // We don't care about RTLD_NOW and RTLD_LAZY.
    var flags = {
      global:   Boolean(flag & 256),  // RTLD_GLOBAL
      nodelete: Boolean(flag & 4096), // RTLD_NODELETE

      fs: FS, // load libraries from provided filesystem
    }

    try {
      var handle = loadDynamicLibrary(filename, flags)
    } catch (e) {
#if ASSERTIONS
      err('Error in loading dynamic library ' + filename + ": " + e);
#endif
      DLFCN.errorMsg = 'Could not load dynamic lib: ' + filename + '\n' + e;
      return 0;
    }

    return handle;
  },

  // int dlclose(void* handle);
  dlclose__deps: ['$DLFCN'],
  dlclose__proxy: 'sync',
  dlclose__sig: 'ii',
  dlclose: function(handle) {
    // int dlclose(void *handle);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/dlclose.html
    if (!LDSO.loadedLibs[handle]) {
      DLFCN.errorMsg = 'Tried to dlclose() unopened handle: ' + handle;
      return 1;
    } else {
      var lib_record = LDSO.loadedLibs[handle];
      if (--lib_record.refcount == 0) {
        if (lib_record.module.cleanups) {
          lib_record.module.cleanups.forEach(function(cleanup) { cleanup() });
        }
        delete LDSO.loadedLibNames[lib_record.name];
        delete LDSO.loadedLibs[handle];
      }
      return 0;
    }
  },

  // void* dlsym(void* handle, const char* symbol);
  dlsym__deps: ['$DLFCN'],
  dlsym__proxy: 'sync',
  dlsym__sig: 'iii',
  dlsym: function(handle, symbol) {
    // void *dlsym(void *restrict handle, const char *restrict name);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/dlsym.html
    symbol = UTF8ToString(symbol);

    if (!LDSO.loadedLibs[handle]) {
      DLFCN.errorMsg = 'Tried to dlsym() from an unopened handle: ' + handle;
      return 0;
    }

    var lib = LDSO.loadedLibs[handle];
    var isMainModule = lib.module == Module;

    var mangled = '_' + symbol;
    var modSymbol = mangled;
#if WASM_BACKEND
    if (!isMainModule) {
      modSymbol = symbol;
    }
#endif

    if (!lib.module.hasOwnProperty(modSymbol)) {
      DLFCN.errorMsg = ('Tried to lookup unknown symbol "' + modSymbol +
                             '" in dynamic lib: ' + lib.name);
      return 0;
    }

    var result = lib.module[modSymbol];
#if WASM
    // Attempt to get the real "unwrapped" symbol so we have more chance of
    // getting wasm function which can be added to a table.
    if (isMainModule) {
#if WASM_BACKEND
      var asmSymbol = symbol;
#else
      var asmSymbol = mangled;
#endif
      if (lib.module["asm"][asmSymbol]) {
        result = lib.module["asm"][asmSymbol];
      }
    }
#endif
    if (typeof result !== 'function')
      return result;

#if WASM && EMULATE_FUNCTION_POINTER_CASTS
    // for wasm with emulated function pointers, the i64 ABI is used for all
    // function calls, so we can't just call addFunction on something JS
    // can call (which does not use that ABI), as the function pointer would
    // not be usable from wasm. instead, the wasm has exported function pointers
    // for everything we need, with prefix fp$, use those
    result = lib.module['fp$' + symbol];
    if (typeof result === 'object') {
      // a breaking change in the wasm spec, globals are now objects
      // https://github.com/WebAssembly/mutable-global/issues/1
      result = result.value;
    }
#if ASSERTIONS
    assert(typeof result === 'number', 'could not find function pointer for ' + symbol);
#endif // ASSERTIONS
    return result;
#else // WASM && EMULATE_FUNCTION_POINTER_CASTS

#if WASM
    // Insert the function into the wasm table.  Since we know the function
    // comes directly from the loaded wasm module we can insert it directly
    // into the table, avoiding any JS interaction.
    return addFunctionWasm(result);
#else
    // convert the exported function into a function pointer using our generic
    // JS mechanism.
    return addFunction(result);
#endif // WASM
#endif // WASM && EMULATE_FUNCTION_POINTER_CASTS
  },

  // char* dlerror(void);
  dlerror__deps: ['$DLFCN', '$stringToNewUTF8'],
  dlerror__proxy: 'sync',
  dlerror__sig: 'i',
  dlerror: function() {
    // char *dlerror(void);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/dlerror.html
    if (DLFCN.errorMsg === null) {
      return 0;
    } else {
      if (DLFCN.error) _free(DLFCN.error);
      DLFCN.error = stringToNewUTF8(DLFCN.errorMsg);
      DLFCN.errorMsg = null;
      return DLFCN.error;
    }
  },

  dladdr__deps: ['$stringToNewUTF8', '_getExecutableName'],
  dladdr__proxy: 'sync',
  dladdr__sig: 'iii',
  dladdr: function(addr, info) {
    // report all function pointers as coming from this program itself XXX not really correct in any way
    var fname = stringToNewUTF8(__getExecutableName()); // XXX leak
    {{{ makeSetValue('info', 0, 'fname', 'i32') }}};
    {{{ makeSetValue('info', Runtime.QUANTUM_SIZE, '0', 'i32') }}};
    {{{ makeSetValue('info', Runtime.QUANTUM_SIZE*2, '0', 'i32') }}};
    {{{ makeSetValue('info', Runtime.QUANTUM_SIZE*3, '0', 'i32') }}};
    return 1;
  },
#endif // MAIN_MODULE != 0

  // ==========================================================================
  // pwd.h
  // ==========================================================================

  // TODO: Implement.
  // http://pubs.opengroup.org/onlinepubs/009695399/basedefs/pwd.h.html
  getpwuid: function(uid) {
    return 0; // NULL
  },


  // ==========================================================================
  // time.h
  // ==========================================================================

  clock: function() {
    if (_clock.start === undefined) _clock.start = Date.now();
    return ((Date.now() - _clock.start) * ({{{ cDefine('CLOCKS_PER_SEC') }}} / 1000))|0;
  },

  time: function(ptr) {
    var ret = (Date.now()/1000)|0;
    if (ptr) {
      {{{ makeSetValue('ptr', 0, 'ret', 'i32') }}};
    }
    return ret;
  },

  difftime: function(time1, time0) {
    return time1 - time0;
  },

  // Statically allocated time struct.
  __tm_current: '{{{ makeStaticAlloc(C_STRUCTS.tm.__size__) }}}',
  // Statically allocated copy of the string "GMT" for gmtime() to point to
  __tm_timezone: '{{{ makeStaticString("GMT") }}}',
  // Statically allocated time strings.
  __tm_formatted: '{{{ makeStaticAlloc(C_STRUCTS.tm.__size__) }}}',
  mktime__deps: ['tzset'],
  mktime__sig: 'ii',
  mktime: function(tmPtr) {
    _tzset();
    var date = new Date({{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_year, 'i32') }}} + 1900,
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mon, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mday, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_hour, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_min, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_sec, 'i32') }}},
                        0);

    // There's an ambiguous hour when the time goes back; the tm_isdst field is
    // used to disambiguate it.  Date() basically guesses, so we fix it up if it
    // guessed wrong, or fill in tm_isdst with the guess if it's -1.
    var dst = {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_isdst, 'i32') }}};
    var guessedOffset = date.getTimezoneOffset();
    var start = new Date(date.getFullYear(), 0, 1);
    var summerOffset = new Date(date.getFullYear(), 6, 1).getTimezoneOffset();
    var winterOffset = start.getTimezoneOffset();
    var dstOffset = Math.min(winterOffset, summerOffset); // DST is in December in South
    if (dst < 0) {
      // Attention: some regions don't have DST at all.
      {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_isdst, 'Number(summerOffset != winterOffset && dstOffset == guessedOffset)', 'i32') }}};
    } else if ((dst > 0) != (dstOffset == guessedOffset)) {
      var nonDstOffset = Math.max(winterOffset, summerOffset);
      var trueOffset = dst > 0 ? dstOffset : nonDstOffset;
      // Don't try setMinutes(date.getMinutes() + ...) -- it's messed up.
      date.setTime(date.getTime() + (trueOffset - guessedOffset)*60000);
    }

    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_wday, 'date.getDay()', 'i32') }}};
    var yday = ((date.getTime() - start.getTime()) / (1000 * 60 * 60 * 24))|0;
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_yday, 'yday', 'i32') }}};

    return (date.getTime() / 1000)|0;
  },
  timelocal: 'mktime',

  gmtime__deps: ['__tm_current', 'gmtime_r'],
  gmtime: function(time) {
    return _gmtime_r(time, ___tm_current);
  },

  gmtime_r__deps: ['__tm_timezone'],
  gmtime_r: function(time, tmPtr) {
    var date = new Date({{{ makeGetValue('time', 0, 'i32') }}}*1000);
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_sec, 'date.getUTCSeconds()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_min, 'date.getUTCMinutes()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_hour, 'date.getUTCHours()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_mday, 'date.getUTCDate()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_mon, 'date.getUTCMonth()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_year, 'date.getUTCFullYear()-1900', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_wday, 'date.getUTCDay()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_gmtoff, '0', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_isdst, '0', 'i32') }}};
    var start = Date.UTC(date.getUTCFullYear(), 0, 1, 0, 0, 0, 0);
    var yday = ((date.getTime() - start) / (1000 * 60 * 60 * 24))|0;
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_yday, 'yday', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_zone, '___tm_timezone', 'i32') }}};

    return tmPtr;
  },
  timegm__deps: ['tzset'],
  timegm: function(tmPtr) {
    _tzset();
    var time = Date.UTC({{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_year, 'i32') }}} + 1900,
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mon, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mday, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_hour, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_min, 'i32') }}},
                        {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_sec, 'i32') }}},
                        0);
    var date = new Date(time);

    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_wday, 'date.getUTCDay()', 'i32') }}};
    var start = Date.UTC(date.getUTCFullYear(), 0, 1, 0, 0, 0, 0);
    var yday = ((date.getTime() - start) / (1000 * 60 * 60 * 24))|0;
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_yday, 'yday', 'i32') }}};

    return (date.getTime() / 1000)|0;
  },

  localtime__deps: ['__tm_current', 'localtime_r'],
  localtime: function(time) {
    return _localtime_r(time, ___tm_current);
  },

  localtime_r__deps: ['__tm_timezone', 'tzset'],
  localtime_r: function(time, tmPtr) {
    _tzset();
    var date = new Date({{{ makeGetValue('time', 0, 'i32') }}}*1000);
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_sec, 'date.getSeconds()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_min, 'date.getMinutes()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_hour, 'date.getHours()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_mday, 'date.getDate()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_mon, 'date.getMonth()', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_year, 'date.getFullYear()-1900', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_wday, 'date.getDay()', 'i32') }}};

    var start = new Date(date.getFullYear(), 0, 1);
    var yday = ((date.getTime() - start.getTime()) / (1000 * 60 * 60 * 24))|0;
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_yday, 'yday', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_gmtoff, '-(date.getTimezoneOffset() * 60)', 'i32') }}};

    // Attention: DST is in December in South, and some regions don't have DST at all.
    var summerOffset = new Date(date.getFullYear(), 6, 1).getTimezoneOffset();
    var winterOffset = start.getTimezoneOffset();
    var dst = (summerOffset != winterOffset && date.getTimezoneOffset() == Math.min(winterOffset, summerOffset))|0;
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_isdst, 'dst', 'i32') }}};

    var zonePtr = {{{ makeGetValue('__get_tzname()', 'dst ? ' + Runtime.QUANTUM_SIZE + ' : 0', 'i32') }}};
    {{{ makeSetValue('tmPtr', C_STRUCTS.tm.tm_zone, 'zonePtr', 'i32') }}};

    return tmPtr;
  },

  asctime__deps: ['__tm_formatted', 'asctime_r'],
  asctime: function(tmPtr) {
    return _asctime_r(tmPtr, ___tm_formatted);
  },

  asctime_r__deps: ['__tm_formatted', 'mktime'],
  asctime_r: function(tmPtr, buf) {
    var date = {
      tm_sec: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_sec, 'i32') }}},
      tm_min: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_min, 'i32') }}},
      tm_hour: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_hour, 'i32') }}},
      tm_mday: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mday, 'i32') }}},
      tm_mon: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_mon, 'i32') }}},
      tm_year: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_year, 'i32') }}},
      tm_wday: {{{ makeGetValue('tmPtr', C_STRUCTS.tm.tm_wday, 'i32') }}}
    };
    var days = [ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" ];
    var months = [ "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" ];
    var s = days[date.tm_wday] + ' ' + months[date.tm_mon] +
        (date.tm_mday < 10 ? '  ' : ' ') + date.tm_mday +
        (date.tm_hour < 10 ? ' 0' : ' ') + date.tm_hour +
        (date.tm_min < 10 ? ':0' : ':') + date.tm_min +
        (date.tm_sec < 10 ? ':0' : ':') + date.tm_sec +
        ' ' + (1900 + date.tm_year) + "\n";

    // asctime_r is specced to behave in an undefined manner if the algorithm would attempt
    // to write out more than 26 bytes (including the null terminator).
    // See http://pubs.opengroup.org/onlinepubs/9699919799/functions/asctime.html
    // Our undefined behavior is to truncate the write to at most 26 bytes, including null terminator.
    stringToUTF8(s, buf, 26);
    return buf;
  },

  ctime__deps: ['__tm_current', 'ctime_r'],
  ctime: function(timer) {
    return _ctime_r(timer, ___tm_current);
  },

  ctime_r__deps: ['localtime_r', 'asctime_r'
#if MINIMAL_RUNTIME && !WASM_BACKEND
    , '$stackSave', '$stackAlloc', '$stackRestore'
#endif
  ],
  ctime_r: function(time, buf) {
    var stack = stackSave();
    var rv = _asctime_r(_localtime_r(time, stackAlloc({{{ C_STRUCTS.tm.__size__ }}})), buf);
    stackRestore(stack);
    return rv;
  },

  dysize: function(year) {
    var leap = ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)));
    return leap ? 366 : 365;
  },

  // TODO: Initialize these to defaults on startup from system settings.
  // Note: glibc has one fewer underscore for all of these. Also used in other related functions (timegm)
  tzset__proxy: 'sync',
  tzset__sig: 'v',
#if MINIMAL_RUNTIME
  tzset__deps: ['$allocateUTF8'],
#endif
  tzset: function() {
    // TODO: Use (malleable) environment variables instead of system settings.
    if (_tzset.called) return;
    _tzset.called = true;

    // timezone is specified as seconds west of UTC ("The external variable
    // `timezone` shall be set to the difference, in seconds, between
    // Coordinated Universal Time (UTC) and local standard time."), the same
    // as returned by getTimezoneOffset().
    // See http://pubs.opengroup.org/onlinepubs/009695399/functions/tzset.html
    {{{ makeSetValue('__get_timezone()', '0', '(new Date()).getTimezoneOffset() * 60', 'i32') }}};

    var currentYear = new Date().getFullYear();
    var winter = new Date(currentYear, 0, 1);
    var summer = new Date(currentYear, 6, 1);
    {{{ makeSetValue('__get_daylight()', '0', 'Number(winter.getTimezoneOffset() != summer.getTimezoneOffset())', 'i32') }}};

    function extractZone(date) {
      var match = date.toTimeString().match(/\(([A-Za-z ]+)\)$/);
      return match ? match[1] : "GMT";
    };
    var winterName = extractZone(winter);
    var summerName = extractZone(summer);
    var winterNamePtr = allocateUTF8(winterName);
    var summerNamePtr = allocateUTF8(summerName);
    if (summer.getTimezoneOffset() < winter.getTimezoneOffset()) {
      // Northern hemisphere
      {{{ makeSetValue('__get_tzname()', '0', 'winterNamePtr', 'i32') }}};
      {{{ makeSetValue('__get_tzname()', Runtime.QUANTUM_SIZE, 'summerNamePtr', 'i32') }}};
    } else {
      {{{ makeSetValue('__get_tzname()', '0', 'summerNamePtr', 'i32') }}};
      {{{ makeSetValue('__get_tzname()', Runtime.QUANTUM_SIZE, 'winterNamePtr', 'i32') }}};
    }
  },

  stime__deps: ['$setErrNo'],
  stime: function(when) {
    setErrNo({{{ cDefine('EPERM') }}});
    return -1;
  },

  __map_file__deps: ['$setErrNo'],
  __map_file: function(pathname, size) {
    setErrNo({{{ cDefine('EPERM') }}});
    return -1;
  },

  _MONTH_DAYS_REGULAR: [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31],
  _MONTH_DAYS_LEAP: [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31],

  _isLeapYear: function(year) {
      return year%4 === 0 && (year%100 !== 0 || year%400 === 0);
  },

  _arraySum: function(array, index) {
    var sum = 0;
    for (var i = 0; i <= index; sum += array[i++]) {
      // no-op
    }
    return sum;
  },

  _addDays__deps: ['_isLeapYear', '_MONTH_DAYS_LEAP', '_MONTH_DAYS_REGULAR'],
  _addDays: function(date, days) {
    var newDate = new Date(date.getTime());
    while(days > 0) {
      var leap = __isLeapYear(newDate.getFullYear());
      var currentMonth = newDate.getMonth();
      var daysInCurrentMonth = (leap ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR)[currentMonth];

      if (days > daysInCurrentMonth-newDate.getDate()) {
        // we spill over to next month
        days -= (daysInCurrentMonth-newDate.getDate()+1);
        newDate.setDate(1);
        if (currentMonth < 11) {
          newDate.setMonth(currentMonth+1)
        } else {
          newDate.setMonth(0);
          newDate.setFullYear(newDate.getFullYear()+1);
        }
      } else {
        // we stay in current month
        newDate.setDate(newDate.getDate()+days);
        return newDate;
      }
    }

    return newDate;
  },

  // Note: this is not used in STANDALONE_WASM mode, because it is more
  //       compact to do it in JS.
  strftime__deps: ['_isLeapYear', '_arraySum', '_addDays', '_MONTH_DAYS_REGULAR', '_MONTH_DAYS_LEAP'
#if MINIMAL_RUNTIME
    , '$intArrayFromString', '$writeArrayToMemory'
#endif
  ],
  strftime: function(s, maxsize, format, tm) {
    // size_t strftime(char *restrict s, size_t maxsize, const char *restrict format, const struct tm *restrict timeptr);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/strftime.html

    var tm_zone = {{{ makeGetValue('tm', C_STRUCTS.tm.tm_zone, 'i32') }}};

    var date = {
      tm_sec: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_sec, 'i32') }}},
      tm_min: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_min, 'i32') }}},
      tm_hour: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_hour, 'i32') }}},
      tm_mday: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_mday, 'i32') }}},
      tm_mon: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_mon, 'i32') }}},
      tm_year: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_year, 'i32') }}},
      tm_wday: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_wday, 'i32') }}},
      tm_yday: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_yday, 'i32') }}},
      tm_isdst: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_isdst, 'i32') }}},
      tm_gmtoff: {{{ makeGetValue('tm', C_STRUCTS.tm.tm_gmtoff, 'i32') }}},
      tm_zone: tm_zone ? UTF8ToString(tm_zone) : ''
    };

    var pattern = UTF8ToString(format);

    // expand format
    var EXPANSION_RULES_1 = {
      '%c': '%a %b %d %H:%M:%S %Y',     // Replaced by the locale's appropriate date and time representation - e.g., Mon Aug  3 14:02:01 2013
      '%D': '%m/%d/%y',                 // Equivalent to %m / %d / %y
      '%F': '%Y-%m-%d',                 // Equivalent to %Y - %m - %d
      '%h': '%b',                       // Equivalent to %b
      '%r': '%I:%M:%S %p',              // Replaced by the time in a.m. and p.m. notation
      '%R': '%H:%M',                    // Replaced by the time in 24-hour notation
      '%T': '%H:%M:%S',                 // Replaced by the time
      '%x': '%m/%d/%y',                 // Replaced by the locale's appropriate date representation
      '%X': '%H:%M:%S',                 // Replaced by the locale's appropriate time representation
      // Modified Conversion Specifiers
      '%Ec': '%c',                      // Replaced by the locale's alternative appropriate date and time representation.
      '%EC': '%C',                      // Replaced by the name of the base year (period) in the locale's alternative representation.
      '%Ex': '%m/%d/%y',                // Replaced by the locale's alternative date representation.
      '%EX': '%H:%M:%S',                // Replaced by the locale's alternative time representation.
      '%Ey': '%y',                      // Replaced by the offset from %EC (year only) in the locale's alternative representation.
      '%EY': '%Y',                      // Replaced by the full alternative year representation.
      '%Od': '%d',                      // Replaced by the day of the month, using the locale's alternative numeric symbols, filled as needed with leading zeros if there is any alternative symbol for zero; otherwise, with leading <space> characters.
      '%Oe': '%e',                      // Replaced by the day of the month, using the locale's alternative numeric symbols, filled as needed with leading <space> characters.
      '%OH': '%H',                      // Replaced by the hour (24-hour clock) using the locale's alternative numeric symbols.
      '%OI': '%I',                      // Replaced by the hour (12-hour clock) using the locale's alternative numeric symbols.
      '%Om': '%m',                      // Replaced by the month using the locale's alternative numeric symbols.
      '%OM': '%M',                      // Replaced by the minutes using the locale's alternative numeric symbols.
      '%OS': '%S',                      // Replaced by the seconds using the locale's alternative numeric symbols.
      '%Ou': '%u',                      // Replaced by the weekday as a number in the locale's alternative representation (Monday=1).
      '%OU': '%U',                      // Replaced by the week number of the year (Sunday as the first day of the week, rules corresponding to %U ) using the locale's alternative numeric symbols.
      '%OV': '%V',                      // Replaced by the week number of the year (Monday as the first day of the week, rules corresponding to %V ) using the locale's alternative numeric symbols.
      '%Ow': '%w',                      // Replaced by the number of the weekday (Sunday=0) using the locale's alternative numeric symbols.
      '%OW': '%W',                      // Replaced by the week number of the year (Monday as the first day of the week) using the locale's alternative numeric symbols.
      '%Oy': '%y',                      // Replaced by the year (offset from %C ) using the locale's alternative numeric symbols.
    };
    for (var rule in EXPANSION_RULES_1) {
      pattern = pattern.replace(new RegExp(rule, 'g'), EXPANSION_RULES_1[rule]);
    }

    var WEEKDAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    var MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];

    function leadingSomething(value, digits, character) {
      var str = typeof value === 'number' ? value.toString() : (value || '');
      while (str.length < digits) {
        str = character[0]+str;
      }
      return str;
    }

    function leadingNulls(value, digits) {
      return leadingSomething(value, digits, '0');
    }

    function compareByDay(date1, date2) {
      function sgn(value) {
        return value < 0 ? -1 : (value > 0 ? 1 : 0);
      }

      var compare;
      if ((compare = sgn(date1.getFullYear()-date2.getFullYear())) === 0) {
        if ((compare = sgn(date1.getMonth()-date2.getMonth())) === 0) {
          compare = sgn(date1.getDate()-date2.getDate());
        }
      }
      return compare;
    }

    function getFirstWeekStartDate(janFourth) {
        switch (janFourth.getDay()) {
          case 0: // Sunday
            return new Date(janFourth.getFullYear()-1, 11, 29);
          case 1: // Monday
            return janFourth;
          case 2: // Tuesday
            return new Date(janFourth.getFullYear(), 0, 3);
          case 3: // Wednesday
            return new Date(janFourth.getFullYear(), 0, 2);
          case 4: // Thursday
            return new Date(janFourth.getFullYear(), 0, 1);
          case 5: // Friday
            return new Date(janFourth.getFullYear()-1, 11, 31);
          case 6: // Saturday
            return new Date(janFourth.getFullYear()-1, 11, 30);
        }
    }

    function getWeekBasedYear(date) {
        var thisDate = __addDays(new Date(date.tm_year+1900, 0, 1), date.tm_yday);

        var janFourthThisYear = new Date(thisDate.getFullYear(), 0, 4);
        var janFourthNextYear = new Date(thisDate.getFullYear()+1, 0, 4);

        var firstWeekStartThisYear = getFirstWeekStartDate(janFourthThisYear);
        var firstWeekStartNextYear = getFirstWeekStartDate(janFourthNextYear);

        if (compareByDay(firstWeekStartThisYear, thisDate) <= 0) {
          // this date is after the start of the first week of this year
          if (compareByDay(firstWeekStartNextYear, thisDate) <= 0) {
            return thisDate.getFullYear()+1;
          } else {
            return thisDate.getFullYear();
          }
        } else {
          return thisDate.getFullYear()-1;
        }
    }

    var EXPANSION_RULES_2 = {
      '%a': function(date) {
        return WEEKDAYS[date.tm_wday].substring(0,3);
      },
      '%A': function(date) {
        return WEEKDAYS[date.tm_wday];
      },
      '%b': function(date) {
        return MONTHS[date.tm_mon].substring(0,3);
      },
      '%B': function(date) {
        return MONTHS[date.tm_mon];
      },
      '%C': function(date) {
        var year = date.tm_year+1900;
        return leadingNulls((year/100)|0,2);
      },
      '%d': function(date) {
        return leadingNulls(date.tm_mday, 2);
      },
      '%e': function(date) {
        return leadingSomething(date.tm_mday, 2, ' ');
      },
      '%g': function(date) {
        // %g, %G, and %V give values according to the ISO 8601:2000 standard week-based year.
        // In this system, weeks begin on a Monday and week 1 of the year is the week that includes
        // January 4th, which is also the week that includes the first Thursday of the year, and
        // is also the first week that contains at least four days in the year.
        // If the first Monday of January is the 2nd, 3rd, or 4th, the preceding days are part of
        // the last week of the preceding year; thus, for Saturday 2nd January 1999,
        // %G is replaced by 1998 and %V is replaced by 53. If December 29th, 30th,
        // or 31st is a Monday, it and any following days are part of week 1 of the following year.
        // Thus, for Tuesday 30th December 1997, %G is replaced by 1998 and %V is replaced by 01.

        return getWeekBasedYear(date).toString().substring(2);
      },
      '%G': function(date) {
        return getWeekBasedYear(date);
      },
      '%H': function(date) {
        return leadingNulls(date.tm_hour, 2);
      },
      '%I': function(date) {
        var twelveHour = date.tm_hour;
        if (twelveHour == 0) twelveHour = 12;
        else if (twelveHour > 12) twelveHour -= 12;
        return leadingNulls(twelveHour, 2);
      },
      '%j': function(date) {
        // Day of the year (001-366)
        return leadingNulls(date.tm_mday+__arraySum(__isLeapYear(date.tm_year+1900) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, date.tm_mon-1), 3);
      },
      '%m': function(date) {
        return leadingNulls(date.tm_mon+1, 2);
      },
      '%M': function(date) {
        return leadingNulls(date.tm_min, 2);
      },
      '%n': function() {
        return '\n';
      },
      '%p': function(date) {
        if (date.tm_hour >= 0 && date.tm_hour < 12) {
          return 'AM';
        } else {
          return 'PM';
        }
      },
      '%S': function(date) {
        return leadingNulls(date.tm_sec, 2);
      },
      '%t': function() {
        return '\t';
      },
      '%u': function(date) {
        return date.tm_wday || 7;
      },
      '%U': function(date) {
        // Replaced by the week number of the year as a decimal number [00,53].
        // The first Sunday of January is the first day of week 1;
        // days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday]
        var janFirst = new Date(date.tm_year+1900, 0, 1);
        var firstSunday = janFirst.getDay() === 0 ? janFirst : __addDays(janFirst, 7-janFirst.getDay());
        var endDate = new Date(date.tm_year+1900, date.tm_mon, date.tm_mday);

        // is target date after the first Sunday?
        if (compareByDay(firstSunday, endDate) < 0) {
          // calculate difference in days between first Sunday and endDate
          var februaryFirstUntilEndMonth = __arraySum(__isLeapYear(endDate.getFullYear()) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, endDate.getMonth()-1)-31;
          var firstSundayUntilEndJanuary = 31-firstSunday.getDate();
          var days = firstSundayUntilEndJanuary+februaryFirstUntilEndMonth+endDate.getDate();
          return leadingNulls(Math.ceil(days/7), 2);
        }

        return compareByDay(firstSunday, janFirst) === 0 ? '01': '00';
      },
      '%V': function(date) {
        // Replaced by the week number of the year (Monday as the first day of the week)
        // as a decimal number [01,53]. If the week containing 1 January has four
        // or more days in the new year, then it is considered week 1.
        // Otherwise, it is the last week of the previous year, and the next week is week 1.
        // Both January 4th and the first Thursday of January are always in week 1. [ tm_year, tm_wday, tm_yday]
        var janFourthThisYear = new Date(date.tm_year+1900, 0, 4);
        var janFourthNextYear = new Date(date.tm_year+1901, 0, 4);

        var firstWeekStartThisYear = getFirstWeekStartDate(janFourthThisYear);
        var firstWeekStartNextYear = getFirstWeekStartDate(janFourthNextYear);

        var endDate = __addDays(new Date(date.tm_year+1900, 0, 1), date.tm_yday);

        if (compareByDay(endDate, firstWeekStartThisYear) < 0) {
          // if given date is before this years first week, then it belongs to the 53rd week of last year
          return '53';
        }

        if (compareByDay(firstWeekStartNextYear, endDate) <= 0) {
          // if given date is after next years first week, then it belongs to the 01th week of next year
          return '01';
        }

        // given date is in between CW 01..53 of this calendar year
        var daysDifference;
        if (firstWeekStartThisYear.getFullYear() < date.tm_year+1900) {
          // first CW of this year starts last year
          daysDifference = date.tm_yday+32-firstWeekStartThisYear.getDate()
        } else {
          // first CW of this year starts this year
          daysDifference = date.tm_yday+1-firstWeekStartThisYear.getDate();
        }
        return leadingNulls(Math.ceil(daysDifference/7), 2);
      },
      '%w': function(date) {
        return date.tm_wday;
      },
      '%W': function(date) {
        // Replaced by the week number of the year as a decimal number [00,53].
        // The first Monday of January is the first day of week 1;
        // days in the new year before this are in week 0. [ tm_year, tm_wday, tm_yday]
        var janFirst = new Date(date.tm_year, 0, 1);
        var firstMonday = janFirst.getDay() === 1 ? janFirst : __addDays(janFirst, janFirst.getDay() === 0 ? 1 : 7-janFirst.getDay()+1);
        var endDate = new Date(date.tm_year+1900, date.tm_mon, date.tm_mday);

        // is target date after the first Monday?
        if (compareByDay(firstMonday, endDate) < 0) {
          var februaryFirstUntilEndMonth = __arraySum(__isLeapYear(endDate.getFullYear()) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, endDate.getMonth()-1)-31;
          var firstMondayUntilEndJanuary = 31-firstMonday.getDate();
          var days = firstMondayUntilEndJanuary+februaryFirstUntilEndMonth+endDate.getDate();
          return leadingNulls(Math.ceil(days/7), 2);
        }
        return compareByDay(firstMonday, janFirst) === 0 ? '01': '00';
      },
      '%y': function(date) {
        // Replaced by the last two digits of the year as a decimal number [00,99]. [ tm_year]
        return (date.tm_year+1900).toString().substring(2);
      },
      '%Y': function(date) {
        // Replaced by the year as a decimal number (for example, 1997). [ tm_year]
        return date.tm_year+1900;
      },
      '%z': function(date) {
        // Replaced by the offset from UTC in the ISO 8601:2000 standard format ( +hhmm or -hhmm ).
        // For example, "-0430" means 4 hours 30 minutes behind UTC (west of Greenwich).
        var off = date.tm_gmtoff;
        var ahead = off >= 0;
        off = Math.abs(off) / 60;
        // convert from minutes into hhmm format (which means 60 minutes = 100 units)
        off = (off / 60)*100 + (off % 60);
        return (ahead ? '+' : '-') + String("0000" + off).slice(-4);
      },
      '%Z': function(date) {
        return date.tm_zone;
      },
      '%%': function() {
        return '%';
      }
    };
    for (var rule in EXPANSION_RULES_2) {
      if (pattern.indexOf(rule) >= 0) {
        pattern = pattern.replace(new RegExp(rule, 'g'), EXPANSION_RULES_2[rule](date));
      }
    }

    var bytes = intArrayFromString(pattern, false);
    if (bytes.length > maxsize) {
      return 0;
    }

    writeArrayToMemory(bytes, s);
    return bytes.length-1;
  },
  strftime_l__deps: ['strftime'],
  strftime_l: function(s, maxsize, format, tm) {
    return _strftime(s, maxsize, format, tm); // no locale support yet
  },

  strptime__deps: ['_isLeapYear', '_arraySum', '_addDays', '_MONTH_DAYS_REGULAR', '_MONTH_DAYS_LEAP', '$jstoi_q'
#if MINIMAL_RUNTIME
    , '$intArrayFromString'
#endif
  ],
  strptime: function(buf, format, tm) {
    // char *strptime(const char *restrict buf, const char *restrict format, struct tm *restrict tm);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/strptime.html
    var pattern = UTF8ToString(format);

    // escape special characters
    // TODO: not sure we really need to escape all of these in JS regexps
    var SPECIAL_CHARS = '\\!@#$^&*()+=-[]/{}|:<>?,.';
    for (var i=0, ii=SPECIAL_CHARS.length; i<ii; ++i) {
      pattern = pattern.replace(new RegExp('\\'+SPECIAL_CHARS[i], 'g'), '\\'+SPECIAL_CHARS[i]);
    }

    // reduce number of matchers
    var EQUIVALENT_MATCHERS = {
      '%A':  '%a',
      '%B':  '%b',
      '%c':  '%a %b %d %H:%M:%S %Y',
      '%D':  '%m\\/%d\\/%y',
      '%e':  '%d',
      '%F':  '%Y-%m-%d',
      '%h':  '%b',
      '%R':  '%H\\:%M',
      '%r':  '%I\\:%M\\:%S\\s%p',
      '%T':  '%H\\:%M\\:%S',
      '%x':  '%m\\/%d\\/(?:%y|%Y)',
      '%X':  '%H\\:%M\\:%S'
    };
    for (var matcher in EQUIVALENT_MATCHERS) {
      pattern = pattern.replace(matcher, EQUIVALENT_MATCHERS[matcher]);
    }

    // TODO: take care of locale

    var DATE_PATTERNS = {
      /* weeday name */     '%a': '(?:Sun(?:day)?)|(?:Mon(?:day)?)|(?:Tue(?:sday)?)|(?:Wed(?:nesday)?)|(?:Thu(?:rsday)?)|(?:Fri(?:day)?)|(?:Sat(?:urday)?)',
      /* month name */      '%b': '(?:Jan(?:uary)?)|(?:Feb(?:ruary)?)|(?:Mar(?:ch)?)|(?:Apr(?:il)?)|May|(?:Jun(?:e)?)|(?:Jul(?:y)?)|(?:Aug(?:ust)?)|(?:Sep(?:tember)?)|(?:Oct(?:ober)?)|(?:Nov(?:ember)?)|(?:Dec(?:ember)?)',
      /* century */         '%C': '\\d\\d',
      /* day of month */    '%d': '0[1-9]|[1-9](?!\\d)|1\\d|2\\d|30|31',
      /* hour (24hr) */     '%H': '\\d(?!\\d)|[0,1]\\d|20|21|22|23',
      /* hour (12hr) */     '%I': '\\d(?!\\d)|0\\d|10|11|12',
      /* day of year */     '%j': '00[1-9]|0?[1-9](?!\\d)|0?[1-9]\\d(?!\\d)|[1,2]\\d\\d|3[0-6]\\d',
      /* month */           '%m': '0[1-9]|[1-9](?!\\d)|10|11|12',
      /* minutes */         '%M': '0\\d|\\d(?!\\d)|[1-5]\\d',
      /* whitespace */      '%n': '\\s',
      /* AM/PM */           '%p': 'AM|am|PM|pm|A\\.M\\.|a\\.m\\.|P\\.M\\.|p\\.m\\.',
      /* seconds */         '%S': '0\\d|\\d(?!\\d)|[1-5]\\d|60',
      /* week number */     '%U': '0\\d|\\d(?!\\d)|[1-4]\\d|50|51|52|53',
      /* week number */     '%W': '0\\d|\\d(?!\\d)|[1-4]\\d|50|51|52|53',
      /* weekday number */  '%w': '[0-6]',
      /* 2-digit year */    '%y': '\\d\\d',
      /* 4-digit year */    '%Y': '\\d\\d\\d\\d',
      /* % */               '%%': '%',
      /* whitespace */      '%t': '\\s',
    };

    var MONTH_NUMBERS = {JAN: 0, FEB: 1, MAR: 2, APR: 3, MAY: 4, JUN: 5, JUL: 6, AUG: 7, SEP: 8, OCT: 9, NOV: 10, DEC: 11};
    var DAY_NUMBERS_SUN_FIRST = {SUN: 0, MON: 1, TUE: 2, WED: 3, THU: 4, FRI: 5, SAT: 6};
    var DAY_NUMBERS_MON_FIRST = {MON: 0, TUE: 1, WED: 2, THU: 3, FRI: 4, SAT: 5, SUN: 6};

    for (var datePattern in DATE_PATTERNS) {
      pattern = pattern.replace(datePattern, '('+datePattern+DATE_PATTERNS[datePattern]+')');
    }

    // take care of capturing groups
    var capture = [];
    for (var i=pattern.indexOf('%'); i>=0; i=pattern.indexOf('%')) {
      capture.push(pattern[i+1]);
      pattern = pattern.replace(new RegExp('\\%'+pattern[i+1], 'g'), '');
    }

    var matches = new RegExp('^'+pattern, "i").exec(UTF8ToString(buf))
    // out(UTF8ToString(buf)+ ' is matched by '+((new RegExp('^'+pattern)).source)+' into: '+JSON.stringify(matches));

    function initDate() {
      function fixup(value, min, max) {
        return (typeof value !== 'number' || isNaN(value)) ? min : (value>=min ? (value<=max ? value: max): min);
      };
      return {
        year: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_year, 'i32', 0, 0, 1) }}} + 1900 , 1970, 9999),
        month: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_mon, 'i32', 0, 0, 1) }}}, 0, 11),
        day: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_mday, 'i32', 0, 0, 1) }}}, 1, 31),
        hour: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_hour, 'i32', 0, 0, 1) }}}, 0, 23),
        min: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_min, 'i32', 0, 0, 1) }}}, 0, 59),
        sec: fixup({{{ makeGetValue('tm', C_STRUCTS.tm.tm_sec, 'i32', 0, 0, 1) }}}, 0, 59)
      };
    };

    if (matches) {
      var date = initDate();
      var value;

      var getMatch = function(symbol) {
        var pos = capture.indexOf(symbol);
        // check if symbol appears in regexp
        if (pos >= 0) {
          // return matched value or null (falsy!) for non-matches
          return matches[pos+1];
        }
        return;
      };

      // seconds
      if ((value=getMatch('S'))) {
        date.sec = jstoi_q(value);
      }

      // minutes
      if ((value=getMatch('M'))) {
        date.min = jstoi_q(value);
      }

      // hours
      if ((value=getMatch('H'))) {
        // 24h clock
        date.hour = jstoi_q(value);
      } else if ((value = getMatch('I'))) {
        // AM/PM clock
        var hour = jstoi_q(value);
        if ((value=getMatch('p'))) {
          hour += value.toUpperCase()[0] === 'P' ? 12 : 0;
        }
        date.hour = hour;
      }

      // year
      if ((value=getMatch('Y'))) {
        // parse from four-digit year
        date.year = jstoi_q(value);
      } else if ((value=getMatch('y'))) {
        // parse from two-digit year...
        var year = jstoi_q(value);
        if ((value=getMatch('C'))) {
          // ...and century
          year += jstoi_q(value)*100;
        } else {
          // ...and rule-of-thumb
          year += year<69 ? 2000 : 1900;
        }
        date.year = year;
      }

      // month
      if ((value=getMatch('m'))) {
        // parse from month number
        date.month = jstoi_q(value)-1;
      } else if ((value=getMatch('b'))) {
        // parse from month name
        date.month = MONTH_NUMBERS[value.substring(0,3).toUpperCase()] || 0;
        // TODO: derive month from day in year+year, week number+day of week+year
      }

      // day
      if ((value=getMatch('d'))) {
        // get day of month directly
        date.day = jstoi_q(value);
      } else if ((value=getMatch('j'))) {
        // get day of month from day of year ...
        var day = jstoi_q(value);
        var leapYear = __isLeapYear(date.year);
        for (var month=0; month<12; ++month) {
          var daysUntilMonth = __arraySum(leapYear ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, month-1);
          if (day<=daysUntilMonth+(leapYear ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR)[month]) {
            date.day = day-daysUntilMonth;
          }
        }
      } else if ((value=getMatch('a'))) {
        // get day of month from weekday ...
        var weekDay = value.substring(0,3).toUpperCase();
        if ((value=getMatch('U'))) {
          // ... and week number (Sunday being first day of week)
          // Week number of the year (Sunday as the first day of the week) as a decimal number [00,53].
          // All days in a new year preceding the first Sunday are considered to be in week 0.
          var weekDayNumber = DAY_NUMBERS_SUN_FIRST[weekDay];
          var weekNumber = jstoi_q(value);

          // January 1st
          var janFirst = new Date(date.year, 0, 1);
          var endDate;
          if (janFirst.getDay() === 0) {
            // Jan 1st is a Sunday, and, hence in the 1st CW
            endDate = __addDays(janFirst, weekDayNumber+7*(weekNumber-1));
          } else {
            // Jan 1st is not a Sunday, and, hence still in the 0th CW
            endDate = __addDays(janFirst, 7-janFirst.getDay()+weekDayNumber+7*(weekNumber-1));
          }
          date.day = endDate.getDate();
          date.month = endDate.getMonth();
        } else if ((value=getMatch('W'))) {
          // ... and week number (Monday being first day of week)
          // Week number of the year (Monday as the first day of the week) as a decimal number [00,53].
          // All days in a new year preceding the first Monday are considered to be in week 0.
          var weekDayNumber = DAY_NUMBERS_MON_FIRST[weekDay];
          var weekNumber = jstoi_q(value);

          // January 1st
          var janFirst = new Date(date.year, 0, 1);
          var endDate;
          if (janFirst.getDay()===1) {
            // Jan 1st is a Monday, and, hence in the 1st CW
             endDate = __addDays(janFirst, weekDayNumber+7*(weekNumber-1));
          } else {
            // Jan 1st is not a Monday, and, hence still in the 0th CW
            endDate = __addDays(janFirst, 7-janFirst.getDay()+1+weekDayNumber+7*(weekNumber-1));
          }

          date.day = endDate.getDate();
          date.month = endDate.getMonth();
        }
      }

      /*
      tm_sec  int seconds after the minute  0-61*
      tm_min  int minutes after the hour  0-59
      tm_hour int hours since midnight  0-23
      tm_mday int day of the month  1-31
      tm_mon  int months since January  0-11
      tm_year int years since 1900
      tm_wday int days since Sunday 0-6
      tm_yday int days since January 1  0-365
      tm_isdst  int Daylight Saving Time flag
      */

      var fullDate = new Date(date.year, date.month, date.day, date.hour, date.min, date.sec, 0);
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_sec, 'fullDate.getSeconds()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_min, 'fullDate.getMinutes()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_hour, 'fullDate.getHours()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_mday, 'fullDate.getDate()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_mon, 'fullDate.getMonth()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_year, 'fullDate.getFullYear()-1900', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_wday, 'fullDate.getDay()', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_yday, '__arraySum(__isLeapYear(fullDate.getFullYear()) ? __MONTH_DAYS_LEAP : __MONTH_DAYS_REGULAR, fullDate.getMonth()-1)+fullDate.getDate()-1', 'i32') }}};
      {{{ makeSetValue('tm', C_STRUCTS.tm.tm_isdst, '0', 'i32') }}};

      // we need to convert the matched sequence into an integer array to take care of UTF-8 characters > 0x7F
      // TODO: not sure that intArrayFromString handles all unicode characters correctly
      return buf+intArrayFromString(matches[0]).length-1;
    }

    return 0;
  },
  strptime_l__deps: ['strptime'],
  strptime_l: function(buf, format, tm) {
    return _strptime(buf, format, tm); // no locale support yet
  },

  getdate: function(string) {
    // struct tm *getdate(const char *string);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/getdate.html
    // TODO: Implement.
    return 0;
  },

  timespec_get__deps: ['clock_gettime', '$setErrNo'],
  timespec_get: function(ts, base) {
    //int timespec_get(struct timespec *ts, int base);
    if (base !== {{{ cDefine('TIME_UTC') }}}) {
      // There is no other implemented value than TIME_UTC; all other values are considered erroneous.
      setErrNo({{{ cDefine('EINVAL') }}});
      return 0;
    }
    var ret = _clock_gettime({{{ cDefine('CLOCK_REALTIME') }}}, ts);
    return ret < 0 ? 0 : base;
  },

  // ==========================================================================
  // sys/time.h
  // ==========================================================================

  clock_gettime__deps: ['emscripten_get_now', 'emscripten_get_now_is_monotonic', '$setErrNo'],
  clock_gettime: function(clk_id, tp) {
    // int clock_gettime(clockid_t clk_id, struct timespec *tp);
    var now;
    if (clk_id === {{{ cDefine('CLOCK_REALTIME') }}}) {
      now = Date.now();
    } else if ((clk_id === {{{ cDefine('CLOCK_MONOTONIC') }}} || clk_id === {{{ cDefine('CLOCK_MONOTONIC_RAW') }}}) && _emscripten_get_now_is_monotonic) {
      now = _emscripten_get_now();
    } else {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    {{{ makeSetValue('tp', C_STRUCTS.timespec.tv_sec, '(now/1000)|0', 'i32') }}}; // seconds
    {{{ makeSetValue('tp', C_STRUCTS.timespec.tv_nsec, '((now % 1000)*1000*1000)|0', 'i32') }}}; // nanoseconds
    return 0;
  },
  __clock_gettime__sig: 'iii',
  __clock_gettime: 'clock_gettime', // musl internal alias
  clock_getres__deps: ['emscripten_get_now_res', 'emscripten_get_now_is_monotonic', '$setErrNo'],
  clock_getres: function(clk_id, res) {
    // int clock_getres(clockid_t clk_id, struct timespec *res);
    var nsec;
    if (clk_id === {{{ cDefine('CLOCK_REALTIME') }}}) {
      nsec = 1000 * 1000; // educated guess that it's milliseconds
    } else if (clk_id === {{{ cDefine('CLOCK_MONOTONIC') }}} && _emscripten_get_now_is_monotonic) {
      nsec = _emscripten_get_now_res();
    } else {
      setErrNo({{{ cDefine('EINVAL') }}});
      return -1;
    }
    {{{ makeSetValue('res', C_STRUCTS.timespec.tv_sec, '(nsec/1000000000)|0', 'i32') }}};
    {{{ makeSetValue('res', C_STRUCTS.timespec.tv_nsec, 'nsec', 'i32') }}} // resolution is nanoseconds
    return 0;
  },
  clock_getcpuclockid: function(pid, clk_id) {
    if (pid < 0) return {{{ cDefine('ESRCH') }}};
    if (pid !== 0 && pid !== {{{ PROCINFO.pid }}}) return {{{ cDefine('ENOSYS') }}};
    if (clk_id) {{{ makeSetValue('clk_id', 0, 2/*CLOCK_PROCESS_CPUTIME_ID*/, 'i32') }}};
    return 0;
  },
  // http://pubs.opengroup.org/onlinepubs/000095399/basedefs/sys/time.h.html
  gettimeofday: function(ptr) {
    var now = Date.now();
    {{{ makeSetValue('ptr', C_STRUCTS.timeval.tv_sec, '(now/1000)|0', 'i32') }}}; // seconds
    {{{ makeSetValue('ptr', C_STRUCTS.timeval.tv_usec, '((now % 1000)*1000)|0', 'i32') }}}; // microseconds
    return 0;
  },

  // ==========================================================================
  // sys/timeb.h
  // ==========================================================================

  ftime: function(p) {
    var millis = Date.now();
    {{{ makeSetValue('p', C_STRUCTS.timeb.time, '(millis/1000)|0', 'i32') }}};
    {{{ makeSetValue('p', C_STRUCTS.timeb.millitm, 'millis % 1000', 'i16') }}};
    {{{ makeSetValue('p', C_STRUCTS.timeb.timezone, '0', 'i16') }}}; // Obsolete field
    {{{ makeSetValue('p', C_STRUCTS.timeb.dstflag, '0', 'i16') }}}; // Obsolete field
    return 0;
  },

  // ==========================================================================
  // sys/times.h
  // ==========================================================================

#if !WASM_BACKEND
  times__deps: ['memset'],
#endif
  times: function(buffer) {
    // clock_t times(struct tms *buffer);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/times.html
    // NOTE: This is fake, since we can't calculate real CPU time usage in JS.
    if (buffer !== 0) {
      _memset(buffer, 0, {{{ C_STRUCTS.tms.__size__ }}});
    }
    return 0;
  },

  // ==========================================================================
  // sys/types.h
  // ==========================================================================
  // http://www.kernel.org/doc/man-pages/online/pages/man3/minor.3.html
  makedev__sig: 'iii',
  makedev: function(maj, min) {
    return ((maj) << 8 | (min));
  },
  gnu_dev_makedev: 'makedev',
  major__sig: 'ii',
  major: function(dev) {
    return ((dev) >> 8);
  },
  gnu_dev_major: 'major',
  minor__sig: 'ii',
  minor: function(dev) {
    return ((dev) & 0xff);
  },
  gnu_dev_minor: 'minor',

  // ==========================================================================
  // setjmp.h
  // ==========================================================================

#if SUPPORT_LONGJMP
  // asm.js-style setjmp/longjmp support for wasm binaryen backend.
  // In asm.js compilation, various variables including setjmpId will be
  // generated within 'var asm' in emscripten.py, while in wasm compilation,
  // wasm side is considered as 'asm' so they are not generated. But
  // saveSetjmp() needs setjmpId and no other functions in wasm side needs it.
  // So we declare it here if WASM_BACKEND=1.
#if WASM_BACKEND == 1
  $setjmpId: 0,
#endif

  saveSetjmp__asm: true,
  saveSetjmp__sig: 'iii',
#if WASM_BACKEND == 1
  saveSetjmp__deps: ['realloc', '$setjmpId'],
#else
  saveSetjmp__deps: ['realloc'],
#endif
  saveSetjmp: function(env, label, table, size) {
    // Not particularly fast: slow table lookup of setjmpId to label. But setjmp
    // prevents relooping anyhow, so slowness is to be expected. And typical case
    // is 1 setjmp per invocation, or less.
    env = env|0;
    label = label|0;
    table = table|0;
    size = size|0;
    var i = 0;
    setjmpId = (setjmpId+1)|0;
    {{{ makeSetValueAsm('env', '0', 'setjmpId', 'i32') }}};
    while ((i|0) < (size|0)) {
      if ({{{ makeGetValueAsm('table', '(i<<3)', 'i32') }}} == 0) {
        {{{ makeSetValueAsm('table', '(i<<3)', 'setjmpId', 'i32') }}};
        {{{ makeSetValueAsm('table', '(i<<3)+4', 'label', 'i32') }}};
        // prepare next slot
        {{{ makeSetValueAsm('table', '(i<<3)+8', '0', 'i32') }}};
        {{{ makeSetTempRet0('size') }}};
        return table | 0;
      }
      i = i+1|0;
    }
    // grow the table
    size = (size*2)|0;
    table = _realloc(table|0, 8*(size+1|0)|0) | 0;
    table = _saveSetjmp(env|0, label|0, table|0, size|0) | 0;
    {{{ makeSetTempRet0('size') }}};
    return table | 0;
  },

  testSetjmp__asm: true,
  testSetjmp__sig: 'iii',
  testSetjmp: function(id, table, size) {
    id = id|0;
    table = table|0;
    size = size|0;
    var i = 0, curr = 0;
    while ((i|0) < (size|0)) {
      curr = {{{ makeGetValueAsm('table', '(i<<3)', 'i32') }}};
      if ((curr|0) == 0) break;
      if ((curr|0) == (id|0)) {
        return {{{ makeGetValueAsm('table', '(i<<3)+4', 'i32') }}};
      }
      i = i+1|0;
    }
    return 0;
  },

  setjmp__deps: ['saveSetjmp', 'testSetjmp'],
  setjmp__inline: function(env) {
    // Save the label
    return '_saveSetjmp(' + env + ', label, setjmpTable)|0';
  },

  longjmp__deps: ['saveSetjmp', 'testSetjmp'
#if WASM_BACKEND == 0
  , 'setThrew'
#endif
  ],
  longjmp: function(env, value) {
    _setThrew(env, value || 1);
    throw 'longjmp';
  },
  emscripten_longjmp__deps: ['longjmp'],
  emscripten_longjmp: function(env, value) {
    _longjmp(env, value);
  },
#endif

  // ==========================================================================
  // sys/wait.h
  // ==========================================================================

  wait__deps: ['$setErrNo'],
  wait__sig: 'ii',
  wait: function(stat_loc) {
    // pid_t wait(int *stat_loc);
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/wait.html
    // Makes no sense in a single-process environment.
    setErrNo({{{ cDefine('ECHILD') }}});
    return -1;
  },
  // NOTE: These aren't really the same, but we use the same stub for them all.
  waitid: 'wait',
  waitpid: 'wait',
  wait3: 'wait',
  wait4: 'wait',

  // ==========================================================================
  // errno.h
  // ==========================================================================

  $ERRNO_CODES: {
    EPERM: {{{ cDefine('EPERM') }}},
    ENOENT: {{{ cDefine('ENOENT') }}},
    ESRCH: {{{ cDefine('ESRCH') }}},
    EINTR: {{{ cDefine('EINTR') }}},
    EIO: {{{ cDefine('EIO') }}},
    ENXIO: {{{ cDefine('ENXIO') }}},
    E2BIG: {{{ cDefine('E2BIG') }}},
    ENOEXEC: {{{ cDefine('ENOEXEC') }}},
    EBADF: {{{ cDefine('EBADF') }}},
    ECHILD: {{{ cDefine('ECHILD') }}},
    EAGAIN: {{{ cDefine('EAGAIN') }}},
    EWOULDBLOCK: {{{ cDefine('EWOULDBLOCK') }}},
    ENOMEM: {{{ cDefine('ENOMEM') }}},
    EACCES: {{{ cDefine('EACCES') }}},
    EFAULT: {{{ cDefine('EFAULT') }}},
    ENOTBLK: {{{ cDefine('ENOTBLK') }}},
    EBUSY: {{{ cDefine('EBUSY') }}},
    EEXIST: {{{ cDefine('EEXIST') }}},
    EXDEV: {{{ cDefine('EXDEV') }}},
    ENODEV: {{{ cDefine('ENODEV') }}},
    ENOTDIR: {{{ cDefine('ENOTDIR') }}},
    EISDIR: {{{ cDefine('EISDIR') }}},
    EINVAL: {{{ cDefine('EINVAL') }}},
    ENFILE: {{{ cDefine('ENFILE') }}},
    EMFILE: {{{ cDefine('EMFILE') }}},
    ENOTTY: {{{ cDefine('ENOTTY') }}},
    ETXTBSY: {{{ cDefine('ETXTBSY') }}},
    EFBIG: {{{ cDefine('EFBIG') }}},
    ENOSPC: {{{ cDefine('ENOSPC') }}},
    ESPIPE: {{{ cDefine('ESPIPE') }}},
    EROFS: {{{ cDefine('EROFS') }}},
    EMLINK: {{{ cDefine('EMLINK') }}},
    EPIPE: {{{ cDefine('EPIPE') }}},
    EDOM: {{{ cDefine('EDOM') }}},
    ERANGE: {{{ cDefine('ERANGE') }}},
    ENOMSG: {{{ cDefine('ENOMSG') }}},
    EIDRM: {{{ cDefine('EIDRM') }}},
    ECHRNG: {{{ cDefine('ECHRNG') }}},
    EL2NSYNC: {{{ cDefine('EL2NSYNC') }}},
    EL3HLT: {{{ cDefine('EL3HLT') }}},
    EL3RST: {{{ cDefine('EL3RST') }}},
    ELNRNG: {{{ cDefine('ELNRNG') }}},
    EUNATCH: {{{ cDefine('EUNATCH') }}},
    ENOCSI: {{{ cDefine('ENOCSI') }}},
    EL2HLT: {{{ cDefine('EL2HLT') }}},
    EDEADLK: {{{ cDefine('EDEADLK') }}},
    ENOLCK: {{{ cDefine('ENOLCK') }}},
    EBADE: {{{ cDefine('EBADE') }}},
    EBADR: {{{ cDefine('EBADR') }}},
    EXFULL: {{{ cDefine('EXFULL') }}},
    ENOANO: {{{ cDefine('ENOANO') }}},
    EBADRQC: {{{ cDefine('EBADRQC') }}},
    EBADSLT: {{{ cDefine('EBADSLT') }}},
    EDEADLOCK: {{{ cDefine('EDEADLOCK') }}},
    EBFONT: {{{ cDefine('EBFONT') }}},
    ENOSTR: {{{ cDefine('ENOSTR') }}},
    ENODATA: {{{ cDefine('ENODATA') }}},
    ETIME: {{{ cDefine('ETIME') }}},
    ENOSR: {{{ cDefine('ENOSR') }}},
    ENONET: {{{ cDefine('ENONET') }}},
    ENOPKG: {{{ cDefine('ENOPKG') }}},
    EREMOTE: {{{ cDefine('EREMOTE') }}},
    ENOLINK: {{{ cDefine('ENOLINK') }}},
    EADV: {{{ cDefine('EADV') }}},
    ESRMNT: {{{ cDefine('ESRMNT') }}},
    ECOMM: {{{ cDefine('ECOMM') }}},
    EPROTO: {{{ cDefine('EPROTO') }}},
    EMULTIHOP: {{{ cDefine('EMULTIHOP') }}},
    EDOTDOT: {{{ cDefine('EDOTDOT') }}},
    EBADMSG: {{{ cDefine('EBADMSG') }}},
    ENOTUNIQ: {{{ cDefine('ENOTUNIQ') }}},
    EBADFD: {{{ cDefine('EBADFD') }}},
    EREMCHG: {{{ cDefine('EREMCHG') }}},
    ELIBACC: {{{ cDefine('ELIBACC') }}},
    ELIBBAD: {{{ cDefine('ELIBBAD') }}},
    ELIBSCN: {{{ cDefine('ELIBSCN') }}},
    ELIBMAX: {{{ cDefine('ELIBMAX') }}},
    ELIBEXEC: {{{ cDefine('ELIBEXEC') }}},
    ENOSYS: {{{ cDefine('ENOSYS') }}},
    ENOTEMPTY: {{{ cDefine('ENOTEMPTY') }}},
    ENAMETOOLONG: {{{ cDefine('ENAMETOOLONG') }}},
    ELOOP: {{{ cDefine('ELOOP') }}},
    EOPNOTSUPP: {{{ cDefine('EOPNOTSUPP') }}},
    EPFNOSUPPORT: {{{ cDefine('EPFNOSUPPORT') }}},
    ECONNRESET: {{{ cDefine('ECONNRESET') }}},
    ENOBUFS: {{{ cDefine('ENOBUFS') }}},
    EAFNOSUPPORT: {{{ cDefine('EAFNOSUPPORT') }}},
    EPROTOTYPE: {{{ cDefine('EPROTOTYPE') }}},
    ENOTSOCK: {{{ cDefine('ENOTSOCK') }}},
    ENOPROTOOPT: {{{ cDefine('ENOPROTOOPT') }}},
    ESHUTDOWN: {{{ cDefine('ESHUTDOWN') }}},
    ECONNREFUSED: {{{ cDefine('ECONNREFUSED') }}},
    EADDRINUSE: {{{ cDefine('EADDRINUSE') }}},
    ECONNABORTED: {{{ cDefine('ECONNABORTED') }}},
    ENETUNREACH: {{{ cDefine('ENETUNREACH') }}},
    ENETDOWN: {{{ cDefine('ENETDOWN') }}},
    ETIMEDOUT: {{{ cDefine('ETIMEDOUT') }}},
    EHOSTDOWN: {{{ cDefine('EHOSTDOWN') }}},
    EHOSTUNREACH: {{{ cDefine('EHOSTUNREACH') }}},
    EINPROGRESS: {{{ cDefine('EINPROGRESS') }}},
    EALREADY: {{{ cDefine('EALREADY') }}},
    EDESTADDRREQ: {{{ cDefine('EDESTADDRREQ') }}},
    EMSGSIZE: {{{ cDefine('EMSGSIZE') }}},
    EPROTONOSUPPORT: {{{ cDefine('EPROTONOSUPPORT') }}},
    ESOCKTNOSUPPORT: {{{ cDefine('ESOCKTNOSUPPORT') }}},
    EADDRNOTAVAIL: {{{ cDefine('EADDRNOTAVAIL') }}},
    ENETRESET: {{{ cDefine('ENETRESET') }}},
    EISCONN: {{{ cDefine('EISCONN') }}},
    ENOTCONN: {{{ cDefine('ENOTCONN') }}},
    ETOOMANYREFS: {{{ cDefine('ETOOMANYREFS') }}},
    EUSERS: {{{ cDefine('EUSERS') }}},
    EDQUOT: {{{ cDefine('EDQUOT') }}},
    ESTALE: {{{ cDefine('ESTALE') }}},
    ENOTSUP: {{{ cDefine('ENOTSUP') }}},
    ENOMEDIUM: {{{ cDefine('ENOMEDIUM') }}},
    EILSEQ: {{{ cDefine('EILSEQ') }}},
    EOVERFLOW: {{{ cDefine('EOVERFLOW') }}},
    ECANCELED: {{{ cDefine('ECANCELED') }}},
    ENOTRECOVERABLE: {{{ cDefine('ENOTRECOVERABLE') }}},
    EOWNERDEAD: {{{ cDefine('EOWNERDEAD') }}},
    ESTRPIPE: {{{ cDefine('ESTRPIPE') }}},
  },
  $ERRNO_MESSAGES: {
    0: 'Success',
    {{{ cDefine('EPERM') }}}: 'Not super-user',
    {{{ cDefine('ENOENT') }}}: 'No such file or directory',
    {{{ cDefine('ESRCH') }}}: 'No such process',
    {{{ cDefine('EINTR') }}}: 'Interrupted system call',
    {{{ cDefine('EIO') }}}: 'I/O error',
    {{{ cDefine('ENXIO') }}}: 'No such device or address',
    {{{ cDefine('E2BIG') }}}: 'Arg list too long',
    {{{ cDefine('ENOEXEC') }}}: 'Exec format error',
    {{{ cDefine('EBADF') }}}: 'Bad file number',
    {{{ cDefine('ECHILD') }}}: 'No children',
    {{{ cDefine('EWOULDBLOCK') }}}: 'No more processes',
    {{{ cDefine('ENOMEM') }}}: 'Not enough core',
    {{{ cDefine('EACCES') }}}: 'Permission denied',
    {{{ cDefine('EFAULT') }}}: 'Bad address',
    {{{ cDefine('ENOTBLK') }}}: 'Block device required',
    {{{ cDefine('EBUSY') }}}: 'Mount device busy',
    {{{ cDefine('EEXIST') }}}: 'File exists',
    {{{ cDefine('EXDEV') }}}: 'Cross-device link',
    {{{ cDefine('ENODEV') }}}: 'No such device',
    {{{ cDefine('ENOTDIR') }}}: 'Not a directory',
    {{{ cDefine('EISDIR') }}}: 'Is a directory',
    {{{ cDefine('EINVAL') }}}: 'Invalid argument',
    {{{ cDefine('ENFILE') }}}: 'Too many open files in system',
    {{{ cDefine('EMFILE') }}}: 'Too many open files',
    {{{ cDefine('ENOTTY') }}}: 'Not a typewriter',
    {{{ cDefine('ETXTBSY') }}}: 'Text file busy',
    {{{ cDefine('EFBIG') }}}: 'File too large',
    {{{ cDefine('ENOSPC') }}}: 'No space left on device',
    {{{ cDefine('ESPIPE') }}}: 'Illegal seek',
    {{{ cDefine('EROFS') }}}: 'Read only file system',
    {{{ cDefine('EMLINK') }}}: 'Too many links',
    {{{ cDefine('EPIPE') }}}: 'Broken pipe',
    {{{ cDefine('EDOM') }}}: 'Math arg out of domain of func',
    {{{ cDefine('ERANGE') }}}: 'Math result not representable',
    {{{ cDefine('ENOMSG') }}}: 'No message of desired type',
    {{{ cDefine('EIDRM') }}}: 'Identifier removed',
    {{{ cDefine('ECHRNG') }}}: 'Channel number out of range',
    {{{ cDefine('EL2NSYNC') }}}: 'Level 2 not synchronized',
    {{{ cDefine('EL3HLT') }}}: 'Level 3 halted',
    {{{ cDefine('EL3RST') }}}: 'Level 3 reset',
    {{{ cDefine('ELNRNG') }}}: 'Link number out of range',
    {{{ cDefine('EUNATCH') }}}: 'Protocol driver not attached',
    {{{ cDefine('ENOCSI') }}}: 'No CSI structure available',
    {{{ cDefine('EL2HLT') }}}: 'Level 2 halted',
    {{{ cDefine('EDEADLK') }}}: 'Deadlock condition',
    {{{ cDefine('ENOLCK') }}}: 'No record locks available',
    {{{ cDefine('EBADE') }}}: 'Invalid exchange',
    {{{ cDefine('EBADR') }}}: 'Invalid request descriptor',
    {{{ cDefine('EXFULL') }}}: 'Exchange full',
    {{{ cDefine('ENOANO') }}}: 'No anode',
    {{{ cDefine('EBADRQC') }}}: 'Invalid request code',
    {{{ cDefine('EBADSLT') }}}: 'Invalid slot',
    {{{ cDefine('EDEADLOCK') }}}: 'File locking deadlock error',
    {{{ cDefine('EBFONT') }}}: 'Bad font file fmt',
    {{{ cDefine('ENOSTR') }}}: 'Device not a stream',
    {{{ cDefine('ENODATA') }}}: 'No data (for no delay io)',
    {{{ cDefine('ETIME') }}}: 'Timer expired',
    {{{ cDefine('ENOSR') }}}: 'Out of streams resources',
    {{{ cDefine('ENONET') }}}: 'Machine is not on the network',
    {{{ cDefine('ENOPKG') }}}: 'Package not installed',
    {{{ cDefine('EREMOTE') }}}: 'The object is remote',
    {{{ cDefine('ENOLINK') }}}: 'The link has been severed',
    {{{ cDefine('EADV') }}}: 'Advertise error',
    {{{ cDefine('ESRMNT') }}}: 'Srmount error',
    {{{ cDefine('ECOMM') }}}: 'Communication error on send',
    {{{ cDefine('EPROTO') }}}: 'Protocol error',
    {{{ cDefine('EMULTIHOP') }}}: 'Multihop attempted',
    {{{ cDefine('EDOTDOT') }}}: 'Cross mount point (not really error)',
    {{{ cDefine('EBADMSG') }}}: 'Trying to read unreadable message',
    {{{ cDefine('ENOTUNIQ') }}}: 'Given log. name not unique',
    {{{ cDefine('EBADFD') }}}: 'f.d. invalid for this operation',
    {{{ cDefine('EREMCHG') }}}: 'Remote address changed',
    {{{ cDefine('ELIBACC') }}}: 'Can   access a needed shared lib',
    {{{ cDefine('ELIBBAD') }}}: 'Accessing a corrupted shared lib',
    {{{ cDefine('ELIBSCN') }}}: '.lib section in a.out corrupted',
    {{{ cDefine('ELIBMAX') }}}: 'Attempting to link in too many libs',
    {{{ cDefine('ELIBEXEC') }}}: 'Attempting to exec a shared library',
    {{{ cDefine('ENOSYS') }}}: 'Function not implemented',
    {{{ cDefine('ENOTEMPTY') }}}: 'Directory not empty',
    {{{ cDefine('ENAMETOOLONG') }}}: 'File or path name too long',
    {{{ cDefine('ELOOP') }}}: 'Too many symbolic links',
    {{{ cDefine('EOPNOTSUPP') }}}: 'Operation not supported on transport endpoint',
    {{{ cDefine('EPFNOSUPPORT') }}}: 'Protocol family not supported',
    {{{ cDefine('ECONNRESET') }}}: 'Connection reset by peer',
    {{{ cDefine('ENOBUFS') }}}: 'No buffer space available',
    {{{ cDefine('EAFNOSUPPORT') }}}: 'Address family not supported by protocol family',
    {{{ cDefine('EPROTOTYPE') }}}: 'Protocol wrong type for socket',
    {{{ cDefine('ENOTSOCK') }}}: 'Socket operation on non-socket',
    {{{ cDefine('ENOPROTOOPT') }}}: 'Protocol not available',
    {{{ cDefine('ESHUTDOWN') }}}: 'Can\'t send after socket shutdown',
    {{{ cDefine('ECONNREFUSED') }}}: 'Connection refused',
    {{{ cDefine('EADDRINUSE') }}}: 'Address already in use',
    {{{ cDefine('ECONNABORTED') }}}: 'Connection aborted',
    {{{ cDefine('ENETUNREACH') }}}: 'Network is unreachable',
    {{{ cDefine('ENETDOWN') }}}: 'Network interface is not configured',
    {{{ cDefine('ETIMEDOUT') }}}: 'Connection timed out',
    {{{ cDefine('EHOSTDOWN') }}}: 'Host is down',
    {{{ cDefine('EHOSTUNREACH') }}}: 'Host is unreachable',
    {{{ cDefine('EINPROGRESS') }}}: 'Connection already in progress',
    {{{ cDefine('EALREADY') }}}: 'Socket already connected',
    {{{ cDefine('EDESTADDRREQ') }}}: 'Destination address required',
    {{{ cDefine('EMSGSIZE') }}}: 'Message too long',
    {{{ cDefine('EPROTONOSUPPORT') }}}: 'Unknown protocol',
    {{{ cDefine('ESOCKTNOSUPPORT') }}}: 'Socket type not supported',
    {{{ cDefine('EADDRNOTAVAIL') }}}: 'Address not available',
    {{{ cDefine('ENETRESET') }}}: 'Connection reset by network',
    {{{ cDefine('EISCONN') }}}: 'Socket is already connected',
    {{{ cDefine('ENOTCONN') }}}: 'Socket is not connected',
    {{{ cDefine('ETOOMANYREFS') }}}: 'Too many references',
    {{{ cDefine('EUSERS') }}}: 'Too many users',
    {{{ cDefine('EDQUOT') }}}: 'Quota exceeded',
    {{{ cDefine('ESTALE') }}}: 'Stale file handle',
    {{{ cDefine('ENOTSUP') }}}: 'Not supported',
    {{{ cDefine('ENOMEDIUM') }}}: 'No medium (in tape drive)',
    {{{ cDefine('EILSEQ') }}}: 'Illegal byte sequence',
    {{{ cDefine('EOVERFLOW') }}}: 'Value too large for defined data type',
    {{{ cDefine('ECANCELED') }}}: 'Operation canceled',
    {{{ cDefine('ENOTRECOVERABLE') }}}: 'State not recoverable',
    {{{ cDefine('EOWNERDEAD') }}}: 'Previous owner died',
    {{{ cDefine('ESTRPIPE') }}}: 'Streams pipe error',
  },
#if SUPPORT_ERRNO
  $setErrNo__deps: ['__errno_location'],
  $setErrNo: function(value) {
    {{{makeSetValue("___errno_location()", 0, 'value', 'i32') }}};
    return value;
  },
#else
  $setErrNo: function(value) {
#if ASSERTIONS
    err('failed to set errno from JS');
#endif
    return 0;
  },
#endif

#if !WASM_BACKEND
  // ==========================================================================
  // sched.h (stubs only - no thread support yet!)
  // ==========================================================================
  sched_yield: function() {
    return 0;
  },
#endif

  // ==========================================================================
  // arpa/inet.h
  // ==========================================================================

#if PROXY_POSIX_SOCKETS == 0
  // old ipv4 only functions
  inet_addr__deps: ['_inet_pton4_raw'],
  inet_addr: function(ptr) {
    var addr = __inet_pton4_raw(UTF8ToString(ptr));
    if (addr === null) {
      return -1;
    }
    return addr;
  },

  // ==========================================================================
  // netinet/in.h
  // ==========================================================================

  in6addr_any:
    '{{{ makeStaticAlloc(16) }}}',
  in6addr_loopback:
    '{{{ makeStaticAlloc(16) }}}',

  // ==========================================================================
  // netdb.h
  // ==========================================================================

  _inet_pton4_raw: function(str) {
    var b = str.split('.');
    for (var i = 0; i < 4; i++) {
      var tmp = Number(b[i]);
      if (isNaN(tmp)) return null;
      b[i] = tmp;
    }
    return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0;
  },
  _inet_ntop4_raw: function(addr) {
    return (addr & 0xff) + '.' + ((addr >> 8) & 0xff) + '.' + ((addr >> 16) & 0xff) + '.' + ((addr >> 24) & 0xff)
  },
  _inet_pton6_raw__deps: ['htons', 'ntohs', '$jstoi_q'],
  _inet_pton6_raw: function(str) {
    var words;
    var w, offset, z, i;
    /* http://home.deds.nl/~aeron/regex/ */
    var valid6regx = /^((?=.*::)(?!.*::.+::)(::)?([\dA-F]{1,4}:(:|\b)|){5}|([\dA-F]{1,4}:){6})((([\dA-F]{1,4}((?!\3)::|:\b|$))|(?!\2\3)){2}|(((2[0-4]|1\d|[1-9])?\d|25[0-5])\.?\b){4})$/i
    var parts = [];
    if (!valid6regx.test(str)) {
      return null;
    }
    if (str === "::") {
      return [0, 0, 0, 0, 0, 0, 0, 0];
    }
    // Z placeholder to keep track of zeros when splitting the string on ":"
    if (str.indexOf("::") === 0) {
      str = str.replace("::", "Z:"); // leading zeros case
    } else {
      str = str.replace("::", ":Z:");
    }

    if (str.indexOf(".") > 0) {
      // parse IPv4 embedded stress
      str = str.replace(new RegExp('[.]', 'g'), ":");
      words = str.split(":");
      words[words.length-4] = jstoi_q(words[words.length-4]) + jstoi_q(words[words.length-3])*256;
      words[words.length-3] = jstoi_q(words[words.length-2]) + jstoi_q(words[words.length-1])*256;
      words = words.slice(0, words.length-2);
    } else {
      words = str.split(":");
    }

    offset = 0; z = 0;
    for (w=0; w < words.length; w++) {
      if (typeof words[w] === 'string') {
        if (words[w] === 'Z') {
          // compressed zeros - write appropriate number of zero words
          for (z = 0; z < (8 - words.length+1); z++) {
            parts[w+z] = 0;
          }
          offset = z-1;
        } else {
          // parse hex to field to 16-bit value and write it in network byte-order
          parts[w+offset] = _htons(parseInt(words[w],16));
        }
      } else {
        // parsed IPv4 words
        parts[w+offset] = words[w];
      }
    }
    return [
      (parts[1] << 16) | parts[0],
      (parts[3] << 16) | parts[2],
      (parts[5] << 16) | parts[4],
      (parts[7] << 16) | parts[6]
    ];
  },
  _inet_pton6__deps: ['_inet_pton6_raw'],
  _inet_pton6: function(src, dst) {
    var ints = __inet_pton6_raw(UTF8ToString(src));
    if (ints === null) {
      return 0;
    }
    for (var i = 0; i < 4; i++) {
      {{{ makeSetValue('dst', 'i*4', 'ints[i]', 'i32') }}};
    }
    return 1;
  },
  _inet_ntop6_raw__deps: ['_inet_ntop4_raw'],
  _inet_ntop6_raw: function(ints) {
    //  ref:  http://www.ietf.org/rfc/rfc2373.txt - section 2.5.4
    //  Format for IPv4 compatible and mapped  128-bit IPv6 Addresses
    //  128-bits are split into eight 16-bit words
    //  stored in network byte order (big-endian)
    //  |                80 bits               | 16 |      32 bits        |
    //  +-----------------------------------------------------------------+
    //  |               10 bytes               |  2 |      4 bytes        |
    //  +--------------------------------------+--------------------------+
    //  +               5 words                |  1 |      2 words        |
    //  +--------------------------------------+--------------------------+
    //  |0000..............................0000|0000|    IPv4 ADDRESS     | (compatible)
    //  +--------------------------------------+----+---------------------+
    //  |0000..............................0000|FFFF|    IPv4 ADDRESS     | (mapped)
    //  +--------------------------------------+----+---------------------+
    var str = "";
    var word = 0;
    var longest = 0;
    var lastzero = 0;
    var zstart = 0;
    var len = 0;
    var i = 0;
    var parts = [
      ints[0] & 0xffff,
      (ints[0] >> 16),
      ints[1] & 0xffff,
      (ints[1] >> 16),
      ints[2] & 0xffff,
      (ints[2] >> 16),
      ints[3] & 0xffff,
      (ints[3] >> 16)
    ];

    // Handle IPv4-compatible, IPv4-mapped, loopback and any/unspecified addresses

    var hasipv4 = true;
    var v4part = "";
    // check if the 10 high-order bytes are all zeros (first 5 words)
    for (i = 0; i < 5; i++) {
      if (parts[i] !== 0) { hasipv4 = false; break; }
    }

    if (hasipv4) {
      // low-order 32-bits store an IPv4 address (bytes 13 to 16) (last 2 words)
      v4part = __inet_ntop4_raw(parts[6] | (parts[7] << 16));
      // IPv4-mapped IPv6 address if 16-bit value (bytes 11 and 12) == 0xFFFF (6th word)
      if (parts[5] === -1) {
        str = "::ffff:";
        str += v4part;
        return str;
      }
      // IPv4-compatible IPv6 address if 16-bit value (bytes 11 and 12) == 0x0000 (6th word)
      if (parts[5] === 0) {
        str = "::";
        //special case IPv6 addresses
        if(v4part === "0.0.0.0") v4part = ""; // any/unspecified address
        if(v4part === "0.0.0.1") v4part = "1";// loopback address
        str += v4part;
        return str;
      }
    }

    // Handle all other IPv6 addresses

    // first run to find the longest contiguous zero words
    for (word = 0; word < 8; word++) {
      if (parts[word] === 0) {
        if (word - lastzero > 1) {
          len = 0;
        }
        lastzero = word;
        len++;
      }
      if (len > longest) {
        longest = len;
        zstart = word - longest + 1;
      }
    }

    for (word = 0; word < 8; word++) {
      if (longest > 1) {
        // compress contiguous zeros - to produce "::"
        if (parts[word] === 0 && word >= zstart && word < (zstart + longest) ) {
          if (word === zstart) {
            str += ":";
            if (zstart === 0) str += ":"; //leading zeros case
          }
          continue;
        }
      }
      // converts 16-bit words from big-endian to little-endian before converting to hex string
      str += Number(_ntohs(parts[word] & 0xffff)).toString(16);
      str += word < 7 ? ":" : "";
    }
    return str;
  },

  _read_sockaddr__deps: ['$Sockets', '_inet_ntop4_raw', '_inet_ntop6_raw', 'ntohs'],
  _read_sockaddr: function (sa, salen) {
    // family / port offsets are common to both sockaddr_in and sockaddr_in6
    var family = {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in.sin_family, 'i16') }}};
    var port = _ntohs({{{ makeGetValue('sa', C_STRUCTS.sockaddr_in.sin_port, 'i16', undefined, true) }}});
    var addr;

    switch (family) {
      case {{{ cDefine('AF_INET') }}}:
        if (salen !== {{{ C_STRUCTS.sockaddr_in.__size__ }}}) {
          return { errno: {{{ cDefine('EINVAL') }}} };
        }
        addr = {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in.sin_addr.s_addr, 'i32') }}};
        addr = __inet_ntop4_raw(addr);
        break;
      case {{{ cDefine('AF_INET6') }}}:
        if (salen !== {{{ C_STRUCTS.sockaddr_in6.__size__ }}}) {
          return { errno: {{{ cDefine('EINVAL') }}} };
        }
        addr = [
          {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+0, 'i32') }}},
          {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+4, 'i32') }}},
          {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+8, 'i32') }}},
          {{{ makeGetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+12, 'i32') }}}
        ];
        addr = __inet_ntop6_raw(addr);
        break;
      default:
        return { errno: {{{ cDefine('EAFNOSUPPORT') }}} };
    }

    return { family: family, addr: addr, port: port };
  },
  _write_sockaddr__deps: ['$Sockets', '_inet_pton4_raw', '_inet_pton6_raw'],
  _write_sockaddr: function (sa, family, addr, port) {
    switch (family) {
      case {{{ cDefine('AF_INET') }}}:
        addr = __inet_pton4_raw(addr);
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in.sin_family, 'family', 'i16') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in.sin_addr.s_addr, 'addr', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in.sin_port, '_htons(port)', 'i16') }}};
        break;
      case {{{ cDefine('AF_INET6') }}}:
        addr = __inet_pton6_raw(addr);
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_family, 'family', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+0, 'addr[0]', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+4, 'addr[1]', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+8, 'addr[2]', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_addr.__in6_union.__s6_addr+12, 'addr[3]', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_port, '_htons(port)', 'i16') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_flowinfo, '0', 'i32') }}};
        {{{ makeSetValue('sa', C_STRUCTS.sockaddr_in6.sin6_scope_id, '0', 'i32') }}};
        break;
      default:
        return { errno: {{{ cDefine('EAFNOSUPPORT') }}} };
    }
    // kind of lame, but let's match _read_sockaddr's interface
    return {};
  },

  // We can't actually resolve hostnames in the browser, so instead
  // we're generating fake IP addresses with lookup_name that we can
  // resolve later on with lookup_addr.
  // We do the aliasing in 172.29.*.*, giving us 65536 possibilities.
  $DNS__deps: ['_inet_pton4_raw', '_inet_pton6_raw'],
  $DNS: {
    address_map: {
      id: 1,
      addrs: {},
      names: {}
    },

    lookup_name: function (name) {
      // If the name is already a valid ipv4 / ipv6 address, don't generate a fake one.
      var res = __inet_pton4_raw(name);
      if (res !== null) {
        return name;
      }
      res = __inet_pton6_raw(name);
      if (res !== null) {
        return name;
      }

      // See if this name is already mapped.
      var addr;

      if (DNS.address_map.addrs[name]) {
        addr = DNS.address_map.addrs[name];
      } else {
        var id = DNS.address_map.id++;
        assert(id < 65535, 'exceeded max address mappings of 65535');

        addr = '172.29.' + (id & 0xff) + '.' + (id & 0xff00);

        DNS.address_map.names[addr] = name;
        DNS.address_map.addrs[name] = addr;
      }

      return addr;
    },

    lookup_addr: function (addr) {
      if (DNS.address_map.names[addr]) {
        return DNS.address_map.names[addr];
      }

      return null;
    }
  },

  // note: lots of leaking here!
  gethostbyaddr__deps: ['$DNS', 'gethostbyname', '_inet_ntop4_raw'],
  gethostbyaddr__proxy: 'sync',
  gethostbyaddr__sig: 'iiii',
  gethostbyaddr: function (addr, addrlen, type) {
    if (type !== {{{ cDefine('AF_INET') }}}) {
      setErrNo({{{ cDefine('EAFNOSUPPORT') }}});
      // TODO: set h_errno
      return null;
    }
    addr = {{{ makeGetValue('addr', '0', 'i32') }}}; // addr is in_addr
    var host = __inet_ntop4_raw(addr);
    var lookup = DNS.lookup_addr(host);
    if (lookup) {
      host = lookup;
    }
    var hostp = allocate(intArrayFromString(host), 'i8', ALLOC_STACK);
    return _gethostbyname(hostp);
  },

  gethostbyname__deps: ['$DNS', '_inet_pton4_raw'],
  gethostbyname__proxy: 'sync',
  gethostbyname__sig: 'ii',
  gethostbyname: function(name) {
    name = UTF8ToString(name);

    // generate hostent
    var ret = _malloc({{{ C_STRUCTS.hostent.__size__ }}}); // XXX possibly leaked, as are others here
    var nameBuf = _malloc(name.length+1);
    stringToUTF8(name, nameBuf, name.length+1);
    {{{ makeSetValue('ret', C_STRUCTS.hostent.h_name, 'nameBuf', 'i8*') }}};
    var aliasesBuf = _malloc(4);
    {{{ makeSetValue('aliasesBuf', '0', '0', 'i8*') }}};
    {{{ makeSetValue('ret', C_STRUCTS.hostent.h_aliases, 'aliasesBuf', 'i8**') }}};
    var afinet = {{{ cDefine('AF_INET') }}};
    {{{ makeSetValue('ret', C_STRUCTS.hostent.h_addrtype, 'afinet', 'i32') }}};
    {{{ makeSetValue('ret', C_STRUCTS.hostent.h_length, '4', 'i32') }}};
    var addrListBuf = _malloc(12);
    {{{ makeSetValue('addrListBuf', '0', 'addrListBuf+8', 'i32*') }}};
    {{{ makeSetValue('addrListBuf', '4', '0', 'i32*') }}};
    {{{ makeSetValue('addrListBuf', '8', '__inet_pton4_raw(DNS.lookup_name(name))', 'i32') }}};
    {{{ makeSetValue('ret', C_STRUCTS.hostent.h_addr_list, 'addrListBuf', 'i8**') }}};
    return ret;
  },

  gethostbyname_r__deps: ['gethostbyname'],
  gethostbyname_r__proxy: 'sync',
  gethostbyname_r__sig: 'iiiiiii',
  gethostbyname_r: function(name, ret, buf, buflen, out, err) {
    var data = _gethostbyname(name);
    _memcpy(ret, data, {{{ C_STRUCTS.hostent.__size__ }}});
    _free(data);
    {{{ makeSetValue('err', '0', '0', 'i32') }}};
    {{{ makeSetValue('out', '0', 'ret', '*') }}};
    return 0;
  },

  getaddrinfo__deps: ['$Sockets', '$DNS', '_inet_pton4_raw', '_inet_ntop4_raw', '_inet_pton6_raw', '_inet_ntop6_raw', '_write_sockaddr'],
  getaddrinfo__proxy: 'sync',
  getaddrinfo__sig: 'iiiii',
  getaddrinfo: function(node, service, hint, out) {
    // Note getaddrinfo currently only returns a single addrinfo with ai_next defaulting to NULL. When NULL
    // hints are specified or ai_family set to AF_UNSPEC or ai_socktype or ai_protocol set to 0 then we
    // really should provide a linked list of suitable addrinfo values.
    var addrs = [];
    var canon = null;
    var addr = 0;
    var port = 0;
    var flags = 0;
    var family = {{{ cDefine('AF_UNSPEC') }}};
    var type = 0;
    var proto = 0;
    var ai, last;

    function allocaddrinfo(family, type, proto, canon, addr, port) {
      var sa, salen, ai;
      var res;

      salen = family === {{{ cDefine('AF_INET6') }}} ?
        {{{ C_STRUCTS.sockaddr_in6.__size__ }}} :
        {{{ C_STRUCTS.sockaddr_in.__size__ }}};
      addr = family === {{{ cDefine('AF_INET6') }}} ?
        __inet_ntop6_raw(addr) :
        __inet_ntop4_raw(addr);
      sa = _malloc(salen);
      res = __write_sockaddr(sa, family, addr, port);
      assert(!res.errno);

      ai = _malloc({{{ C_STRUCTS.addrinfo.__size__ }}});
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_family, 'family', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_socktype, 'type', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_protocol, 'proto', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_canonname, 'canon', 'i32') }}};
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addr, 'sa', '*') }}};
      if (family === {{{ cDefine('AF_INET6') }}}) {
        {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addrlen, C_STRUCTS.sockaddr_in6.__size__, 'i32') }}};
      } else {
        {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_addrlen, C_STRUCTS.sockaddr_in.__size__, 'i32') }}};
      }
      {{{ makeSetValue('ai', C_STRUCTS.addrinfo.ai_next, '0', 'i32') }}};

      return ai;
    }

    if (hint) {
      flags = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_flags, 'i32') }}};
      family = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_family, 'i32') }}};
      type = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_socktype, 'i32') }}};
      proto = {{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_protocol, 'i32') }}};
    }
    if (type && !proto) {
      proto = type === {{{ cDefine('SOCK_DGRAM') }}} ? {{{ cDefine('IPPROTO_UDP') }}} : {{{ cDefine('IPPROTO_TCP') }}};
    }
    if (!type && proto) {
      type = proto === {{{ cDefine('IPPROTO_UDP') }}} ? {{{ cDefine('SOCK_DGRAM') }}} : {{{ cDefine('SOCK_STREAM') }}};
    }

    // If type or proto are set to zero in hints we should really be returning multiple addrinfo values, but for
    // now default to a TCP STREAM socket so we can at least return a sensible addrinfo given NULL hints.
    if (proto === 0) {
      proto = {{{ cDefine('IPPROTO_TCP') }}};
    }
    if (type === 0) {
      type = {{{ cDefine('SOCK_STREAM') }}};
    }

    if (!node && !service) {
      return {{{ cDefine('EAI_NONAME') }}};
    }
    if (flags & ~({{{ cDefine('AI_PASSIVE') }}}|{{{ cDefine('AI_CANONNAME') }}}|{{{ cDefine('AI_NUMERICHOST') }}}|
        {{{ cDefine('AI_NUMERICSERV') }}}|{{{ cDefine('AI_V4MAPPED') }}}|{{{ cDefine('AI_ALL') }}}|{{{ cDefine('AI_ADDRCONFIG') }}})) {
      return {{{ cDefine('EAI_BADFLAGS') }}};
    }
    if (hint !== 0 && ({{{ makeGetValue('hint', C_STRUCTS.addrinfo.ai_flags, 'i32') }}} & {{{ cDefine('AI_CANONNAME') }}}) && !node) {
      return {{{ cDefine('EAI_BADFLAGS') }}};
    }
    if (flags & {{{ cDefine('AI_ADDRCONFIG') }}}) {
      // TODO
      return {{{ cDefine('EAI_NONAME') }}};
    }
    if (type !== 0 && type !== {{{ cDefine('SOCK_STREAM') }}} && type !== {{{ cDefine('SOCK_DGRAM') }}}) {
      return {{{ cDefine('EAI_SOCKTYPE') }}};
    }
    if (family !== {{{ cDefine('AF_UNSPEC') }}} && family !== {{{ cDefine('AF_INET') }}} && family !== {{{ cDefine('AF_INET6') }}}) {
      return {{{ cDefine('EAI_FAMILY') }}};
    }

    if (service) {
      service = UTF8ToString(service);
      port = parseInt(service, 10);

      if (isNaN(port)) {
        if (flags & {{{ cDefine('AI_NUMERICSERV') }}}) {
          return {{{ cDefine('EAI_NONAME') }}};
        }
        // TODO support resolving well-known service names from:
        // http://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.txt
        return {{{ cDefine('EAI_SERVICE') }}};
      }
    }

    if (!node) {
      if (family === {{{ cDefine('AF_UNSPEC') }}}) {
        family = {{{ cDefine('AF_INET') }}};
      }
      if ((flags & {{{ cDefine('AI_PASSIVE') }}}) === 0) {
        if (family === {{{ cDefine('AF_INET') }}}) {
          addr = _htonl({{{ cDefine('INADDR_LOOPBACK') }}});
        } else {
          addr = [0, 0, 0, 1];
        }
      }
      ai = allocaddrinfo(family, type, proto, null, addr, port);
      {{{ makeSetValue('out', '0', 'ai', '*') }}};
      return 0;
    }

    //
    // try as a numeric address
    //
    node = UTF8ToString(node);
    addr = __inet_pton4_raw(node);
    if (addr !== null) {
      // incoming node is a valid ipv4 address
      if (family === {{{ cDefine('AF_UNSPEC') }}} || family === {{{ cDefine('AF_INET') }}}) {
        family = {{{ cDefine('AF_INET') }}};
      }
      else if (family === {{{ cDefine('AF_INET6') }}} && (flags & {{{ cDefine('AI_V4MAPPED') }}})) {
        addr = [0, 0, _htonl(0xffff), addr];
        family = {{{ cDefine('AF_INET6') }}};
      } else {
        return {{{ cDefine('EAI_NONAME') }}};
      }
    } else {
      addr = __inet_pton6_raw(node);
      if (addr !== null) {
        // incoming node is a valid ipv6 address
        if (family === {{{ cDefine('AF_UNSPEC') }}} || family === {{{ cDefine('AF_INET6') }}}) {
          family = {{{ cDefine('AF_INET6') }}};
        } else {
          return {{{ cDefine('EAI_NONAME') }}};
        }
      }
    }
    if (addr != null) {
      ai = allocaddrinfo(family, type, proto, node, addr, port);
      {{{ makeSetValue('out', '0', 'ai', '*') }}};
      return 0;
    }
    if (flags & {{{ cDefine('AI_NUMERICHOST') }}}) {
      return {{{ cDefine('EAI_NONAME') }}};
    }

    //
    // try as a hostname
    //
    // resolve the hostname to a temporary fake address
    node = DNS.lookup_name(node);
    addr = __inet_pton4_raw(node);
    if (family === {{{ cDefine('AF_UNSPEC') }}}) {
      family = {{{ cDefine('AF_INET') }}};
    } else if (family === {{{ cDefine('AF_INET6') }}}) {
      addr = [0, 0, _htonl(0xffff), addr];
    }
    ai = allocaddrinfo(family, type, proto, null, addr, port);
    {{{ makeSetValue('out', '0', 'ai', '*') }}};
    return 0;
  },

  getnameinfo__deps: ['$Sockets', '$DNS', '_read_sockaddr'],
  getnameinfo: function (sa, salen, node, nodelen, serv, servlen, flags) {
    var info = __read_sockaddr(sa, salen);
    if (info.errno) {
      return {{{ cDefine('EAI_FAMILY') }}};
    }
    var port = info.port;
    var addr = info.addr;

    var overflowed = false;

    if (node && nodelen) {
      var lookup;
      if ((flags & {{{ cDefine('NI_NUMERICHOST') }}}) || !(lookup = DNS.lookup_addr(addr))) {
        if (flags & {{{ cDefine('NI_NAMEREQD') }}}) {
          return {{{ cDefine('EAI_NONAME') }}};
        }
      } else {
        addr = lookup;
      }
      var numBytesWrittenExclNull = stringToUTF8(addr, node, nodelen);

      if (numBytesWrittenExclNull+1 >= nodelen) {
        overflowed = true;
      }
    }

    if (serv && servlen) {
      port = '' + port;
      var numBytesWrittenExclNull = stringToUTF8(port, serv, servlen);

      if (numBytesWrittenExclNull+1 >= servlen) {
        overflowed = true;
      }
    }

    if (overflowed) {
      // Note: even when we overflow, getnameinfo() is specced to write out the truncated results.
      return {{{ cDefine('EAI_OVERFLOW') }}};
    }

    return 0;
  },
  // Can't use a literal for $GAI_ERRNO_MESSAGES as was done for $ERRNO_MESSAGES as the keys (e.g. EAI_BADFLAGS)
  // are actually negative numbers and you can't have expressions as keys in JavaScript literals.
  $GAI_ERRNO_MESSAGES: {},

  gai_strerror__deps: ['$GAI_ERRNO_MESSAGES'
#if MINIMAL_RUNTIME
    , '$writeAsciiToMemory'
#endif
  ],
  gai_strerror: function(val) {
    var buflen = 256;

    // On first call to gai_strerror we initialise the buffer and populate the error messages.
    if (!_gai_strerror.buffer) {
        _gai_strerror.buffer = _malloc(buflen);

        GAI_ERRNO_MESSAGES['0'] = 'Success';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_BADFLAGS') }}}] = 'Invalid value for \'ai_flags\' field';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_NONAME') }}}] = 'NAME or SERVICE is unknown';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_AGAIN') }}}] = 'Temporary failure in name resolution';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_FAIL') }}}] = 'Non-recoverable failure in name res';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_FAMILY') }}}] = '\'ai_family\' not supported';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_SOCKTYPE') }}}] = '\'ai_socktype\' not supported';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_SERVICE') }}}] = 'SERVICE not supported for \'ai_socktype\'';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_MEMORY') }}}] = 'Memory allocation failure';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_SYSTEM') }}}] = 'System error returned in \'errno\'';
        GAI_ERRNO_MESSAGES['' + {{{ cDefine('EAI_OVERFLOW') }}}] = 'Argument buffer overflow';
    }

    var msg = 'Unknown error';

    if (val in GAI_ERRNO_MESSAGES) {
      if (GAI_ERRNO_MESSAGES[val].length > buflen - 1) {
        msg = 'Message too long'; // EMSGSIZE message. This should never occur given the GAI_ERRNO_MESSAGES above.
      } else {
        msg = GAI_ERRNO_MESSAGES[val];
      }
    }

    writeAsciiToMemory(msg, _gai_strerror.buffer);
    return _gai_strerror.buffer;
  },

  // Implement netdb.h protocol entry (getprotoent, getprotobyname, getprotobynumber, setprotoent, endprotoent)
  // http://pubs.opengroup.org/onlinepubs/9699919799/functions/getprotobyname.html
  // The Protocols object holds our 'fake' protocols 'database'.
  $Protocols: {
    list: [],
    map: {}
  },
  setprotoent__deps: ['$Protocols'
#if MINIMAL_RUNTIME
    , '$writeAsciiToMemory'
#endif
  ],
  setprotoent: function(stayopen) {
    // void setprotoent(int stayopen);

    // Allocate and populate a protoent structure given a name, protocol number and array of aliases
    function allocprotoent(name, proto, aliases) {
      // write name into buffer
      var nameBuf = _malloc(name.length + 1);
      writeAsciiToMemory(name, nameBuf);

      // write aliases into buffer
      var j = 0;
      var length = aliases.length;
      var aliasListBuf = _malloc((length + 1) * 4); // Use length + 1 so we have space for the terminating NULL ptr.

      for (var i = 0; i < length; i++, j += 4) {
        var alias = aliases[i];
        var aliasBuf = _malloc(alias.length + 1);
        writeAsciiToMemory(alias, aliasBuf);
        {{{ makeSetValue('aliasListBuf', 'j', 'aliasBuf', 'i8*') }}};
      }
      {{{ makeSetValue('aliasListBuf', 'j', '0', 'i8*') }}}; // Terminating NULL pointer.

      // generate protoent
      var pe = _malloc({{{ C_STRUCTS.protoent.__size__ }}});
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_name, 'nameBuf', 'i8*') }}};
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_aliases, 'aliasListBuf', 'i8**') }}};
      {{{ makeSetValue('pe', C_STRUCTS.protoent.p_proto, 'proto', 'i32') }}};
      return pe;
    };

    // Populate the protocol 'database'. The entries are limited to tcp and udp, though it is fairly trivial
    // to add extra entries from /etc/protocols if desired - though not sure if that'd actually be useful.
    var list = Protocols.list;
    var map  = Protocols.map;
    if (list.length === 0) {
        var entry = allocprotoent('tcp', 6, ['TCP']);
        list.push(entry);
        map['tcp'] = map['6'] = entry;
        entry = allocprotoent('udp', 17, ['UDP']);
        list.push(entry);
        map['udp'] = map['17'] = entry;
    }

    _setprotoent.index = 0;
  },

  endprotoent: function() {
    // void endprotoent(void);
    // We're not using a real protocol database so we don't do a real close.
  },

  getprotoent__deps: ['setprotoent', '$Protocols'],
  getprotoent: function(number) {
    // struct protoent *getprotoent(void);
    // reads the  next  entry  from  the  protocols 'database' or return NULL if 'eof'
    if (_setprotoent.index === Protocols.list.length) {
      return 0;
    } else {
      var result = Protocols.list[_setprotoent.index++];
      return result;
    }
  },

  getprotobyname__deps: ['setprotoent', '$Protocols'],
  getprotobyname: function(name) {
    // struct protoent *getprotobyname(const char *);
    name = UTF8ToString(name);
    _setprotoent(true);
    var result = Protocols.map[name];
    return result;
  },

  getprotobynumber__deps: ['setprotoent', '$Protocols'],
  getprotobynumber: function(number) {
    // struct protoent *getprotobynumber(int proto);
    _setprotoent(true);
    var result = Protocols.map[number];
    return result;
  },

  // ==========================================================================
  // sockets. Note that the implementation assumes all sockets are always
  // nonblocking
  // ==========================================================================
#if SOCKET_WEBRTC
  $Sockets__deps: ['$setErrNo',
    function() { return 'var SocketIO = ' + read('socket.io.js') + ';\n' },
    function() { return 'var Peer = ' + read('wrtcp.js') + ';\n' }],
#else
  $Sockets__deps: ['$setErrNo'],
#endif
  $Sockets: {
    BUFFER_SIZE: 10*1024, // initial size
    MAX_BUFFER_SIZE: 10*1024*1024, // maximum size we will grow the buffer

    nextFd: 1,
    fds: {},
    nextport: 1,
    maxport: 65535,
    peer: null,
    connections: {},
    portmap: {},
    localAddr: 0xfe00000a, // Local address is always 10.0.0.254
    addrPool: [            0x0200000a, 0x0300000a, 0x0400000a, 0x0500000a,
               0x0600000a, 0x0700000a, 0x0800000a, 0x0900000a, 0x0a00000a,
               0x0b00000a, 0x0c00000a, 0x0d00000a, 0x0e00000a] /* 0x0100000a is reserved */
  },

#endif // PROXY_POSIX_SOCKETS == 0

  // pwd.h

  getpwnam: function() { throw 'getpwnam: TODO' },
  getpwnam_r: function() { throw 'getpwnam_r: TODO' },
  getpwuid: function() { throw 'getpwuid: TODO' },
  getpwuid_r: function() { throw 'getpwuid_r: TODO' },
  setpwent: function() { throw 'setpwent: TODO' },
  getpwent: function() { throw 'getpwent: TODO' },
  endpwent: function() { throw 'endpwent: TODO' },

  // grp.h

  getgrgid: function() { throw 'getgrgid: TODO' },
  getgrgid_r: function() { throw 'getgrgid_r: TODO' },
  getgrnam: function() { throw 'getgrnam: TODO' },
  getgrnam_r: function() { throw 'getgrnam_r: TODO' },
  getgrent: function() { throw 'getgrent: TODO' },
  endgrent: function() { throw 'endgrent: TODO' },
  setgrent: function() { throw 'setgrent: TODO' },

  // ==========================================================================
  // emscripten.h
  // ==========================================================================

  emscripten_run_script: function(ptr) {
    {{{ makeEval('eval(UTF8ToString(ptr));') }}}
  },

  emscripten_run_script_int__docs: '/** @suppress{checkTypes} */',
  emscripten_run_script_int: function(ptr) {
    {{{ makeEval('return eval(UTF8ToString(ptr))|0;') }}}
  },

  emscripten_run_script_string: function(ptr) {
    {{{ makeEval("var s = eval(UTF8ToString(ptr));") }}}
    if (s == null) {
      return 0;
    }
    s += '';
    var me = _emscripten_run_script_string;
    var len = lengthBytesUTF8(s);
    if (!me.bufferSize || me.bufferSize < len+1) {
      if (me.bufferSize) _free(me.buffer);
      me.bufferSize = len+1;
      me.buffer = _malloc(me.bufferSize);
    }
    stringToUTF8(s, me.buffer, me.bufferSize);
    return me.buffer;
  },

  emscripten_random: function() {
    return Math.random();
  },

  emscripten_get_now__import: true,
  emscripten_get_now: ';' +
#if ENVIRONMENT_MAY_BE_NODE
                               "if (ENVIRONMENT_IS_NODE) {\n" +
                               "  _emscripten_get_now = function() {\n" +
                               "    var t = process['hrtime']();\n" +
                               "    return t[0] * 1e3 + t[1] / 1e6;\n" +
                               "  };\n" +
                               "} else " +
#endif
#if USE_PTHREADS
// Pthreads need their clocks synchronized to the execution of the main thread, so give them a special form of the function.
                               "if (ENVIRONMENT_IS_PTHREAD) {\n" +
                               "  _emscripten_get_now = function() { return performance.now() - Module['__performance_now_clock_drift']; };\n" +
                               "} else " +
#endif
#if ENVIRONMENT_MAY_BE_SHELL
                               "if (typeof dateNow !== 'undefined') {\n" +
                               "  _emscripten_get_now = dateNow;\n" +
                               "} else " +
#endif
#if MIN_IE_VERSION <= 9 || MIN_FIREFOX_VERSION <= 14 || MIN_CHROME_VERSION <= 23 || MIN_SAFARI_VERSION <= 80400 // https://caniuse.com/#feat=high-resolution-time
                               "if (typeof performance !== 'undefined' && performance.now) {\n" +
                               "  _emscripten_get_now = function() { return performance.now(); }\n" +
                               "} else {\n" +
                               "  _emscripten_get_now = Date.now;\n" +
                               "}",
#else
                               // Modern environment where performance.now() is supported:
                               // N.B. a shorter form "_emscripten_get_now = return performance.now;" is unfortunately not allowed even in current browsers (e.g. FF Nightly 75).
                               "_emscripten_get_now = function() { return performance.now(); }\n",
#endif

  emscripten_get_now_res: function() { // return resolution of get_now, in nanoseconds
#if ENVIRONMENT_MAY_BE_NODE
    if (ENVIRONMENT_IS_NODE) {
      return 1; // nanoseconds
    } else
#endif
#if ENVIRONMENT_MAY_BE_SHELL
    if (typeof dateNow !== 'undefined') {
      return 1000; // microseconds (1/1000 of a millisecond)
    } else
#endif
#if MIN_IE_VERSION <= 9 || MIN_FIREFOX_VERSION <= 14 || MIN_CHROME_VERSION <= 23 || MIN_SAFARI_VERSION <= 80400 // https://caniuse.com/#feat=high-resolution-time
    if (typeof performance === 'object' && performance && typeof performance['now'] === 'function') {
      return 1000; // microseconds (1/1000 of a millisecond)
    } else {
      return 1000*1000; // milliseconds
    }
#else
    // Modern environment where performance.now() is supported:
    return 1000; // microseconds (1/1000 of a millisecond)
#endif
  },

  // Represents whether emscripten_get_now is guaranteed monotonic; the Date.now
  // implementation is not :(
#if MIN_IE_VERSION <= 9 || MIN_FIREFOX_VERSION <= 14 || MIN_CHROME_VERSION <= 23 || MIN_SAFARI_VERSION <= 80400 // https://caniuse.com/#feat=high-resolution-time
  emscripten_get_now_is_monotonic: `
     ((typeof performance === 'object' && performance && typeof performance['now'] === 'function')
#if ENVIRONMENT_MAY_BE_NODE
      || ENVIRONMENT_IS_NODE
#endif
#if ENVIRONMENT_MAY_BE_SHELL
      || (typeof dateNow !== 'undefined')
#endif
    );`,
#else
  // Modern environment where performance.now() is supported: (rely on minifier to return true unconditionally from this function)
  emscripten_get_now_is_monotonic: 'true;',
#endif

#if MINIMAL_RUNTIME
  $warnOnce: function(text) {
    if (!warnOnce.shown) warnOnce.shown = {};
    if (!warnOnce.shown[text]) {
      warnOnce.shown[text] = 1;
      err(text);
    }
  },
#endif

  // Returns [parentFuncArguments, functionName, paramListName]
  _emscripten_traverse_stack: function(args) {
    if (!args || !args.callee || !args.callee.name) {
      return [null, '', ''];
    }

    var funstr = args.callee.toString();
    var funcname = args.callee.name;
    var str = '(';
    var first = true;
    for (var i in args) {
      var a = args[i];
      if (!first) {
        str += ", ";
      }
      first = false;
      if (typeof a === 'number' || typeof a === 'string') {
        str += a;
      } else {
        str += '(' + typeof a + ')';
      }
    }
    str += ')';
    var caller = args.callee.caller;
    args = caller ? caller.arguments : [];
    if (first)
      str = '';
    return [args, funcname, str];
  },

  emscripten_get_callstack_js__deps: ['_emscripten_traverse_stack', '$jsStackTrace', '$demangle'
#if MINIMAL_RUNTIME
    , '$warnOnce'
#endif
  ],
  emscripten_get_callstack_js__docs: '/** @param {number=} flags */',
  emscripten_get_callstack_js: function(flags) {
    var callstack = jsStackTrace();

    // Find the symbols in the callstack that corresponds to the functions that report callstack information, and remove everything up to these from the output.
    var iThisFunc = callstack.lastIndexOf('_emscripten_log');
    var iThisFunc2 = callstack.lastIndexOf('_emscripten_get_callstack');
    var iNextLine = callstack.indexOf('\n', Math.max(iThisFunc, iThisFunc2))+1;
    callstack = callstack.slice(iNextLine);

    // If user requested to see the original source stack, but no source map information is available, just fall back to showing the JS stack.
    if (flags & 8/*EM_LOG_C_STACK*/ && typeof emscripten_source_map === 'undefined') {
      warnOnce('Source map information is not available, emscripten_log with EM_LOG_C_STACK will be ignored. Build with "--pre-js $EMSCRIPTEN/src/emscripten-source-map.min.js" linker flag to add source map loading to code.');
      flags ^= 8/*EM_LOG_C_STACK*/;
      flags |= 16/*EM_LOG_JS_STACK*/;
    }

    var stack_args = null;
    if (flags & 128 /*EM_LOG_FUNC_PARAMS*/) {
      // To get the actual parameters to the functions, traverse the stack via the unfortunately deprecated 'arguments.callee' method, if it works:
      stack_args = __emscripten_traverse_stack(arguments);
      while (stack_args[1].indexOf('_emscripten_') >= 0)
        stack_args = __emscripten_traverse_stack(stack_args[0]);
    }

    // Process all lines:
    var lines = callstack.split('\n');
    callstack = '';
    var newFirefoxRe = new RegExp('\\s*(.*?)@(.*?):([0-9]+):([0-9]+)'); // New FF30 with column info: extract components of form '       Object._main@http://server.com:4324:12'
    var firefoxRe = new RegExp('\\s*(.*?)@(.*):(.*)(:(.*))?'); // Old FF without column info: extract components of form '       Object._main@http://server.com:4324'
    var chromeRe = new RegExp('\\s*at (.*?) \\\((.*):(.*):(.*)\\\)'); // Extract components of form '    at Object._main (http://server.com/file.html:4324:12)'

    for (var l in lines) {
      var line = lines[l];

      var jsSymbolName = '';
      var file = '';
      var lineno = 0;
      var column = 0;

      var parts = chromeRe.exec(line);
      if (parts && parts.length == 5) {
        jsSymbolName = parts[1];
        file = parts[2];
        lineno = parts[3];
        column = parts[4];
      } else {
        parts = newFirefoxRe.exec(line);
        if (!parts) parts = firefoxRe.exec(line);
        if (parts && parts.length >= 4) {
          jsSymbolName = parts[1];
          file = parts[2];
          lineno = parts[3];
          column = parts[4]|0; // Old Firefox doesn't carry column information, but in new FF30, it is present. See https://bugzilla.mozilla.org/show_bug.cgi?id=762556
        } else {
          // Was not able to extract this line for demangling/sourcemapping purposes. Output it as-is.
          callstack += line + '\n';
          continue;
        }
      }

      // Try to demangle the symbol, but fall back to showing the original JS symbol name if not available.
      var cSymbolName = (flags & 32/*EM_LOG_DEMANGLE*/) ? demangle(jsSymbolName) : jsSymbolName;
      if (!cSymbolName) {
        cSymbolName = jsSymbolName;
      }

      var haveSourceMap = false;

      if (flags & 8/*EM_LOG_C_STACK*/) {
        var orig = emscripten_source_map.originalPositionFor({line: lineno, column: column});
        haveSourceMap = (orig && orig.source);
        if (haveSourceMap) {
          if (flags & 64/*EM_LOG_NO_PATHS*/) {
            orig.source = orig.source.substring(orig.source.replace(/\\/g, "/").lastIndexOf('/')+1);
          }
          callstack += '    at ' + cSymbolName + ' (' + orig.source + ':' + orig.line + ':' + orig.column + ')\n';
        }
      }
      if ((flags & 16/*EM_LOG_JS_STACK*/) || !haveSourceMap) {
        if (flags & 64/*EM_LOG_NO_PATHS*/) {
          file = file.substring(file.replace(/\\/g, "/").lastIndexOf('/')+1);
        }
        callstack += (haveSourceMap ? ('     = '+jsSymbolName) : ('    at '+cSymbolName)) + ' (' + file + ':' + lineno + ':' + column + ')\n';
      }

      // If we are still keeping track with the callstack by traversing via 'arguments.callee', print the function parameters as well.
      if (flags & 128 /*EM_LOG_FUNC_PARAMS*/ && stack_args[0]) {
        if (stack_args[1] == jsSymbolName && stack_args[2].length > 0) {
          callstack = callstack.replace(/\s+$/, '');
          callstack += ' with values: ' + stack_args[1] + stack_args[2] + '\n';
        }
        stack_args = __emscripten_traverse_stack(stack_args[0]);
      }
    }
    // Trim extra whitespace at the end of the output.
    callstack = callstack.replace(/\s+$/, '');
    return callstack;
  },

  emscripten_get_callstack__deps: ['emscripten_get_callstack_js'],
  emscripten_get_callstack: function(flags, str, maxbytes) {
    var callstack = _emscripten_get_callstack_js(flags);
    // User can query the required amount of bytes to hold the callstack.
    if (!str || maxbytes <= 0) {
      return lengthBytesUTF8(callstack)+1;
    }
    // Output callstack string as C string to HEAP.
    var bytesWrittenExcludingNull = stringToUTF8(callstack, str, maxbytes);

    // Return number of bytes written, including null.
    return bytesWrittenExcludingNull+1;
  },

  emscripten_log_js__deps: ['emscripten_get_callstack_js'],
  emscripten_log_js: function(flags, str) {
    if (flags & 24/*EM_LOG_C_STACK | EM_LOG_JS_STACK*/) {
      str = str.replace(/\s+$/, ''); // Ensure the message and the callstack are joined cleanly with exactly one newline.
      str += (str.length > 0 ? '\n' : '') + _emscripten_get_callstack_js(flags);
    }

    if (flags & 1 /*EM_LOG_CONSOLE*/) {
      if (flags & 4 /*EM_LOG_ERROR*/) {
        console.error(str);
      } else if (flags & 2 /*EM_LOG_WARN*/) {
        console.warn(str);
      } else if (flags & 512 /*EM_LOG_INFO*/) {
        console.info(str);
      } else if (flags & 256 /*EM_LOG_DEBUG*/) {
        console.debug(str);
      } else {
        console.log(str);
      }
    } else if (flags & 6 /*EM_LOG_ERROR|EM_LOG_WARN*/) {
      err(str);
    } else {
      out(str);
    }
  },

  emscripten_log__deps: ['$formatString', 'emscripten_log_js'],
  emscripten_log: function(flags, format, varargs) {
    var str = '';
    var result = formatString(format, varargs);
    for (var i = 0 ; i < result.length; ++i) {
      str += String.fromCharCode(result[i]);
    }
    _emscripten_log_js(flags, str);
  },

  emscripten_get_compiler_setting: function(name) {
    name = UTF8ToString(name);

    var ret = getCompilerSetting(name);
    if (typeof ret === 'number') return ret;

    if (!_emscripten_get_compiler_setting.cache) _emscripten_get_compiler_setting.cache = {};
    var cache = _emscripten_get_compiler_setting.cache;
    var fullname = name + '__str';
    var fullret = cache[fullname];
    if (fullret) return fullret;
    return cache[fullname] = allocate(intArrayFromString(ret + ''), 'i8', ALLOC_NORMAL);
  },

  emscripten_has_asyncify: function() {
    return {{{ ASYNCIFY || EMTERPRETIFY_ASYNC }}};
  },

  emscripten_debugger: function() {
    debugger;
  },

  emscripten_print_double: function(x, to, max) {
    var str = x + '';
    if (to) return stringToUTF8(str, to, max);
    else return lengthBytesUTF8(str);
  },

  // Generates a representation of the program counter from a line of stack trace.
  // The exact return value depends in whether we are running WASM or JS, and whether
  // the engine supports offsets into WASM. See the function body for details.
  emscripten_generate_pc: function(frame) {
#if !USE_OFFSET_CONVERTER
    abort('Cannot use emscripten_generate_pc (needed by __builtin_return_address) without -s USE_OFFSET_CONVERTER');
#endif
    var match;

    if (match = /\bwasm-function\[\d+\]:(0x[0-9a-f]+)/.exec(frame)) {
      // some engines give the binary offset directly, so we use that as return address
      return +match[1];
    } else if (match = /\bwasm-function\[(\d+)\]:(\d+)/.exec(frame)) {
      // other engines only give function index and offset in the function,
      // so we try using the offset converter. If that doesn't work,
      // we pack index and offset into a "return address"
      return wasmOffsetConverter.convert(+match[1], +match[2]);
    } else if (match = /:(\d+):\d+(?:\)|$)/.exec(frame)) {
      // if we are in js, we can use the js line number as the "return address"
      // this should work for wasm2js and fastcomp
      // we tag the high bit to distinguish this from wasm addresses
      return 0x80000000 | +match[1];
    } else {
      // return 0 if we can't find any
      return 0;
    }
  },

  // Returns a representation of a call site of the caller of this function, in a manner
  // similar to __builtin_return_address. If level is 0, we return the call site of the
  // caller of this function.
  emscripten_return_address__deps: ['emscripten_generate_pc'],
  emscripten_return_address: function(level) {
    var callstack = new Error().stack.split('\n');
    if (callstack[0] == 'Error') {
      callstack.shift();
    }
    // skip this function and the caller to get caller's return address
    return _emscripten_generate_pc(callstack[level + 2]);
  },

  $UNWIND_CACHE: {},

  // This function pulls the JavaScript stack trace and updates UNWIND_CACHE so that
  // our representation of the program counter is mapped to the line of the stack trace
  // for every line in the stack trace. This allows emscripten_pc_get_* to lookup the
  // line of the stack trace from the PC and return meaningful information.
  //
  // Additionally, it saves a copy of the entire stack trace and the return address of
  // the caller. This is because there are two common forms of a stack trace.
  // The first form starts the stack trace at the caller of the function requesting a stack
  // trace. In this case, the function can simply walk down the stack from the return address
  // using emscripten_return_address with increasing values for level.
  // The second form starts the stack trace at the current function. This requires a helper
  // function to get the program counter. This helper function will return the return address.
  // This is the program counter at the call site. But there is a problem: when calling into
  // code that performs stack unwinding, the program counter has changed since execution
  // continued from calling the helper function. So we can't just walk down the stack and expect
  // to see.the PC value we got. By caching the call stack, we can call emscripten_stack_unwind
  // with the PC value and use that to unwind the cached stack. Naturally, the PC helper function
  // will have to call emscripten_stack_snapshot to cache the stack. We also return the return
  // address of the caller so the PC helper function does not need to call
  // emscripten_return_address, saving a lot of time.
  //
  // One might expect that a sensible solution is to call the stack unwinder and explicitly tell it
  // how many functions to skip from the stack. However, existing libraries do not work this way.
  // For example, compiler-rt's sanitizer_common library has macros GET_CALLER_PC_BP_SP and
  // GET_CURRENT_PC_BP_SP, which obtains the PC value for the two common cases stated above,
  // respectively. Then, it passes the PC, BP, SP values along until some other function uses them
  // to unwind. On standard machines, the stack can be unwound by treating BP as a linked list.
  // This makes PC unnecessary to walk the stack, since walking is done with BP, which remains
  // valid until the function returns. But on Emscripten, BP does not exist, at least in
  // JavaScript frames, so we have to rely on PC values. Therefore, we must be able to unwind from
  // a PC value that may no longer be on the execution stack, and so we are forced to cache the
  // entire call stack.
  emscripten_stack_snapshot__deps: ['emscripten_generate_pc', '$UNWIND_CACHE', '_emscripten_save_in_unwind_cache'],
  emscripten_stack_snapshot: function () {
    var callstack = new Error().stack.split('\n');
    if (callstack[0] == 'Error') {
      callstack.shift();
    }
    __emscripten_save_in_unwind_cache(callstack);

    // Caches the stack snapshot so that emscripten_stack_unwind_buffer() can unwind from this spot.
    UNWIND_CACHE.last_addr = _emscripten_generate_pc(callstack[2]);
    UNWIND_CACHE.last_stack = callstack;
    return UNWIND_CACHE.last_addr;
  },

  _emscripten_save_in_unwind_cache__deps: ['$UNWIND_CACHE', 'emscripten_generate_pc'],
  _emscripten_save_in_unwind_cache: function (callstack) {
    callstack.forEach(function (frame) {
      var pc = _emscripten_generate_pc(frame);
      if (pc) {
        UNWIND_CACHE[pc] = frame;
      }
    });
  },

  // Unwinds the stack from a cached PC value. See emscripten_stack_snapshot for how this is used.
  // addr must be the return address of the last call to emscripten_stack_snapshot, or this
  // function will instead use the current call stack.
  emscripten_stack_unwind_buffer__deps: ['$UNWIND_CACHE', '_emscripten_save_in_unwind_cache', 'emscripten_generate_pc'],
  emscripten_stack_unwind_buffer: function (addr, buffer, count) {
    var stack;
    if (UNWIND_CACHE.last_addr == addr) {
      stack = UNWIND_CACHE.last_stack;
    } else {
      stack = new Error().stack.split('\n');
      if (stack[0] == 'Error') {
        stack.shift();
      }
      __emscripten_save_in_unwind_cache(stack);
    }

    var offset = 2;
    while (stack[offset] && _emscripten_generate_pc(stack[offset]) != addr) {
      ++offset;
    }

    for (var i = 0; i < count && stack[i+offset]; ++i) {
      {{{ makeSetValue('buffer', 'i*4', '_emscripten_generate_pc(stack[i + offset])', 'i32', 0, true) }}};
    }
    return i;
  },

  // Look up the function name from our stack frame cache with our PC representation.
  emscripten_pc_get_function__deps: ['$UNWIND_CACHE', 'emscripten_with_builtin_malloc'
#if MINIMAL_RUNTIME
    , '$allocateUTF8'
#endif
  ],
  emscripten_pc_get_function: function (pc) {
#if !USE_OFFSET_CONVERTER
    abort('Cannot use emscripten_pc_get_function without -s USE_OFFSET_CONVERTER');
#endif
    var name;
    if (pc & 0x80000000) {
      // If this is a JavaScript function, try looking it up in the unwind cache.
      var frame = UNWIND_CACHE[pc];
      if (!frame) return 0;

      var match;
      if (match = /^\s+at (.*) \(.*\)$/.exec(frame)) {
        name = match[1];
      } else if (match = /^(.+?)@/.exec(frame)) {
        name = match[1];
      } else {
        return 0;
      }
    } else {
      name = wasmOffsetConverter.getName(pc);
    }
    _emscripten_with_builtin_malloc(function () {
      if (_emscripten_pc_get_function.ret) _free(_emscripten_pc_get_function.ret);
      _emscripten_pc_get_function.ret = allocateUTF8(name);
    });
    return _emscripten_pc_get_function.ret;
  },

  emscripten_pc_get_source_js__deps: ['$UNWIND_CACHE', 'emscripten_generate_pc'],
  emscripten_pc_get_source_js: function (pc) {
    if (UNWIND_CACHE.last_get_source_pc == pc) return UNWIND_CACHE.last_source;

    var match;
    var source;
#if LOAD_SOURCE_MAP
    if (wasmSourceMap) {
      var info = wasmSourceMap.lookup(pc);
      if (info) {
        source = {file: info.source, line: info.line, column: info.column};
      }
    }
#endif

    if (!source) {
      var frame = UNWIND_CACHE[pc];
      if (!frame) return null;
      // Example: at callMain (a.out.js:6335:22)
      if (match = /\((.*):(\d+):(\d+)\)$/.exec(frame)) {
        source = {file: match[1], line: match[2], column: match[3]};
      // Example: main@a.out.js:1337:42
      } else if (match = /@(.*):(\d+):(\d+)/.exec(frame)) {
        source = {file: match[1], line: match[2], column: match[3]};
      }
    }
    UNWIND_CACHE.last_get_source_pc = pc;
    UNWIND_CACHE.last_source = source;
    return source;
  },

  // Look up the file name from our stack frame cache with our PC representation.
  emscripten_pc_get_file__deps: ['emscripten_pc_get_source_js', 'emscripten_with_builtin_malloc',
#if MINIMAL_RUNTIME
    '$allocateUTF8',
#endif
  ],
  emscripten_pc_get_file: function (pc) {
    var result = _emscripten_pc_get_source_js(pc);
    if (!result) return 0;

    _emscripten_with_builtin_malloc(function () {
      if (_emscripten_pc_get_file.ret) _free(_emscripten_pc_get_file.ret);
      _emscripten_pc_get_file.ret = allocateUTF8(result.file);
    });
    return _emscripten_pc_get_file.ret;
  },

  // Look up the line number from our stack frame cache with our PC representation.
  emscripten_pc_get_line__deps: ['emscripten_pc_get_source_js'],
  emscripten_pc_get_line: function (pc) {
    var result = _emscripten_pc_get_source_js(pc);
    return result ? result.line : 0;
  },

  // Look up the column number from our stack frame cache with our PC representation.
  emscripten_pc_get_column__deps: ['emscripten_pc_get_source_js'],
  emscripten_pc_get_column: function (pc) {
    var result = _emscripten_pc_get_source_js(pc);
    return result ? result.column || 0 : 0;
  },

  emscripten_get_module_name: function(buf, length) {
#if MINIMAL_RUNTIME
    return stringToUTF8('{{{ TARGET_BASENAME }}}.wasm', buf, length);
#else
    return stringToUTF8(wasmBinaryFile, buf, length);
#endif
  },

  emscripten_with_builtin_malloc__deps: ['emscripten_builtin_malloc', 'emscripten_builtin_free', 'emscripten_builtin_memalign'],
  emscripten_with_builtin_malloc__docs: '/** @suppress{checkTypes} */',
  emscripten_with_builtin_malloc: function (func) {
    var prev_malloc = typeof _malloc !== 'undefined' ? _malloc : undefined;
    var prev_memalign = typeof _memalign !== 'undefined' ? _memalign : undefined;
    var prev_free = typeof _free !== 'undefined' ? _free : undefined;
    _malloc = _emscripten_builtin_malloc;
    _memalign = _emscripten_builtin_memalign;
    _free = _emscripten_builtin_free;
    try {
      return func();
    } finally {
      _malloc = prev_malloc;
      _memalign = prev_memalign;
      _free = prev_free;
    }
  },

  emscripten_builtin_mmap2__deps: ['emscripten_with_builtin_malloc', '$syscallMmap2'],
  emscripten_builtin_mmap2: function (addr, len, prot, flags, fd, off) {
    return _emscripten_with_builtin_malloc(function () {
      return syscallMmap2(addr, len, prot, flags, fd, off);
    });
  },

  emscripten_builtin_munmap__deps: ['emscripten_with_builtin_malloc', '$syscallMunmap'],
  emscripten_builtin_munmap: function (addr, len) {
    return _emscripten_with_builtin_malloc(function () {
      return syscallMunmap(addr, len);
    });
  },

  emscripten_get_stack_top: function() {
    return STACKTOP;
  },

  emscripten_get_stack_base: function() {
    return STACK_BASE;
  },

  $readAsmConstArgs: function(sigPtr, buf) {
    if (!readAsmConstArgs.array) {
      readAsmConstArgs.array = [];
    }
    var args = readAsmConstArgs.array;
    args.length = 0;
    var ch;
    while (ch = HEAPU8[sigPtr++]) {
      if (ch === 100/*'d'*/ || ch === 102/*'f'*/) {
        buf = (buf + 7) & ~7;
        args.push(HEAPF64[(buf >> 3)]);
        buf += 8;
      } else
#if ASSERTIONS
      if (ch === 105 /*'i'*/)
#endif
      {
        buf = (buf + 3) & ~3;
        args.push(HEAP32[(buf >> 2)]);
        buf += 4;
      }
#if ASSERTIONS
      else abort("unexpected char in asm const signature " + ch);
#endif
    }
    return args;
  },

#if !DECLARE_ASM_MODULE_EXPORTS
  // When DECLARE_ASM_MODULE_EXPORTS is not set we export native symbols
  // at runtime rather than statically in JS code.
  $exportAsmFunctions: function(asm) {
#if WASM_BACKEND
    var asmjsMangle = function(x) {
      var unmangledSymbols = {{{ buildStringArray(WASM_FUNCTIONS_THAT_ARE_NOT_NAME_MANGLED) }}};
      return x.indexOf('dynCall_') == 0 || unmangledSymbols.indexOf(x) != -1 ? x : '_' + x;
    };
#endif

#if ENVIRONMENT_MAY_BE_NODE
#if ENVIRONMENT_MAY_BE_WEB
    var global_object = (typeof process !== "undefined" ? global : this);
#else
    var global_object = global;
#endif
#else
    var global_object = this;
#endif

    for (var __exportedFunc in asm) {
#if WASM_BACKEND
      var jsname = asmjsMangle(__exportedFunc);
#else
      var jsname = __exportedFunc;
#endif
#if MINIMAL_RUNTIME
      global_object[jsname] = asm[__exportedFunc];
#else
      global_object[jsname] = Module[jsname] = asm[__exportedFunc];
#endif
    }

  },
#endif

  // Parses as much of the given JS string to an integer, with quiet error
  // handling (returns a NaN on error). E.g. jstoi_q("123abc") returns 123.
  // Note that "smart" radix handling is employed for input string:
  // "0314" is parsed as octal, and "0x1234" is parsed as base-16.
  $jstoi_q__docs: '/** @suppress {checkTypes} */',
  $jstoi_q: function(str) {
    return parseInt(str);
  },

  // Converts a JS string to an integer base-10, with signaling error
  // handling (throws a JS exception on error). E.g. jstoi_s("123abc")
  // throws an exception.
  $jstoi_s: function(str) {
    return Number(str);
  },

  //============================
  // i64 math
  //============================

  i64Add__asm: true,
  i64Add__sig: 'iiiii',
  i64Add: function(a, b, c, d) {
    /*
      x = a + b*2^32
      y = c + d*2^32
      result = l + h*2^32
    */
    a = a|0; b = b|0; c = c|0; d = d|0;
    var l = 0, h = 0;
    l = (a + c)>>>0;
    h = (b + d + (((l>>>0) < (a>>>0))|0))>>>0; // Add carry from low word to high word on overflow.
    {{{ makeStructuralReturn(['l|0', 'h'], true) }}};
  },

  i64Subtract__asm: true,
  i64Subtract__sig: 'iiiii',
  i64Subtract: function(a, b, c, d) {
    a = a|0; b = b|0; c = c|0; d = d|0;
    var l = 0, h = 0;
    l = (a - c)>>>0;
    h = (b - d)>>>0;
    h = (b - d - (((c>>>0) > (a>>>0))|0))>>>0; // Borrow one from high word to low word on underflow.
    {{{ makeStructuralReturn(['l|0', 'h'], true) }}};
  },

  bitshift64Shl__asm: true,
  bitshift64Shl__sig: 'iiii',
  bitshift64Shl: function(low, high, bits) {
    low = low|0; high = high|0; bits = bits|0;
    var ander = 0;
    if ((bits|0) < 32) {
      ander = ((1 << bits) - 1)|0;
      {{{ makeSetTempRet0('(high << bits) | ((low&(ander << (32 - bits))) >>> (32 - bits))') }}};
      return low << bits;
    }
    {{{ makeSetTempRet0('low << (bits - 32)') }}};
    return 0;
  },
  bitshift64Ashr__asm: true,
  bitshift64Ashr__sig: 'iiii',
  bitshift64Ashr: function(low, high, bits) {
    low = low|0; high = high|0; bits = bits|0;
    var ander = 0;
    if ((bits|0) < 32) {
      ander = ((1 << bits) - 1)|0;
      {{{ makeSetTempRet0('high >> bits') }}};
      return (low >>> bits) | ((high&ander) << (32 - bits));
    }
    {{{ makeSetTempRet0('(high|0) < 0 ? -1 : 0') }}};
    return (high >> (bits - 32))|0;
  },
  bitshift64Lshr__asm: true,
  bitshift64Lshr__sig: 'iiii',
  bitshift64Lshr: function(low, high, bits) {
    low = low|0; high = high|0; bits = bits|0;
    var ander = 0;
    if ((bits|0) < 32) {
      ander = ((1 << bits) - 1)|0;
      {{{ makeSetTempRet0('high >>> bits') }}};
      return (low >>> bits) | ((high&ander) << (32 - bits));
    }
    {{{ makeSetTempRet0('0') }}};
    return (high >>> (bits - 32))|0;
  },

  // libunwind

  _Unwind_Backtrace__deps: ['emscripten_get_callstack_js'],
  _Unwind_Backtrace: function(func, arg) {
    var trace = _emscripten_get_callstack_js();
    var parts = trace.split('\n');
    for (var i = 0; i < parts.length; i++) {
      var ret = {{{ makeDynCall('iii') }}}(func, 0, arg);
      if (ret !== 0) return;
    }
  },

  _Unwind_GetIPInfo: function() {
    abort('Unwind_GetIPInfo');
  },

  _Unwind_FindEnclosingFunction: function() {
    return 0; // we cannot succeed
  },

  _Unwind_RaiseException__deps: ['__cxa_throw'],
  _Unwind_RaiseException: function(ex) {
    err('Warning: _Unwind_RaiseException is not correctly implemented');
    return ___cxa_throw(ex, 0, 0);
  },

  _Unwind_DeleteException: function(ex) {
    err('TODO: Unwind_DeleteException');
  },

  // autodebugging

  emscripten_autodebug_i64: function(line, valuel, valueh) {
    out('AD:' + [line, valuel, valueh]);
  },
  emscripten_autodebug_i32: function(line, value) {
    out('AD:' + [line, value]);
  },
  emscripten_autodebug_i16: function(line, value) {
    out('AD:' + [line, value]);
  },
  emscripten_autodebug_i8: function(line, value) {
    out('AD:' + [line, value]);
  },
  emscripten_autodebug_float: function(line, value) {
    out('AD:' + [line, value]);
  },
  emscripten_autodebug_double: function(line, value) {
    out('AD:' + [line, value]);
  },

  // special runtime support

#if MINIMAL_RUNTIME && !WASM_BACKEND
  emscripten_scan_stack__deps: ['$stackSave'],
#endif
  emscripten_scan_stack: function(func) {
    var base = STACK_BASE; // TODO verify this is right on pthreads
    var end = stackSave();
    {{{ makeDynCall('vii') }}}(func, Math.min(base, end), Math.max(base, end));
  },

  // misc definitions to avoid unnecessary unresolved symbols being reported
  // by fastcomp or wasm-ld
#if SUPPORT_LONGJMP
  emscripten_prep_setjmp: function() {},
  emscripten_cleanup_setjmp: function() {},
  emscripten_check_longjmp: function() {},
  emscripten_get_longjmp_result: function() {},
  emscripten_setjmp: function() {},
#endif
  emscripten_preinvoke: function() {},
  emscripten_postinvoke: function() {},
  emscripten_resume: function() {},
  emscripten_landingpad: function() {},
  getHigh32: function() {},
  setHigh32: function() {},
  FtoILow: function() {},
  FtoIHigh: function() {},
  DtoILow: function() {},
  DtoIHigh: function() {},
  BDtoILow: function() {},
  BDtoIHigh: function() {},
  SItoF: function() {},
  UItoF: function() {},
  SItoD: function() {},
  UItoD: function() {},
  BItoD: function() {},
  llvm_dbg_value: function() {},
  llvm_debugtrap: function() {},
  llvm_ctlz_i32: function() {},
  emscripten_asm_const: function() {},
  emscripten_asm_const_int: function() {},
  emscripten_asm_const_double: function() {},
  emscripten_asm_const_int_sync_on_main_thread: function() {},
  emscripten_asm_const_double_sync_on_main_thread: function() {},
  emscripten_asm_const_async_on_main_thread: function() {},

#if !WASM_BACKEND
  // ======== compiled code from system/lib/compiler-rt , see readme therein
  __muldsi3__asm: true,
  __muldsi3__sig: 'iii',
  __muldsi3__deps: ['Math_imul'],
  __muldsi3: function($a, $b) {
    $a = $a | 0;
    $b = $b | 0;
    var $1 = 0, $2 = 0, $3 = 0, $6 = 0, $8 = 0, $11 = 0, $12 = 0;
    $1 = $a & 65535;
    $2 = $b & 65535;
    $3 = Math_imul($2, $1) | 0;
    $6 = $a >>> 16;
    $8 = ($3 >>> 16) + (Math_imul($2, $6) | 0) | 0;
    $11 = $b >>> 16;
    $12 = Math_imul($11, $1) | 0;
    return ({{{ makeSetTempRet0('(($8 >>> 16) + (Math_imul($11, $6) | 0) | 0) + ((($8 & 65535) + $12 | 0) >>> 16) | 0') }}}, 0 | ($8 + $12 << 16 | $3 & 65535)) | 0;
  },
  __divdi3__sig: 'iiiii',
  __divdi3__asm: true,
  __divdi3__deps: ['__udivmoddi4', 'i64Subtract'],
  __divdi3: function($a$0, $a$1, $b$0, $b$1) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    var $1$0 = 0, $1$1 = 0, $2$0 = 0, $2$1 = 0, $4$0 = 0, $4$1 = 0, $6$0 = 0, $7$0 = 0, $7$1 = 0, $8$0 = 0, $10$0 = 0;
    $1$0 = $a$1 >> 31 | (($a$1 | 0) < 0 ? -1 : 0) << 1;
    $1$1 = (($a$1 | 0) < 0 ? -1 : 0) >> 31 | (($a$1 | 0) < 0 ? -1 : 0) << 1;
    $2$0 = $b$1 >> 31 | (($b$1 | 0) < 0 ? -1 : 0) << 1;
    $2$1 = (($b$1 | 0) < 0 ? -1 : 0) >> 31 | (($b$1 | 0) < 0 ? -1 : 0) << 1;
    $4$0 = _i64Subtract($1$0 ^ $a$0 | 0, $1$1 ^ $a$1 | 0, $1$0 | 0, $1$1 | 0) | 0;
    $4$1 = {{{ makeGetTempRet0() }}};
    $6$0 = _i64Subtract($2$0 ^ $b$0 | 0, $2$1 ^ $b$1 | 0, $2$0 | 0, $2$1 | 0) | 0;
    $7$0 = $2$0 ^ $1$0;
    $7$1 = $2$1 ^ $1$1;
    $8$0 = ___udivmoddi4($4$0, $4$1, $6$0, {{{ makeGetTempRet0() }}}, 0) | 0;
    $10$0 = _i64Subtract($8$0 ^ $7$0 | 0, {{{ makeGetTempRet0() }}} ^ $7$1 | 0, $7$0 | 0, $7$1 | 0) | 0;
    return $10$0 | 0;
  },
  __remdi3__sig: 'iiiii',
  __remdi3__asm: true,
  __remdi3__deps: ['__udivmoddi4', 'i64Subtract'],
  __remdi3: function($a$0, $a$1, $b$0, $b$1) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    var $rem = 0, $1$0 = 0, $1$1 = 0, $2$0 = 0, $2$1 = 0, $4$0 = 0, $4$1 = 0, $6$0 = 0, $10$0 = 0, $10$1 = 0, __stackBase__ = 0;
    __stackBase__ = STACKTOP;
    STACKTOP = STACKTOP + 16 | 0;
    $rem = __stackBase__ | 0;
    $1$0 = $a$1 >> 31 | (($a$1 | 0) < 0 ? -1 : 0) << 1;
    $1$1 = (($a$1 | 0) < 0 ? -1 : 0) >> 31 | (($a$1 | 0) < 0 ? -1 : 0) << 1;
    $2$0 = $b$1 >> 31 | (($b$1 | 0) < 0 ? -1 : 0) << 1;
    $2$1 = (($b$1 | 0) < 0 ? -1 : 0) >> 31 | (($b$1 | 0) < 0 ? -1 : 0) << 1;
    $4$0 = _i64Subtract($1$0 ^ $a$0 | 0, $1$1 ^ $a$1 | 0, $1$0 | 0, $1$1 | 0) | 0;
    $4$1 = {{{ makeGetTempRet0() }}};
    $6$0 = _i64Subtract($2$0 ^ $b$0 | 0, $2$1 ^ $b$1 | 0, $2$0 | 0, $2$1 | 0) | 0;
    ___udivmoddi4($4$0, $4$1, $6$0, {{{ makeGetTempRet0() }}}, $rem) | 0;
    $10$0 = _i64Subtract(HEAP32[$rem >> 2] ^ $1$0 | 0, HEAP32[$rem + 4 >> 2] ^ $1$1 | 0, $1$0 | 0, $1$1 | 0) | 0;
    $10$1 = {{{ makeGetTempRet0() }}};
    STACKTOP = __stackBase__;
    return ({{{ makeSetTempRet0('$10$1') }}}, $10$0) | 0;
  },
  __muldi3__sig: 'iiiii',
  __muldi3__asm: true,
  __muldi3__deps: ['__muldsi3', 'Math_imul'],
  __muldi3: function($a$0, $a$1, $b$0, $b$1) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    var $x_sroa_0_0_extract_trunc = 0, $y_sroa_0_0_extract_trunc = 0, $1$0 = 0, $1$1 = 0, $2 = 0;
    $x_sroa_0_0_extract_trunc = $a$0;
    $y_sroa_0_0_extract_trunc = $b$0;
    $1$0 = ___muldsi3($x_sroa_0_0_extract_trunc, $y_sroa_0_0_extract_trunc) | 0;
    $1$1 = {{{ makeGetTempRet0() }}};
    $2 = Math_imul($a$1, $y_sroa_0_0_extract_trunc) | 0;
    return ({{{ makeSetTempRet0('((Math_imul($b$1, $x_sroa_0_0_extract_trunc) | 0) + $2 | 0) + $1$1 | $1$1 & 0') }}}, 0 | $1$0 & -1) | 0;
  },
  __udivdi3__sig: 'iiiii',
  __udivdi3__asm: true,
  __udivdi3__deps: ['__udivmoddi4'],
  __udivdi3: function($a$0, $a$1, $b$0, $b$1) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    var $1$0 = 0;
    $1$0 = ___udivmoddi4($a$0, $a$1, $b$0, $b$1, 0) | 0;
    return $1$0 | 0;
  },
  __uremdi3__sig: 'iiiii',
  __uremdi3__asm: true,
  __uremdi3__deps: ['__udivmoddi4'],
  __uremdi3: function($a$0, $a$1, $b$0, $b$1) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    var $rem = 0, __stackBase__ = 0;
    __stackBase__ = STACKTOP;
    STACKTOP = STACKTOP + 16 | 0;
    $rem = __stackBase__ | 0;
    ___udivmoddi4($a$0, $a$1, $b$0, $b$1, $rem) | 0;
    STACKTOP = __stackBase__;
    return ({{{ makeSetTempRet0('HEAP32[$rem + 4 >> 2] | 0') }}}, HEAP32[$rem >> 2] | 0) | 0;
  },
  __udivmoddi4__sig: 'iiiiii',
  __udivmoddi4__asm: true,
  __udivmoddi4__deps: ['i64Add', 'i64Subtract', 'llvm_cttz_i32', 'Math_clz32'],
  __udivmoddi4: function($a$0, $a$1, $b$0, $b$1, $rem) {
    $a$0 = $a$0 | 0;
    $a$1 = $a$1 | 0;
    $b$0 = $b$0 | 0;
    $b$1 = $b$1 | 0;
    $rem = $rem | 0;
    var $n_sroa_0_0_extract_trunc = 0, $n_sroa_1_4_extract_shift$0 = 0, $n_sroa_1_4_extract_trunc = 0, $d_sroa_0_0_extract_trunc = 0, $d_sroa_1_4_extract_shift$0 = 0, $d_sroa_1_4_extract_trunc = 0, $4 = 0, $17 = 0, $37 = 0, $49 = 0, $51 = 0, $57 = 0, $58 = 0, $66 = 0, $78 = 0, $86 = 0, $88 = 0, $89 = 0, $91 = 0, $92 = 0, $95 = 0, $105 = 0, $117 = 0, $119 = 0, $125 = 0, $126 = 0, $130 = 0, $q_sroa_1_1_ph = 0, $q_sroa_0_1_ph = 0, $r_sroa_1_1_ph = 0, $r_sroa_0_1_ph = 0, $sr_1_ph = 0, $d_sroa_0_0_insert_insert99$0 = 0, $d_sroa_0_0_insert_insert99$1 = 0, $137$0 = 0, $137$1 = 0, $carry_0203 = 0, $sr_1202 = 0, $r_sroa_0_1201 = 0, $r_sroa_1_1200 = 0, $q_sroa_0_1199 = 0, $q_sroa_1_1198 = 0, $147 = 0, $149 = 0, $r_sroa_0_0_insert_insert42$0 = 0, $r_sroa_0_0_insert_insert42$1 = 0, $150$1 = 0, $151$0 = 0, $152 = 0, $154$0 = 0, $r_sroa_0_0_extract_trunc = 0, $r_sroa_1_4_extract_trunc = 0, $155 = 0, $carry_0_lcssa$0 = 0, $carry_0_lcssa$1 = 0, $r_sroa_0_1_lcssa = 0, $r_sroa_1_1_lcssa = 0, $q_sroa_0_1_lcssa = 0, $q_sroa_1_1_lcssa = 0, $q_sroa_0_0_insert_ext75$0 = 0, $q_sroa_0_0_insert_ext75$1 = 0, $q_sroa_0_0_insert_insert77$1 = 0, $_0$0 = 0, $_0$1 = 0;
    $n_sroa_0_0_extract_trunc = $a$0;
    $n_sroa_1_4_extract_shift$0 = $a$1;
    $n_sroa_1_4_extract_trunc = $n_sroa_1_4_extract_shift$0;
    $d_sroa_0_0_extract_trunc = $b$0;
    $d_sroa_1_4_extract_shift$0 = $b$1;
    $d_sroa_1_4_extract_trunc = $d_sroa_1_4_extract_shift$0;
    if (($n_sroa_1_4_extract_trunc | 0) == 0) {
      $4 = ($rem | 0) != 0;
      if (($d_sroa_1_4_extract_trunc | 0) == 0) {
        if ($4) {
          HEAP32[$rem >> 2] = ($n_sroa_0_0_extract_trunc >>> 0) % ($d_sroa_0_0_extract_trunc >>> 0);
          HEAP32[$rem + 4 >> 2] = 0;
        }
        $_0$1 = 0;
        $_0$0 = ($n_sroa_0_0_extract_trunc >>> 0) / ($d_sroa_0_0_extract_trunc >>> 0) >>> 0;
        return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
      } else {
        if (!$4) {
          $_0$1 = 0;
          $_0$0 = 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        HEAP32[$rem >> 2] = $a$0 & -1;
        HEAP32[$rem + 4 >> 2] = $a$1 & 0;
        $_0$1 = 0;
        $_0$0 = 0;
        return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
      }
    }
    $17 = ($d_sroa_1_4_extract_trunc | 0) == 0;
    do {
      if (($d_sroa_0_0_extract_trunc | 0) == 0) {
        if ($17) {
          if (($rem | 0) != 0) {
            HEAP32[$rem >> 2] = ($n_sroa_1_4_extract_trunc >>> 0) % ($d_sroa_0_0_extract_trunc >>> 0);
            HEAP32[$rem + 4 >> 2] = 0;
          }
          $_0$1 = 0;
          $_0$0 = ($n_sroa_1_4_extract_trunc >>> 0) / ($d_sroa_0_0_extract_trunc >>> 0) >>> 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        if (($n_sroa_0_0_extract_trunc | 0) == 0) {
          if (($rem | 0) != 0) {
            HEAP32[$rem >> 2] = 0;
            HEAP32[$rem + 4 >> 2] = ($n_sroa_1_4_extract_trunc >>> 0) % ($d_sroa_1_4_extract_trunc >>> 0);
          }
          $_0$1 = 0;
          $_0$0 = ($n_sroa_1_4_extract_trunc >>> 0) / ($d_sroa_1_4_extract_trunc >>> 0) >>> 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        $37 = $d_sroa_1_4_extract_trunc - 1 | 0;
        if (($37 & $d_sroa_1_4_extract_trunc | 0) == 0) {
          if (($rem | 0) != 0) {
            HEAP32[$rem >> 2] = 0 | $a$0 & -1;
            HEAP32[$rem + 4 >> 2] = $37 & $n_sroa_1_4_extract_trunc | $a$1 & 0;
          }
          $_0$1 = 0;
          $_0$0 = $n_sroa_1_4_extract_trunc >>> ((_llvm_cttz_i32($d_sroa_1_4_extract_trunc | 0) | 0) >>> 0);
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        $49 = Math_clz32($d_sroa_1_4_extract_trunc | 0) | 0;
        $51 = $49 - (Math_clz32($n_sroa_1_4_extract_trunc | 0) | 0) | 0;
        if ($51 >>> 0 <= 30) {
          $57 = $51 + 1 | 0;
          $58 = 31 - $51 | 0;
          $sr_1_ph = $57;
          $r_sroa_0_1_ph = $n_sroa_1_4_extract_trunc << $58 | $n_sroa_0_0_extract_trunc >>> ($57 >>> 0);
          $r_sroa_1_1_ph = $n_sroa_1_4_extract_trunc >>> ($57 >>> 0);
          $q_sroa_0_1_ph = 0;
          $q_sroa_1_1_ph = $n_sroa_0_0_extract_trunc << $58;
          break;
        }
        if (($rem | 0) == 0) {
          $_0$1 = 0;
          $_0$0 = 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        HEAP32[$rem >> 2] = 0 | $a$0 & -1;
        HEAP32[$rem + 4 >> 2] = $n_sroa_1_4_extract_shift$0 | $a$1 & 0;
        $_0$1 = 0;
        $_0$0 = 0;
        return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
      } else {
        if (!$17) {
          $117 = Math_clz32($d_sroa_1_4_extract_trunc | 0) | 0;
          $119 = $117 - (Math_clz32($n_sroa_1_4_extract_trunc | 0) | 0) | 0;
          if ($119 >>> 0 <= 31) {
            $125 = $119 + 1 | 0;
            $126 = 31 - $119 | 0;
            $130 = $119 - 31 >> 31;
            $sr_1_ph = $125;
            $r_sroa_0_1_ph = $n_sroa_0_0_extract_trunc >>> ($125 >>> 0) & $130 | $n_sroa_1_4_extract_trunc << $126;
            $r_sroa_1_1_ph = $n_sroa_1_4_extract_trunc >>> ($125 >>> 0) & $130;
            $q_sroa_0_1_ph = 0;
            $q_sroa_1_1_ph = $n_sroa_0_0_extract_trunc << $126;
            break;
          }
          if (($rem | 0) == 0) {
            $_0$1 = 0;
            $_0$0 = 0;
            return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
          }
          HEAP32[$rem >> 2] = 0 | $a$0 & -1;
          HEAP32[$rem + 4 >> 2] = $n_sroa_1_4_extract_shift$0 | $a$1 & 0;
          $_0$1 = 0;
          $_0$0 = 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
        $66 = $d_sroa_0_0_extract_trunc - 1 | 0;
        if (($66 & $d_sroa_0_0_extract_trunc | 0) != 0) {
          $86 = (Math_clz32($d_sroa_0_0_extract_trunc | 0) | 0) + 33 | 0;
          $88 = $86 - (Math_clz32($n_sroa_1_4_extract_trunc | 0) | 0) | 0;
          $89 = 64 - $88 | 0;
          $91 = 32 - $88 | 0;
          $92 = $91 >> 31;
          $95 = $88 - 32 | 0;
          $105 = $95 >> 31;
          $sr_1_ph = $88;
          $r_sroa_0_1_ph = $91 - 1 >> 31 & $n_sroa_1_4_extract_trunc >>> ($95 >>> 0) | ($n_sroa_1_4_extract_trunc << $91 | $n_sroa_0_0_extract_trunc >>> ($88 >>> 0)) & $105;
          $r_sroa_1_1_ph = $105 & $n_sroa_1_4_extract_trunc >>> ($88 >>> 0);
          $q_sroa_0_1_ph = $n_sroa_0_0_extract_trunc << $89 & $92;
          $q_sroa_1_1_ph = ($n_sroa_1_4_extract_trunc << $89 | $n_sroa_0_0_extract_trunc >>> ($95 >>> 0)) & $92 | $n_sroa_0_0_extract_trunc << $91 & $88 - 33 >> 31;
          break;
        }
        if (($rem | 0) != 0) {
          HEAP32[$rem >> 2] = $66 & $n_sroa_0_0_extract_trunc;
          HEAP32[$rem + 4 >> 2] = 0;
        }
        if (($d_sroa_0_0_extract_trunc | 0) == 1) {
          $_0$1 = $n_sroa_1_4_extract_shift$0 | $a$1 & 0;
          $_0$0 = 0 | $a$0 & -1;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        } else {
          $78 = _llvm_cttz_i32($d_sroa_0_0_extract_trunc | 0) | 0;
          $_0$1 = 0 | $n_sroa_1_4_extract_trunc >>> ($78 >>> 0);
          $_0$0 = $n_sroa_1_4_extract_trunc << 32 - $78 | $n_sroa_0_0_extract_trunc >>> ($78 >>> 0) | 0;
          return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
        }
      }
    } while (0);
    if (($sr_1_ph | 0) == 0) {
      $q_sroa_1_1_lcssa = $q_sroa_1_1_ph;
      $q_sroa_0_1_lcssa = $q_sroa_0_1_ph;
      $r_sroa_1_1_lcssa = $r_sroa_1_1_ph;
      $r_sroa_0_1_lcssa = $r_sroa_0_1_ph;
      $carry_0_lcssa$1 = 0;
      $carry_0_lcssa$0 = 0;
    } else {
      $d_sroa_0_0_insert_insert99$0 = 0 | $b$0 & -1;
      $d_sroa_0_0_insert_insert99$1 = $d_sroa_1_4_extract_shift$0 | $b$1 & 0;
      $137$0 = _i64Add($d_sroa_0_0_insert_insert99$0 | 0, $d_sroa_0_0_insert_insert99$1 | 0, -1, -1) | 0;
      $137$1 = {{{ makeGetTempRet0() }}};
      $q_sroa_1_1198 = $q_sroa_1_1_ph;
      $q_sroa_0_1199 = $q_sroa_0_1_ph;
      $r_sroa_1_1200 = $r_sroa_1_1_ph;
      $r_sroa_0_1201 = $r_sroa_0_1_ph;
      $sr_1202 = $sr_1_ph;
      $carry_0203 = 0;
      while (1) {
        $147 = $q_sroa_0_1199 >>> 31 | $q_sroa_1_1198 << 1;
        $149 = $carry_0203 | $q_sroa_0_1199 << 1;
        $r_sroa_0_0_insert_insert42$0 = 0 | ($r_sroa_0_1201 << 1 | $q_sroa_1_1198 >>> 31);
        $r_sroa_0_0_insert_insert42$1 = $r_sroa_0_1201 >>> 31 | $r_sroa_1_1200 << 1 | 0;
        _i64Subtract($137$0 | 0, $137$1 | 0, $r_sroa_0_0_insert_insert42$0 | 0, $r_sroa_0_0_insert_insert42$1 | 0) | 0;
        $150$1 = {{{ makeGetTempRet0() }}};
        $151$0 = $150$1 >> 31 | (($150$1 | 0) < 0 ? -1 : 0) << 1;
        $152 = $151$0 & 1;
        $154$0 = _i64Subtract($r_sroa_0_0_insert_insert42$0 | 0, $r_sroa_0_0_insert_insert42$1 | 0, $151$0 & $d_sroa_0_0_insert_insert99$0 | 0, ((($150$1 | 0) < 0 ? -1 : 0) >> 31 | (($150$1 | 0) < 0 ? -1 : 0) << 1) & $d_sroa_0_0_insert_insert99$1 | 0) | 0;
        $r_sroa_0_0_extract_trunc = $154$0;
        $r_sroa_1_4_extract_trunc = {{{ makeGetTempRet0() }}};
        $155 = $sr_1202 - 1 | 0;
        if (($155 | 0) == 0) {
          break;
        } else {
          $q_sroa_1_1198 = $147;
          $q_sroa_0_1199 = $149;
          $r_sroa_1_1200 = $r_sroa_1_4_extract_trunc;
          $r_sroa_0_1201 = $r_sroa_0_0_extract_trunc;
          $sr_1202 = $155;
          $carry_0203 = $152;
        }
      }
      $q_sroa_1_1_lcssa = $147;
      $q_sroa_0_1_lcssa = $149;
      $r_sroa_1_1_lcssa = $r_sroa_1_4_extract_trunc;
      $r_sroa_0_1_lcssa = $r_sroa_0_0_extract_trunc;
      $carry_0_lcssa$1 = 0;
      $carry_0_lcssa$0 = $152;
    }
    $q_sroa_0_0_insert_ext75$0 = $q_sroa_0_1_lcssa;
    $q_sroa_0_0_insert_ext75$1 = 0;
    $q_sroa_0_0_insert_insert77$1 = $q_sroa_1_1_lcssa | $q_sroa_0_0_insert_ext75$1;
    if (($rem | 0) != 0) {
      HEAP32[$rem >> 2] = 0 | $r_sroa_0_1_lcssa;
      HEAP32[$rem + 4 >> 2] = $r_sroa_1_1_lcssa | 0;
    }
    $_0$1 = (0 | $q_sroa_0_0_insert_ext75$0) >>> 31 | $q_sroa_0_0_insert_insert77$1 << 1 | ($q_sroa_0_0_insert_ext75$1 << 1 | $q_sroa_0_0_insert_ext75$0 >>> 31) & 0 | $carry_0_lcssa$1;
    $_0$0 = ($q_sroa_0_0_insert_ext75$0 << 1 | 0 >>> 31) & -2 | $carry_0_lcssa$0;
    return ({{{ makeSetTempRet0('$_0$1') }}}, $_0$0) | 0;
  },
  // =======================================================================
#endif

  __handle_stack_overflow: function() {
    abort('stack overflow')
  },

  _getExecutableName: function() {
#if MINIMAL_RUNTIME // MINIMAL_RUNTIME does not have a global runtime variable thisProgram
#if ENVIRONMENT_MAY_BE_NODE
    if (ENVIRONMENT_IS_NODE && process['argv'].length > 1) {
      return process['argv'][1].replace(/\\/g, '/');
    }
#endif
    return "./this.program";
#else
    return thisProgram || './this.program';
#endif
  },
};

function autoAddDeps(object, name) {
  for (var item in object) {
    if (item.substr(-6) != '__deps') {
      if (!object[item + '__deps']) {
        object[item + '__deps'] = [name];
      } else {
        object[item + '__deps'].push(name); // add to existing list
      }
    }
  }
}
