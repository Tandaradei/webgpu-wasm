/**
 * @license
 * Copyright 2019 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

#if SEPARATE_ASM && ASSERTIONS && WASM == 0 && MODULARIZE
if (!({{{ASM_MODULE_NAME}}})) throw 'Must load asm.js Module in to variable {{{ASM_MODULE_NAME}}} before adding compiled output .js script to the DOM';
#endif

#include "runtime_safe_heap.js"

#if ASSERTIONS
/** @type {function(*, string=)} */
function assert(condition, text) {
  if (!condition) throw text;
}
#endif

/** @param {string|number=} what */
function abort(what) {
#if ASSERTIONS
  throw new Error(what);
#else
  throw what;
#endif
}

var tempRet0 = 0;
var setTempRet0 = function(value) {
  tempRet0 = value;
}
var getTempRet0 = function() {
  return tempRet0;
}

function alignUp(x, multiple) {
  if (x % multiple > 0) {
    x += multiple - (x % multiple);
  }
  return x;
}

#if WASM != 2 && MAYBE_WASM2JS
#if !WASM2JS
if (Module['doWasm2JS']) {
#endif
#include "wasm2js.js"
#if !WASM2JS
}
#endif
#endif

#if SINGLE_FILE && WASM == 1 && !WASM2JS
#include "base64Decode.js"
Module['wasm'] = base64Decode('{{{ getQuoted("WASM_BINARY_DATA") }}}');
#endif

#include "runtime_functions.js"
#include "runtime_strings.js"
#include "runtime_sab_polyfill.js"

#if USE_PTHREADS
var STATIC_BASE = {{{ GLOBAL_BASE }}};

if (!ENVIRONMENT_IS_PTHREAD) {
#endif

var GLOBAL_BASE = {{{ GLOBAL_BASE }}},
    TOTAL_STACK = {{{ TOTAL_STACK }}},
#if !USE_PTHREADS
    STATIC_BASE = {{{ GLOBAL_BASE }}},
#endif
    STACK_BASE = {{{ getQuoted('STACK_BASE') }}},
    STACKTOP = STACK_BASE,
    STACK_MAX = {{{ getQuoted('STACK_MAX') }}}
#if USES_DYNAMIC_ALLOC
    , DYNAMICTOP_PTR = {{{ DYNAMICTOP_PTR }}};
#endif
    ;

#if WASM

#if ALLOW_MEMORY_GROWTH && MAXIMUM_MEMORY != -1
var wasmMaximumMemory = {{{ MAXIMUM_MEMORY >>> 16 }}};
#else
var wasmMaximumMemory = {{{ INITIAL_MEMORY >>> 16}}};
#endif

var wasmMemory = new WebAssembly.Memory({
  'initial': {{{ INITIAL_MEMORY >>> 16 }}}
#if USE_PTHREADS || !ALLOW_MEMORY_GROWTH || MAXIMUM_MEMORY != -1
  , 'maximum': wasmMaximumMemory
#endif
#if USE_PTHREADS
  , 'shared': true
#endif
  });

var buffer = wasmMemory.buffer;

#if USE_PTHREADS
}
#if ASSERTIONS
assert(buffer instanceof SharedArrayBuffer, 'requested a shared WebAssembly.Memory but the returned buffer is not a SharedArrayBuffer, indicating that while the browser has SharedArrayBuffer it does not have WebAssembly threads support - you may need to set a flag');
#endif
#endif

var wasmTable = new WebAssembly.Table({
  'initial': {{{ getQuoted('WASM_TABLE_SIZE') }}},
#if !ALLOW_TABLE_GROWTH
#if WASM_BACKEND
  'maximum': {{{ getQuoted('WASM_TABLE_SIZE') }}} + {{{ RESERVED_FUNCTION_POINTERS }}},
#else
  'maximum': {{{ getQuoted('WASM_TABLE_SIZE') }}},
#endif
#endif // WASM_BACKEND
  'element': 'anyfunc'
});

#else

#if USE_PTHREADS
var buffer = new SharedArrayBuffer({{{ INITIAL_MEMORY }}});
#else
var buffer = new ArrayBuffer({{{ INITIAL_MEMORY }}});
#endif

#if USE_PTHREADS
}
#endif

#endif

#if ASSERTIONS
var WASM_PAGE_SIZE = 65536;
#if USE_PTHREADS
if (!ENVIRONMENT_IS_PTHREAD) {
#endif
assert(STACK_BASE % 16 === 0, 'stack must start aligned to 16 bytes, STACK_BASE==' + STACK_BASE);
assert(({{{ getQuoted('DYNAMIC_BASE') }}}) % 16 === 0, 'heap must start aligned to 16 bytes, DYNAMIC_BASE==' + {{{ getQuoted('DYNAMIC_BASE') }}});
assert({{{ INITIAL_MEMORY }}} >= TOTAL_STACK, 'INITIAL_MEMORY should be larger than TOTAL_STACK, was ' + {{{ INITIAL_MEMORY }}} + '! (TOTAL_STACK=' + TOTAL_STACK + ')');
assert({{{ INITIAL_MEMORY }}} % WASM_PAGE_SIZE === 0);
#if MAXIMUM_MEMORY != -1
assert({{{ MAXIMUM_MEMORY }}} % WASM_PAGE_SIZE == 0);
#endif
assert(buffer.byteLength === {{{ INITIAL_MEMORY }}});
#if USE_PTHREADS
}
#endif
#endif // ASSERTIONS

#if ALLOW_MEMORY_GROWTH
// In ALLOW_MEMORY_GROWTH, we need to be able to re-initialize the
// typed array buffer and heap views to the buffer whenever the heap
// is resized.
var HEAP8, HEAP16, HEAP32, HEAPU8, HEAPU16, HEAPU32, HEAPF32, HEAPF64;
function updateGlobalBufferAndViews(b) {
  buffer = b;
  HEAP8 = new Int8Array(b);
  HEAP16 = new Int16Array(b);
  HEAP32 = new Int32Array(b);
  HEAPU8 = new Uint8Array(b);
  HEAPU16 = new Uint16Array(b);
  HEAPU32 = new Uint32Array(b);
  HEAPF32 = new Float32Array(b);
  HEAPF64 = new Float64Array(b);
}
updateGlobalBufferAndViews(buffer);
#else
// In non-ALLOW_MEMORY_GROWTH scenario, we only need to initialize
// the heap once, so optimize code size to do it statically here.
var HEAP8 = new Int8Array(buffer);
var HEAP16 = new Int16Array(buffer);
var HEAP32 = new Int32Array(buffer);
var HEAPU8 = new Uint8Array(buffer);
var HEAPU16 = new Uint16Array(buffer);
var HEAPU32 = new Uint32Array(buffer);
var HEAPF32 = new Float32Array(buffer);
var HEAPF64 = new Float64Array(buffer);
#endif

#if USE_PTHREADS && ((MEM_INIT_METHOD == 1 && !MEM_INIT_IN_WASM && !SINGLE_FILE) || (SINGLE_FILE && !WASM && !WASM_BACKEND) || USES_DYNAMIC_ALLOC)
if (!ENVIRONMENT_IS_PTHREAD) {
#endif

#if MEM_INIT_METHOD == 1 && !MEM_INIT_IN_WASM && !SINGLE_FILE

#if ASSERTIONS
if (!Module['mem']) throw 'Must load memory initializer as an ArrayBuffer in to variable Module.mem before adding compiled output .js script to the DOM';
#endif
HEAPU8.set(new Uint8Array(Module['mem']), GLOBAL_BASE);

#endif

#if SINGLE_FILE && !WASM && !WASM_BACKEND
#include "base64Decode.js"
HEAPU8.set(base64Decode('{{{ getQuoted("BASE64_MEMORY_INITIALIZER") }}}'), GLOBAL_BASE);
#endif

#if USES_DYNAMIC_ALLOC
  HEAP32[DYNAMICTOP_PTR>>2] = {{{ getQuoted('DYNAMIC_BASE') }}};
#endif

#if USE_PTHREADS && ((MEM_INIT_METHOD == 1 && !MEM_INIT_IN_WASM && !SINGLE_FILE) || (SINGLE_FILE && !WASM && !WASM_BACKEND) || USES_DYNAMIC_ALLOC)
}
#endif

#include "runtime_stack_check.js"
#include "runtime_assertions.js"

#if LOAD_SOURCE_MAP
var wasmSourceMap;
#include "source_map_support.js"
#endif

#if USE_OFFSET_CONVERTER
var wasmOffsetConverter;
#include "wasm_offset_converter.js"
#endif

#if EXIT_RUNTIME

function callRuntimeCallbacks(callbacks) {
  while(callbacks.length > 0) {
    var callback = callbacks.shift();
    if (typeof callback == 'function') {
      callback();
      continue;
    }
    var func = callback.func;
    if (typeof func === 'number') {
      if (callback.arg === undefined) {
        dynCall_v(func);
      } else {
        dynCall_vi(func, callback.arg);
      }
    } else {
      func(callback.arg === undefined ? null : callback.arg);
    }
  }
}

var __ATEXIT__    = []; // functions called during shutdown
#endif

#if ASSERTIONS
var runtimeInitialized = false;

// This is always false in minimal_runtime - the runtime does not have a concept of exiting (keeping this variable here for now since it is referenced from generated code)
var runtimeExited = false;
#endif

/** @param {number|boolean=} ignore */
{{{ unSign }}}
/** @param {number|boolean=} ignore */
{{{ reSign }}}

#include "runtime_math.js"

var memoryInitializer = null;

#include "memoryprofiler.js"

#include "runtime_debug.js"

// === Body ===
