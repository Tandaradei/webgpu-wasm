//
// @license
// Copyright 2010 The Emscripten Authors
// SPDX-License-Identifier: MIT
//

//
// Various compiler settings. These are simply variables present when the
// JS compiler runs. To set them, do something like:
//
//   emcc -s OPTION1=VALUE1 -s OPTION2=VALUE2 [..other stuff..]
//
// For convenience and readability `-s OPTION` expands to `-s OPTION=1`
// and `-s NO_OPTION` expands to `-s OPTION=0` (assuming OPTION is a valid
// option).
//
// See https://github.com/emscripten-core/emscripten/wiki/Code-Generation-Modes/
//
// Note that the values here are the defaults in -O0, that is, unoptimized
// mode. See apply_opt_level in tools/shared.py for how -O1,2,3 affect these
// flags.
//
// These flags should only have an effect when compiling to JS, so there
// should not be a need to have them when just compiling source to
// bitcode. However, there will also be no harm either, so it is ok to.
//
// Settings in this file can be directly set from the command line.  Internal
// settings that are not part of the user ABI live in the settings_internal.js.
//
// In general it is best to pass the same arguments at both compile and link
// time, as whether wasm object files are used or not affects when codegen
// happens (without wasm object files, or when using fastcomp, codegen is all
// during link; otherwise, it is during compile). Flags affecting codegen must
// be passed when codegen happens, so to let a build easily switch when codegen
// happens (LTO vs normal), pass the flags at both times. The flags are also
// annotated in this file:
//
// [link] - Should be passed at link time. This is the case for all JS flags,
//          as we emit JS at link (and that is most of the flags here, and
//          hence the default).
// [compile+link] - A flag that has an effect at both compile and link time,
//                  basically any time emcc is invoked. The same flag should be
//                  passed at both times in most cases.
//
// If not otherwise specified, a flag is [link]. Note that no flag is only
// relevant during compile time, as during link we may do codegen for system
// libraries and other support code, so all flags are either link or
// compile+link.
//
// The [fastcomp-only] annotation means that a flag only affects code generation
// in fastcomp.
//
// The [upstream-only] annotation means that a flag only affects code generation
// in upstream.
//

// Tuning

// Whether we should add runtime assertions, for example to
// check that each allocation to the stack does not
// exceed its size, whether all allocations (stack and static) are
// of positive size, etc., whether we should throw if we encounter a bad __label__, i.e.,
// if code flow runs into a fault
// ASSERTIONS == 2 gives even more runtime checks, that may be very slow. That
// includes internal dlmalloc assertions.
var ASSERTIONS = 1;

// Whether extra logging should be enabled.
// This logging isn't quite assertion-quality in that it isn't necessarily a
// symptom that something is wrong.
var RUNTIME_LOGGING = 0;

// Chooses what kind of stack smash checks to emit to generated code:
// 0: Stack overflows are not checked.
// 1: Adds a security cookie at the top of the stack, which is checked at end of
//    each tick and at exit (practically zero performance overhead)
// 2: Same as above, but also adds an explicit check for allocate() calls which
//    call ALLOC_STACK. Has a small performance cost.
//    -s ASSERTIONS=1 automatically enables -s STACK_OVERFLOW_CHECK=2.
var STACK_OVERFLOW_CHECK = 0;

// When set to 1, will generate more verbose output during compilation.
var VERBOSE = 0;

// Whether we will run the main() function. Disable if you embed the generated
// code in your own, and will call main() yourself at the right time (which you
// can do with Module.callMain(), with an optional parameter of commandline args).
var INVOKE_RUN = 1;

// If 0, the runtime is not quit when main() completes (allowing code to
// run afterwards, for example from the browser main event loop). atexit()s
// are also not executed, and we can avoid including code for runtime shutdown,
// like flushing the stdio streams.
// Set this to 1 if you do want atexit()s or stdio streams to be flushed
// on exit.
// This setting is controlled automatically in STANDALONE_WASM mode:
//  - For a command (has a main function) this is always 1
//  - For a reactor (no a main function) this is always 0
var EXIT_RUNTIME = 0;

// How to represent the initial memory content.
// 0: embed a base64 string literal representing the initial memory data
// 1: create a *.mem file containing the binary data of the initial memory;

//    use the --memory-init-file command line switch to select this method
// 2: embed a string literal representing that initial memory data
var MEM_INIT_METHOD = 0;

// The total stack size. There is no way to enlarge the stack, so this
// value must be large enough for the program's requirements. If
// assertions are on, we will assert on not exceeding this, otherwise,
// it will fail silently.
var TOTAL_STACK = 5*1024*1024;

// What malloc()/free() to use, out of
//  * dlmalloc - a powerful general-purpose malloc
//  * emmalloc - a simple and compact malloc designed for emscripten
//  * none     - no malloc() implementation is provided, but you must implement
//               malloc() and free() yourself.
// dlmalloc is necessary for split memory and other special modes, and will be
// used automatically in those cases.
// In general, if you don't need one of those special modes, and if you don't
// allocate very many small objects, you should use emmalloc since it's
// smaller. Otherwise, if you do allocate many small objects, dlmalloc
// is usually worth the extra size.
var MALLOC = "dlmalloc";

// If 1, then when malloc would fail we abort(). This is nonstandard behavior,
// but makes sense for the web since we have a fixed amount of memory that
// must all be allocated up front, and so (a) failing mallocs are much more
// likely than on other platforms, and (b) people need a way to find out
// how big that initial allocation (INITIAL_MEMORY) must be.
// If you set this to 0, then you get the standard malloc behavior of
// returning NULL (0) when it fails.
//
// Setting ALLOW_MEMORY_GROWTH turns this off, as in that mode we default to
// the behavior of trying to grow and returning 0 from malloc on failure, like
// a standard system would. However, you can still set this flag to override
// that.
//    * This is a mostly-backwards-compatible change. Previously this option
//      was ignored when growth was on. The current behavior is that growth
//      turns it off by default, so for users that never specified the flag
//      nothing changes. But if you do specify it, it will have an effect now,
//      which it did not previously. If you don't want that, just stop passing
//      it in at link time.
//
// [link]
var ABORTING_MALLOC = 1;

// If 1, generated a version of memcpy() and memset() that unroll their
// copy sizes. If 0, optimizes for size instead to generate a smaller memcpy.
// This flag only has effect when targeting asm.js.
// [fastcomp-only]
var FAST_UNROLLED_MEMCPY_AND_MEMSET = 1;

// The initial amount of memory to use. Using more memory than this will
// cause us to expand the heap, which can be costly with typed arrays:
// we need to copy the old heap into a new one in that case.
// If ALLOW_MEMORY_GROWTH is set, this initial amount of memory can increase
// later; if not, then it is the final and total amount of memory.
//
// (This option was formerly called TOTAL_MEMORY.)
var INITIAL_MEMORY = 16777216;

// Set the maximum size of memory in the wasm module (in bytes). This is only
// relevant when ALLOW_MEMORY_GROWTH is set, as without growth, the size of
// INITIAL_MEMORY is the final size of memory anyhow.
//
// If this value is -1, it means there is no specified limit.
//
// This setting only matters for wasm and wasm2js, as in asm.js with fastcomp
// there is no place to set a maximum.
//
// Note that the default value here is 2GB, which means that by default if you
// enable memory growth then we can grow up to 2GB but no higher. 2GB is a
// natural limit for several reasons:
//
//   * If the maximum heap size is over 2GB, then pointers must be unsigned in
//     JavaScript, which increases code size. We don't want memory growth builds
//     to be larger unless someone explicitly opts in to >2GB+ heaps.
//   * Historically no VM has supported more >2GB+, and only recently (Mar 2020)
//     has support started to appear. As support is limited, it's safer for
//     people to opt into >2GB+ heaps rather than get a build that may not
//     work on all VMs.
//
// (This option was formerly called WASM_MEM_MAX and BINARYEN_MEM_MAX.)
var MAXIMUM_MEMORY = 2147483648;

// If false, we abort with an error if we try to allocate more memory than
// we can (INITIAL_MEMORY). If true, we will grow the memory arrays at
// runtime, seamlessly and dynamically. This has a performance cost in asm.js,
// both during the actual growth and in general (the latter is because in
// that case we must be careful about optimizations, in particular the
// eliminator), but in wasm it is efficient and should be used whenever relevant.
// See https://code.google.com/p/v8/issues/detail?id=3907 regarding
// memory growth performance in chrome.
// Note that growing memory means we replace the JS typed array views, as
// once created they cannot be resized. (This happens both in asm.js and in
// wasm - in wasm we can grow the Memory, but still need to create new
// views for JS.)
// Setting this option on will disable ABORTING_MALLOC, in other words,
// ALLOW_MEMORY_GROWTH enables fully standard behavior, of both malloc
// returning 0 when it fails, and also of being able to allocate more
// memory from the system as necessary.
var ALLOW_MEMORY_GROWTH = 0;

// If ALLOW_MEMORY_GROWTH is true, this variable specifies the geometric
// overgrowth rate of the heap at resize. Specify MEMORY_GROWTH_GEOMETRIC_STEP=0
// to disable overgrowing the heap at all, or e.g.
// MEMORY_GROWTH_GEOMETRIC_STEP=1.0 to double the heap (+100%) at every grow step.
// The larger this value is, the more memory the WebAssembly heap overreserves
// to reduce performance hiccups coming from memory resize, and the smaller
// this value is, the more memory is conserved, at the performance of more
// stuttering when the heap grows. (profiled to be on the order of ~20 msecs)
var MEMORY_GROWTH_GEOMETRIC_STEP = 0.20;

// Specifies a cap for the maximum geometric overgrowth size, in bytes. Use
// this value to constrain the geometric grow to not exceed a specific rate.
// Pass MEMORY_GROWTH_GEOMETRIC_CAP=0 to disable the cap and allow unbounded
// size increases.
var MEMORY_GROWTH_GEOMETRIC_CAP = 96*1024*1024;

// If ALLOW_MEMORY_GROWTH is true and MEMORY_GROWTH_LINEAR_STEP == -1, then
// geometric memory overgrowth is utilized (above variable). Set
// MEMORY_GROWTH_LINEAR_STEP to a multiple of WASM page size (64KB), eg. 16MB to
// replace geometric overgrowth rate with a constant growth step size. When
// MEMORY_GROWTH_LINEAR_STEP is used, the variables MEMORY_GROWTH_GEOMETRIC_STEP
// and MEMORY_GROWTH_GEOMETRIC_CAP are ignored.
var MEMORY_GROWTH_LINEAR_STEP = -1;

// If true, allows more functions to be added to the table at runtime. This is
// necessary for dynamic linking, and set automatically in that mode.
var ALLOW_TABLE_GROWTH = 0;

// where global data begins; the start of static memory. -1 means use the
// default, any other value will be used as an override
var GLOBAL_BASE = -1;

// where the stack will begin. -1 means use the default. if the stack cannot
// start at the value specified here, it may start at a higher location.
// this is useful when debugging two builds that may differ in their static
// allocations, by forcing the stack to start in the same place their
// memory usage patterns would be the same.

// How to load and store 64-bit doubles.  A potential risk is that doubles may
// be only 32-bit aligned. Forcing 64-bit alignment in Clang itself should be
// able to solve that, or as a workaround in DOUBLE_MODE 1 we will carefully
// load in parts, in a way that requires only 32-bit alignment. In DOUBLE_MODE 0
// we will simply store and load doubles as 32-bit floats, so when they are
// stored/loaded they will truncate from 64 to 32 bits, and lose precision. This
// is faster, and might work for some code (but probably that code should just
// use floats and not doubles anyhow).  Note that a downside of DOUBLE_MODE 1 is
// that we currently store the double in parts, then load it aligned, and that
// load-store will make JS engines alter it if it is being stored to a typed
// array for security reasons. That will 'fix' the number from being a NaN or an
// infinite number.
// [fastcomp-only]
var DOUBLE_MODE = 1;

// Warn at compile time about instructions that LLVM tells us are not fully
// aligned.  This is useful to find places in your code where you might refactor
// to ensure proper alignment.  This is currently only supported in asm.js, not
// wasm.
var WARN_UNALIGNED = 0;

// 0: Use JS numbers for floating-point values. These are 64-bit and do not model C++
//    floats exactly, which are 32-bit.
// 1: Model C++ floats precisely, using Math.fround, polyfilling when necessary. This
//    can be slow if the polyfill is used on heavy float32 computation. See note on
//    browser support below.
// 2: Model C++ floats precisely using Math.fround if available in the JS engine, otherwise
//    use an empty polyfill. This will have much less of a speed penalty than using the full
//    polyfill in cases where engine support is not present. In addition, we can
//    remove the empty polyfill calls themselves on the client when generating html,
//    which should mean that this gives you the best of both worlds of 0 and 1, and is
//    therefore recommended, *unless* you need a guarantee of proper float32 precision
//    (in that case, use option 1).
// XXX Note: To optimize float32-using code, we use the 'const' keyword in the emitted
//           code. This allows us to avoid unnecessary calls to Math.fround, which would
//           slow down engines not yet supporting that function. 'const' is present in
//           all modern browsers, including Firefox, Chrome and Safari, but in IE is only
//           present in IE11 and above. Therefore if you need to support legacy versions of
//           IE, you should not enable PRECISE_F32 1 or 2.
// [fastcomp-only]
// With upstream backend and WASM=0, JS output always uses Math.fround for consistent
// behavior with WebAssembly.
var PRECISE_F32 = 0;

// Whether to allow autovectorized SIMD code
// (https://github.com/johnmccutchan/ecmascript_simd).  SIMD intrinsics are
// always compiled to SIMD code, so you only need this option if you also want
// the autovectorizer to run.  Note that SIMD support in browsers is not yet
// there (as of Sep 2, 2014), so you will be running in a polyfill, which is not
// fast.
var SIMD = 0;

// Whether closure compiling is being run on this output
var USE_CLOSURE_COMPILER = 0;

// Specifies how warnings emitted by Closure are treated. Possible
// options: 'quiet', 'warn', 'error'. If set to 'warn', Closure warnings are printed
// out to console. If set to 'error', Closure warnings are treated like errors,
// similar to -Werror compiler flag.
var CLOSURE_WARNINGS = 'quiet';

// If set to 1, each asm.js/wasm module export is individually declared with a
// JavaScript "var" definition. This is the simple and recommended approach.
// However, this does increase code size (especially if you have many such
// exports), which can be avoided in an unsafe way by setting this to 0. In that
// case, no "var" is created for each export, and instead a loop (of small
// constant code size, no matter how many exports you have) writes all the
// exports received into the global scope. Doing so is dangerous since such
// modifications of the global scope can confuse external JS minifier tools, and
// also things can break if the scope the code is in is not the global scope
// (e.g. if you manually enclose them in a function scope).
var DECLARE_ASM_MODULE_EXPORTS = 1;

// Ignore closure warnings and errors (like on duplicate definitions)
var IGNORE_CLOSURE_COMPILER_ERRORS = 0;

// A limit on inlining. If 0, we will inline normally in LLVM and closure. If
// greater than 0, we will *not* inline in LLVM, and we will prevent inlining of
// functions of this size or larger in closure. 50 is a reasonable setting if
// you do not want inlining
// [compile+link]
var INLINING_LIMIT = 0;

// Run aggressiveVariableElimination in js-optimizer.js
// [fastcomp-only]
var AGGRESSIVE_VARIABLE_ELIMINATION = 0;

// Whether to simplify ifs in js-optimizer.js
// [fastcomp-only]
var SIMPLIFY_IFS = 1;

// Check each write to the heap, for example, this will give a clear
// error on what would be segfaults in a native build (like dereferencing
// 0). See preamble.js for the actual checks performed.
var SAFE_HEAP = 0;

// Log out all SAFE_HEAP operations
var SAFE_HEAP_LOG = 0;

// In asm.js mode, we cannot simply add function pointers to function tables, so
// we reserve some slots for them. An alternative to this is to use
// EMULATED_FUNCTION_POINTERS, in which case we don't need to reserve.
// [fastcomp-only]
var RESERVED_FUNCTION_POINTERS = 0;

// Whether to allow function pointers to alias if they have a different type.
// This can greatly decrease table sizes in asm.js, but can break code that
// compares function pointers across different types.
// [fastcomp-only]
var ALIASING_FUNCTION_POINTERS = 0;

// asm.js: By default we implement function pointers using asm.js function
// tables, which is very fast. With this option, we implement them more flexibly
// by emulating them: we call out into JS, which handles the function tables.
//  1: Full emulation. This means you can modify the
//     table in JS fully dynamically, not just add to
//     the end.
//  2: Optimized emulation. Assumes once something is
//     added to the table, it will not change. This allows
//     dynamic linking while keeping performance fast,
//     as we can do a fast call into the internal table
//     if the fp is in the right range. Shared modules
//     (MAIN_MODULE, SIDE_MODULE) do this by default.
//     This requires RELOCATABLE to be set.
// wasm:
// By default we use a wasm Table for function pointers, which is fast and
// efficient. When enabling emulation, we also use the Table *outside* the wasm
// module, exactly as when emulating in asm.js, just replacing the plain JS
// array with a Table.
// [fastcomp-only]
var EMULATED_FUNCTION_POINTERS = 0;

// Allows function pointers to be cast, wraps each call of an incorrect type
// with a runtime correction.  This adds overhead and should not be used
// normally.  It also forces ALIASING_FUNCTION_POINTERS to 0.  Aside from making
// calls not fail, this tries to convert values as best it can. In asm.js, this
// uses doubles as the JS number type, so if you send a double to a parameter
// accepting an int, it will be |0-d into a (signed) int. In wasm, we have i64s
// so that is not valid, and instead we use 64 bits to represent values, as if
// we wrote the sent value to memory and loaded the received type from the same
// memory (using truncs/extends/ reinterprets). This means that when types do
// not match the emulated values may differ between asm.js and wasm (and native,
// for that matter - this is all undefined behavior). In any case, both
// approaches appear good enough to support Python, which is the main use case
// motivating this feature.
var EMULATE_FUNCTION_POINTER_CASTS = 0;

// Print out exceptions in emscriptened code. Does not work in asm.js mode
var EXCEPTION_DEBUG = 0;

// If 1, build in libcxxabi's full c++ demangling code, to allow stackTrace()
// to emit fully proper demangled c++ names
var DEMANGLE_SUPPORT = 0;

// Print out when we enter a library call (library*.js). You can also unset
// Runtime.debug at runtime for logging to cease, and can set it when you want
// it back. A simple way to set it in C++ is
//   emscripten_run_script("Runtime.debug = ...;");
var LIBRARY_DEBUG = 0;

// Print out all musl syscalls, including translating their numeric index
// to the string name, which can be convenient for debugging. (Other system
// calls are not numbered and already have clear names; use LIBRARY_DEBUG
// to get logging for all of them.)
var SYSCALL_DEBUG = 0;

// Log out socket/network data transfer.
var SOCKET_DEBUG = 0;

// Select socket backend, either webrtc or websockets. XXX webrtc is not
// currently tested, may be broken

// As well as being configurable at compile time via the "-s" option the
// WEBSOCKET_URL and WEBSOCKET_SUBPROTOCOL
// settings may configured at run time via the Module object e.g.
// Module['websocket'] = {subprotocol: 'base64, binary, text'};
// Module['websocket'] = {url: 'wss://', subprotocol: 'base64'};
// You can set 'subprotocol' to null, if you don't want to specify it
// Run time configuration may be useful as it lets an application select
// multiple different services.
var SOCKET_WEBRTC = 0;

// A string containing either a WebSocket URL prefix (ws:// or wss://) or a complete
// RFC 6455 URL - "ws[s]:" "//" host [ ":" port ] path [ "?" query ].
// In the (default) case of only a prefix being specified the URL will be constructed from
// prefix + addr + ':' + port
// where addr and port are derived from the socket connect/bind/accept calls.
var WEBSOCKET_URL = 'ws://';

// If 1, the POSIX sockets API uses a native bridge process server to proxy sockets calls
// from browser to native world.
var PROXY_POSIX_SOCKETS = 0;

// A string containing a comma separated list of WebSocket subprotocols
// as would be present in the Sec-WebSocket-Protocol header.
// You can set 'null', if you don't want to specify it.
var WEBSOCKET_SUBPROTOCOL = 'binary';

// Print out debugging information from our OpenAL implementation.
var OPENAL_DEBUG = 0;

// If 1, prints out debugging related to calls from emscripten_web_socket_* functions
// in emscripten/websocket.h.
// If 2, additionally traces bytes communicated via the sockets.
var WEBSOCKET_DEBUG = 0;

// Adds extra checks for error situations in the GL library. Can impact
// performance.
var GL_ASSERTIONS = 0;

// If enabled, prints out all API calls to WebGL contexts. (*very* verbose)
var TRACE_WEBGL_CALLS = 0;

// Enables more verbose debug printing of WebGL related operations. As with
// LIBRARY_DEBUG, this is toggleable at runtime with option GL.debug.
var GL_DEBUG = 0;

// When enabled, sets preserveDrawingBuffer in the context, to allow tests to
// work (but adds overhead)
var GL_TESTING = 0;

// How large GL emulation temp buffers are
var GL_MAX_TEMP_BUFFER_SIZE = 2097152;

// Enables some potentially-unsafe optimizations in GL emulation code
var GL_UNSAFE_OPTS = 1;

// Forces support for all GLES2 features, not just the WebGL-friendly subset.
var FULL_ES2 = 0;

// If true, glGetString() for GL_VERSION and GL_SHADING_LANGUAGE_VERSION will
// return strings OpenGL ES format "Open GL ES ... (WebGL ...)" rather than the
// WebGL format. If false, the direct WebGL format strings are returned. Set
// this to true to make GL contexts appear like an OpenGL ES context in these
// version strings (at the expense of a little bit of added code size), and to
// false to make GL contexts appear like WebGL contexts and to save some bytes
// from the output.
var GL_EMULATE_GLES_VERSION_STRING_FORMAT = 1;

// If true, all GL extensions are advertised in both unprefixed WebGL extension
// format, but also in desktop/mobile GLES/GL extension format with "GL_" prefix.
var GL_EXTENSIONS_IN_PREFIXED_FORMAT = 1;

// If true, adds support for automatically enabling all GL extensions for
// GLES/GL emulation purposes. This takes up code size. If you set this to 0,
// you will need to manually enable the extensions you need.
var GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS = 1;

// If true, the function emscripten_webgl_enable_extension() can be called to
// enable any WebGL extension. If false, to save code size,
// emscripten_webgl_enable_extension() cannot be called to enable any of extensions
// 'ANGLE_instanced_arrays', 'OES_vertex_array_object', 'WEBGL_draw_buffers' or
// 'WEBGL_draw_instanced_base_vertex_base_instance', but the dedicated functions
// emscripten_webgl_enable_*() found in html5.h are used to enable each of those
// extensions. This way code size is increased only for the extensions that are
// actually used.
// N.B. if setting this to 0, GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS must be set
// to zero as well.
var GL_SUPPORT_SIMPLE_ENABLE_EXTENSIONS = 1;

// If set to 0, Emscripten GLES2->WebGL translation layer does not track the kind
// of GL errors that exist in GLES2 but do not exist in WebGL. Settings this to 0
// saves code size. (Good to keep at 1 for development)
var GL_TRACK_ERRORS = 1;

// If true, GL contexts support the explicitSwapControl context creation flag.
// Set to 0 to save a little bit of space on projects that do not need it.
var GL_SUPPORT_EXPLICIT_SWAP_CONTROL = 0;

// If true, calls to glUniform*fv and glUniformMatrix*fv utilize a pool of
// preallocated temporary buffers for common small sizes to avoid generating
// temporary garbage for WebGL 1. Disable this to optimize generated size of the
// GL library a little bit, at the expense of generating garbage in WebGL 1. If
// you are only using WebGL 2 and do not support WebGL 1, this is not needed and
// you can turn it off.
var GL_POOL_TEMP_BUFFERS = 1;

// Some old Android WeChat (Chromium 37?) browser has a WebGL bug that it ignores
// the offset of a typed array view pointing to an ArrayBuffer. Set this to
// 1 to enable a polyfill that works around the issue when it appears. This
// bug is only relevant to WebGL 1, the affected browsers do not support WebGL 2.
var WORKAROUND_OLD_WEBGL_UNIFORM_UPLOAD_IGNORED_OFFSET_BUG = 0;

// Deprecated. Pass -s MAX_WEBGL_VERSION=2 to target WebGL 2.0.
var USE_WEBGL2 = 0;

// Specifies the lowest WebGL version to target. Pass -s MIN_WEBGL_VERSION=1
// to enable targeting WebGL 1, and -s MIN_WEBGL_VERSION=2 to drop support
// for WebGL 1.0
var MIN_WEBGL_VERSION = 1;

// Specifies the highest WebGL version to target. Pass -s MAX_WEBGL_VERSION=2
// to enable targeting WebGL 2. If WebGL 2 is enabled, some APIs (EGL, GLUT, SDL)
// will default to creating a WebGL 2 context if no version is specified.
var MAX_WEBGL_VERSION = 1;

// If true, emulates some WebGL 1 features on WebGL 2 contexts, meaning that
// applications that use WebGL 1/GLES 2 can initialize a WebGL 2/GLES3 context,
// but still keep using WebGL1/GLES 2 functionality that no longer is supported
// in WebGL2/GLES3. Currently this emulates GL_EXT_shader_texture_lod extension
// in GLSLES 1.00 shaders, support for unsized internal texture formats, and the
// GL_HALF_FLOAT_OES != GL_HALF_FLOAT mixup.
var WEBGL2_BACKWARDS_COMPATIBILITY_EMULATION = 0;

// Forces support for all GLES3 features, not just the WebGL2-friendly subset.
// This automatically turns on FULL_ES2 and WebGL2 support.
var FULL_ES3 = 0;

// Includes code to emulate various desktop GL features. Incomplete but useful
// in some cases, see
// http://kripken.github.io/emscripten-site/docs/porting/multimedia_and_graphics/OpenGL-support.html
var LEGACY_GL_EMULATION = 0;

// If you specified LEGACY_GL_EMULATION = 1 and only use fixed function pipeline
// in your code, you can also set this to 1 to signal the GL emulation layer
// that it can perform extra optimizations by knowing that the user code does
// not use shaders at all. If LEGACY_GL_EMULATION = 0, this setting has no
// effect.
var GL_FFP_ONLY = 0;

// If you want to create the WebGL context up front in JS code, set this to 1
// and set Module['preinitializedWebGLContext'] to a precreated WebGL context.
// WebGL initialization afterwards will use this GL context to render.
var GL_PREINITIALIZED_CONTEXT = 0;

// Enables support for WebGPU (via "webgpu/webgpu.h").
var USE_WEBGPU = 0;

// Enables building of stb-image, a tiny public-domain library for decoding
// images, allowing decoding of images without using the browser's built-in
// decoders. The benefit is that this can be done synchronously, however, it
// will not be as fast as the browser itself.  When enabled, stb-image will be
// used automatically from IMG_Load and IMG_Load_RW. You can also call the
// stbi_* functions directly yourself.
var STB_IMAGE = 0;

// If WORKAROUND_IOS_9_RIGHT_SHIFT_BUG==1, work around Safari/WebKit bug in iOS 9.3.5: https://bugs.webkit.org/show_bug.cgi?id=151514 where computing "a >> b" or "a >>> b" in
// JavaScript would erroneously output 0 when a!=0 and b==0, after suitable JIT compiler optimizations have been applied to a function at runtime (bug does not
// occur in debug builds). Fix was landed in https://trac.webkit.org/changeset/196591/webkit on Feb 15th 2016. iOS 9.3.5 was released on August 25 2016, but
// oddly did not have the fix. iOS Safari 10.3.3 was released on July 19 2017, that no longer has the issue. Unknown which released version between these was the
// first to contain the fix, though notable is that iOS 9.3.5 and iOS 10.3.3 are the two consecutive "end-of-life" versions of iOS that users are likely
// to be on, e.g. iPhone 4s, iPad 2, iPad 3, iPad Mini 1, Pod Touch 5 all had end-of-life at iOS 9.3.5 (tested to be affected),
// and iPad 4, iPhone 5 and iPhone 5c all had end-of-life at iOS 10.3.3 (confirmed not affected).
// If you do not care about old iOS 9 support, keep this disabled.
var WORKAROUND_IOS_9_RIGHT_SHIFT_BUG = 0;

// From Safari 8 (where WebGL was introduced to Safari) onwards, OES_texture_half_float and OES_texture_half_float_linear extensions
// are broken and do not function correctly, when used as source textures.
// See https://bugs.webkit.org/show_bug.cgi?id=183321, https://bugs.webkit.org/show_bug.cgi?id=169999,
// https://stackoverflow.com/questions/54248633/cannot-create-half-float-oes-texture-from-uint16array-on-ipad
var GL_DISABLE_HALF_FLOAT_EXTENSION_IF_BROKEN = 0;

// Use JavaScript math functions like Math.tan. This saves code size as we can avoid shipping
// compiled musl code. However, it can be significantly slower as it calls out to JS. It
// also may give different results as JS math is specced somewhat differently than libc, and
// can also vary between browsers.
// [upstream-only]
var JS_MATH = 0;

// If set, enables polyfilling for Math.clz32, Math.trunc, Math.imul, Math.fround.
var POLYFILL_OLD_MATH_FUNCTIONS = 0;

// Set this to enable compatibility emulations for old JavaScript engines. This gives you
// the highest possible probability of the code working everywhere, even in rare old
// browsers and shell environments. Specifically:
//  * Add polyfilling for Math.clz32, Math.trunc, Math.imul, Math.fround. (-s POLYFILL_OLD_MATH_FUNCTIONS=1)
//  * Work around iOS 9 right shift bug (-s WORKAROUND_IOS_9_RIGHT_SHIFT_BUG=1)
//  * Work around old Chromium WebGL 1 bug (-s WORKAROUND_OLD_WEBGL_UNIFORM_UPLOAD_IGNORED_OFFSET_BUG=1)
//  * Disable WebAssembly. (Must be paired with -s WASM=0)
//  * Adjusts MIN_X_VERSION settings to 0 to include support for all browser versions.
// You can also configure the above options individually.
var LEGACY_VM_SUPPORT = 0;

// By default, emscripten output will run on the web, in a web worker,
// in node.js, or in a JS shell like d8, js, or jsc. You can set this option to
// specify that the output should only run in one particular environment, which
// must be one of
//    'web'     - the normal web environment.
//    'webview' - just like web, but in a webview like Cordova;
//                considered to be same as "web" in almost every place
//    'worker'  - a web worker environment.
//    'node'    - Node.js.
//    'shell'   - a JS shell like d8, js, or jsc.
// Or it can be a comma-separated list of them, e.g., "web,worker". If this is
// the empty string, then all runtime environments are supported.
//
// Note that the set of environments recognized here is not identical to the
// ones we identify at runtime using ENVIRONMENT_IS_*. Specifically:
//  * We detect whether we are a pthread at runtime, but that's set for workers
//    and not for the main file so it wouldn't make sense to specify here.
//  * The webview target is basically a subset of web. It must be specified
//    alongside web (e.g. "web,webview") and we only use it for code generation
//    at compile time, there is no runtime behavior change.
var ENVIRONMENT = '';

// Enable this to support lz4-compressed file packages. They are stored compressed in memory, and
// decompressed on the fly, avoiding storing the entire decompressed data in memory at once.
// If you run the file packager separately, you still need to build the main program with this flag,
// and also pass --lz4 to the file packager.
// (You can also manually compress one on the client, using LZ4.loadPackage(), but that is less
// recommended.)
// Limitations:
//   * LZ4-compressed files are only decompressed when needed, so they are not available
//     for special preloading operations like pre-decoding of images using browser codecs,
//     preloadPlugin stuff, etc.
//   * LZ4 files are read-only.
var LZ4 = 0;

// Emscripten exception handling options.
// These options only pertain to Emscripten exception handling and do not
// control the experimental native wasm exception handling option.

// Disables generating code to actually catch exceptions. This disabling is on
// by default as the overhead of exceptions is quite high in size and speed
// currently (in the future, wasm should improve that). When exceptions are
// disabled, if an exception actually happens then it will not be caught
// and the program will halt (so this will not introduce silent failures).
// There are 3 specific modes here:
// DISABLE_EXCEPTION_CATCHING = 0 - generate code to actually catch exceptions
// DISABLE_EXCEPTION_CATCHING = 1 - disable exception catching at all
// DISABLE_EXCEPTION_CATCHING = 2 - disable exception catching, but enables
//                                  catching in whitelist
// XXX note that this removes *catching* of exceptions, which is the main
//     issue for speed, but you should build source files with
//     -fno-exceptions to really get rid of all exceptions code overhead,
//     as it may contain thrown exceptions that are never caught (e.g.
//     just using std::vector can have that). -fno-rtti may help as well.
//
// [compile+link] - affects user code at compile and system libraries at link
var DISABLE_EXCEPTION_CATCHING = 1;

// Enables catching exception in the listed functions only, if
// DISABLE_EXCEPTION_CATCHING = 2 is set
// [compile+link] - affects user code at compile and system libraries at link
var EXCEPTION_CATCHING_WHITELIST = [];

// By default we handle exit() in node, by catching the Exit exception. However,
// this means we catch all process exceptions. If you disable this, then we no
// longer do that, and exceptions work normally, which can be useful for libraries
// or programs that don't need exit() to work.

// Emscripten uses an ExitStatus exception to halt when exit() is called.
// With this option, we prevent that from showing up as an unhandled
// exception.
var NODEJS_CATCH_EXIT = 1;

// Catch unhandled rejections in node. Without this, node may print the error,
// and that this behavior will change in future node, wait a few seconds, and
// then exit with 0 (which hides the error if you don't read the log). With
// this, we catch any unhandled rejection and throw an actual error, which will
// make the process exit immediately with a non-0 return code.
var NODEJS_CATCH_REJECTION = 1;

// Whether to enable asyncify transformation
// This allows to inject some async functions to the C code that appear to be sync
// e.g. emscripten_sleep
// On fastcomp this uses the Asyncify IR transform.
// On upstream this uses the Asyncify pass in Binaryen. TODO: whitelist, coroutines
var ASYNCIFY = 0;

// Imports which can do a sync operation, in addition to the default ones that
// emscripten defines like emscripten_sleep. If you add more you will need to
// mention them to here, or else they will not work (in ASSERTIONS builds an
// error will be shown).
// Note that this list used to contain the default ones, which meant that you
// had to list them when adding your own; the default ones are now added
// automatically.
var ASYNCIFY_IMPORTS = [];

// Whether indirect calls can be on the stack during an unwind/rewind.
// If you know they cannot, then setting this can be extremely helpful, as otherwise asyncify
// must assume an indirect call can reach almost everywhere.
var ASYNCIFY_IGNORE_INDIRECT = 0;

// The size of the asyncify stack - the region used to store unwind/rewind
// info. This must be large enough to store the call stack and locals. If it is too
// small, you will see a wasm trap due to executing an "unreachable" instruction.
// In that case, you should increase this size.
var ASYNCIFY_STACK_SIZE = 4096;

// If the Asyncify blacklist is provided, then the functions in it will not
// be instrumented even if it looks like they need to. This can be useful
// if you know things the whole-program analysis doesn't, like if you
// know certain indirect calls are safe and won't unwind. But if you
// get the list wrong things will break (and in a production build user
// input might reach code paths you missed during testing, so it's hard
// to know you got this right), so this is not recommended unless you
// really know what are doing, and need to optimize every bit of speed
// and size.
// The names in this list are names from the WebAssembly Names section. The
// wasm backend will emit those names in *human-readable* form instead of
// typical C++ mangling. For example, you should write Struct::func()
// instead of _ZN6Struct4FuncEv. C is also different from C++, as C
// names don't end with parameters; as a result foo(int) in C++ would appear
// as just foo in C (C++ has parameters because it needs to differentiate
// overloaded functions). You will see warnings in the console if a name in the
// list is missing (these are not errors because inlining etc. may cause
// changes which would mean a single list couldn't work for both -O0 and -O1
// builds, etc.). You can inspect the wasm binary to look for the actual names,
// either directly or using wasm-objdump or wasm-dis, etc.
// Simple '*' wildcard matching is supported.
var ASYNCIFY_BLACKLIST = [];

// If the Asyncify whitelist is provided, then *only* the functions in the list
// will be instrumented. Like the blacklist, getting this wrong will break
// your application.
// See notes on ASYNCIFY_BLACKLIST about the names.
var ASYNCIFY_WHITELIST = [];

// Allows lazy code loading: where emscripten_lazy_load_code() is written, we
// will pause execution, load the rest of the code, and then resume.
var ASYNCIFY_LAZY_LOAD_CODE = 0;

// Runtime debug logging from asyncify internals.
var ASYNCIFY_DEBUG = 0;

// Runtime elements that are exported on Module by default. We used to export
// quite a lot here, but have removed them all, so this option is redundant
// given that EXTRA_EXPORTED_RUNTIME_METHODS exists, and so this option exists
// only for backwards compatibility. You should use
// EXTRA_EXPORTED_RUNTIME_METHODS for things you want to export from the
// runtime.  Note that methods on this list are only exported if they are
// included (either automatically from linking, or due to being in
// DEFAULT_LIBRARY_FUNCS_TO_INCLUDE).
// Note that the name may be slightly misleading, as this is for any JS library
// element, and not just methods. For example, we can export the FS object by
// having "FS" in this list.
var EXPORTED_RUNTIME_METHODS = [];

// Additional methods to those in EXPORTED_RUNTIME_METHODS. Adjusting that list
// lets you remove methods that would be exported by default; setting values in
// this list lets you add to the default list without modifying it.
var EXTRA_EXPORTED_RUNTIME_METHODS = [];

// A list of incoming values on the Module object in JS that we care about. If
// a value is not in this list, then we don't emit code to check if you provide
// it on the Module object. For example, if
// you have this:
//
//  var Module = {
//    print: function(x) { console.log('print: ' + x) },
//    preRun: [function() { console.log('pre run') }]
//  };
//
// Then MODULE_JS_API must contain 'print' and 'preRun'; if it does not then
// we may not emit code to read and use that value. In other words, this
// option lets you set, statically at compile time, the list of which Module
// JS values you will be providing at runtime, so the compiler can better
// optimize.
//
// Setting this list to [], or at least a short and concise set of names you
// actually use, can be very useful for reducing code size. By default the
// list contains all the possible APIs.
//
// FIXME: should this just be  0  if we want everything?
var INCOMING_MODULE_JS_API = [
  'ENVIRONMENT', 'GL_MAX_TEXTURE_IMAGE_UNITS', 'SDL_canPlayWithWebAudio',
  'SDL_numSimultaneouslyQueuedBuffers', 'INITIAL_MEMORY', 'wasmMemory', 'arguments',
  'buffer', 'canvas', 'doNotCaptureKeyboard', 'dynamicLibraries',
  'elementPointerLock', 'extraStackTrace', 'forcedAspectRatio',
  'instantiateWasm', 'keyboardListeningElementfreePreloadedMediaOnUse',
  'locateFile', 'logReadFiles', 'mainScriptUrlOrBlob', 'mem',
  'monitorRunDependencies', 'noExitRuntime', 'noInitialRun', 'onAbort',
  'onCustomMessage', 'onExit', 'onFree', 'onFullScreen', 'onMalloc',
  'onRealloc', 'onRuntimeInitialized', 'postMainLoop', 'postRun', 'preInit',
  'preMainLoop', 'preRun',
  'preinitializedWebGLContextmemoryInitializerRequest', 'preloadPlugins',
  'print', 'printErr', 'quit', 'setStatus', 'statusMessage', 'stderr',
  'stdin', 'stdout', 'thisProgram', 'wasm', 'wasmBinary', 'websocket'
];

// If set to nonzero, the provided virtual filesystem if treated
// case-insensitive, like Windows and macOS do. If set to 0, the VFS is
// case-sensitive, like on Linux.
var CASE_INSENSITIVE_FS = 0;

// If set to 0, does not build in any filesystem support. Useful if you are just
// doing pure computation, but not reading files or using any streams (including
// fprintf, and other stdio.h things) or anything related. The one exception is
// there is partial support for printf, and puts, hackishly.  The compiler will
// automatically set this if it detects that syscall usage (which is static)
// does not require a full filesystem. If you still want filesystem support, use
// FORCE_FILESYSTEM
var FILESYSTEM = 1;

// Makes full filesystem support be included, even if statically it looks like
// it is not used. For example, if your C code uses no files, but you include
// some JS that does, you might need this.
var FORCE_FILESYSTEM = 0;

// Enables support for the NODERAWFS filesystem backend. This is a special
// backend as it replaces all normal filesystem access with direct Node.js
// operations, without the need to do `FS.mount()`, and this backend only
// works with Node.js. The initial working directory will be same as
// process.cwd() instead of VFS root directory.  Because this mode directly uses
// Node.js to access the real local filesystem on your OS, the code will not
// necessarily be portable between OSes - it will be as portable as a Node.js
// program would be, which means that differences in how the underlying OS
// handles permissions and errors and so forth may be noticeable.  This has
// mostly been tested on Linux so far.
var NODERAWFS = 0;

// This saves the compiled wasm module in a file with name
//   $WASM_BINARY_NAME.$V8_VERSION.cached
// and loads it on subsequent runs. This caches the compiled wasm code from
// v8 in node, which saves compiling on subsequent runs, making them start up
// much faster.
// The V8 version used in node is included in the cache name so that we don't
// try to load cached code from another version, which fails silently (it seems
// to load ok, but we do actually recompile).
//  * The only version known to work for sure is node 12.9.1, as this has
//    regressed, see
//    https://github.com/nodejs/node/issues/18265#issuecomment-622971547
//  * The default location of the .cached files is alongside the wasm binary,
//    as mentioned earlier. If that is in a read-only directory, you may need
//    to place them elsewhere. You can use the locateFile() hook to do so.
var NODE_CODE_CACHING = 0;

// Functions that are explicitly exported. These functions are kept alive
// through LLVM dead code elimination, and also made accessible outside of the
// generated code even after running closure compiler (on "Module").  Note the
// necessary prefix of "_".
//
// Note also that this is the full list of exported functions - if you have a
// main() function and want it to run, you must include it in this list (as
// _main is by default in this value, and if you override it without keeping it
// there, you are in effect removing it).
var EXPORTED_FUNCTIONS = ['_main'];

// If true, we export all the symbols that are present in JS onto the Module
// object. This does not affect which symbols will be present - it does not
// prevent DCE or cause anything to be included in linking. It only does
//   Module['X'] = X;
// for all X that end up in the JS file. This is useful to export the JS
// library functions on Module, for things like dynamic linking.
var EXPORT_ALL = 0;

// Export all bindings generator functions (prefixed with emscripten_bind_). This
// is necessary to use the WebIDL binder with asm.js
var EXPORT_BINDINGS = 0;

// If true, export all the functions appearing in a function table, and the
// tables themselves.
var EXPORT_FUNCTION_TABLES = 0;

// Remembers the values of these settings, and makes them accessible
// through Runtime.getCompilerSetting and emscripten_get_compiler_setting.
// To see what is retained, look for compilerSettings in the generated code.
var RETAIN_COMPILER_SETTINGS = 0;

// JS library elements (C functions implemented in JS) that we include by
// default. If you want to make sure something is included by the JS compiler,
// add it here.  For example, if you do not use some emscripten_* C API call
// from C, but you want to call it from JS, add it here (and in EXPORTED
// FUNCTIONS with prefix "_", if you use closure compiler).  Note that the name
// may be slightly misleading, as this is for any JS library element, and not
// just functions. For example, you can include the Browser object by adding
// "$Browser" to this list.
var DEFAULT_LIBRARY_FUNCS_TO_INCLUDE = [];

// This list is also used to determine auto-exporting of library dependencies
// (i.e., functions that might be dependencies of JS library functions, that if
// so we must export so that if they are implemented in C they will be
// accessible, in ASM_JS mode).
var LIBRARY_DEPS_TO_AUTOEXPORT = ['memcpy'];

// Include all JS library functions instead of the sum of
// DEFAULT_LIBRARY_FUNCS_TO_INCLUDE + any functions used by the generated code.
// This is needed when dynamically loading (i.e. dlopen) modules that make use
// of runtime library functions that are not used in the main module.  Note that
// this only applies to js libraries, *not* C. You will need the main file to
// include all needed C libraries.  For example, if a module uses malloc or new,
// you will need to use those in the main file too to pull in malloc for use by
// the module.
var INCLUDE_FULL_LIBRARY = 0;

// Set this to a string to override the shell file used
var SHELL_FILE = 0;

// If set to 1, we emit relocatable code from the LLVM backend; both
// globals and function pointers are all offset (by gb and fp, respectively)
// Automatically set for SIDE_MODULE or MAIN_MODULE.
var RELOCATABLE = 0;

// A main module is a file compiled in a way that allows us to link it to
// a side module using emlink.py.
//  1: Normal main module.
//  2: DCE'd main module. We eliminate dead code normally. If a side
//     module needs something from main, it is up to you to make sure
//     it is kept alive.
var MAIN_MODULE = 0;

// Corresponds to MAIN_MODULE (also supports modes 1 and 2)
var SIDE_MODULE = 0;

// If this is a shared object (MAIN_MODULE == 1 || SIDE_MODULE == 1), then we
// will link these at runtime. They must have been built with SIDE_MODULE == 1.
var RUNTIME_LINKED_LIBS = [];

// If set to 1, this is a worker library, a special kind of library that is run
// in a worker. See emscripten.h
var BUILD_AS_WORKER = 0;

// If set to 1, we build the project into a js file that will run in a worker,
// and generate an html file that proxies input and output to/from it.
var PROXY_TO_WORKER = 0;

// If set, the script file name the main thread loads.  Useful if your project
// doesn't run the main emscripten- generated script immediately but does some
// setup before
var PROXY_TO_WORKER_FILENAME = '';

// If set to 1, compiles in a small stub main() in between the real main() which
// calls pthread_create() to run the application main() in a pthread.  This is
// something that applications can do manually as well if they wish, this option
// is provided as convenience.
//
// This proxies Module['canvas'], if present, and if OFFSCREEN_CANVAS support
// is enabled. This has to happen because this is the only chance - this browser
// main thread does the only pthread_create call that happens on
// that thread, so it's the only chance to transfer the canvas from there.
var PROXY_TO_PTHREAD = 0;

// If set to 1, this file can be linked with others, either as a shared library
// or as the main file that calls a shared library. To enable that, we will not
// internalize all symbols and cull the unused ones, in other words, we will not
// remove unused functions and globals, which might be used by another module we
// are linked with.
//
// MAIN_MODULE and SIDE_MODULE both imply this, so it not normally necessary
// to set this explicitly. Note that MAIN_MODULE and SIDE_MODULE mode 2 do
// *not* set this, so that we still do normal DCE on them, and in that case
// you must keep relevant things alive yourself using exporting.
var LINKABLE = 0;

// Emscripten 'strict' build mode: Drop supporting any deprecated build options.
// Set the environment variable EMCC_STRICT=1 or pass -s STRICT=1 to test that a
// codebase builds nicely in forward compatible manner.
// Changes enabled by this:
//   * The C define EMSCRIPTEN is not defined (__EMSCRIPTEN__ always is, and
//     is the correct thing to use).
//   * STRICT_JS is enabled.
//   * IGNORE_MISSING_MAIN is disabled.
//   * AUTO_JS_LIBRARIES is disabled.
//   * AUTO_ARCHIVE_INDEXES is disabled.
//   * DEFAULT_TO_CXX is disabled.
// [compile+link]
var STRICT = 0;

// Allow program to link with or without `main` symbol.
// If this is disabled then one must provide a `main` symbol or explicitly
// opt out by passing `--no-entry` or an EXPORTED_FUNCTIONS list that doesn't
// include `_main`.
var IGNORE_MISSING_MAIN = 1;

// Automatically attempt to add archive indexes at link time to archives that 
// don't already have them.  This can happen when GNU ar or GNU ranlib is used
// rather than `llvm-ar` or `emar` since the former don't understand the wasm
// object format.
// [link]
var AUTO_ARCHIVE_INDEXES = 1;

// Add "use strict;" to generated JS
var STRICT_JS = 0;

// If set to 1, we will warn on any undefined symbols that are not resolved by
// the library_*.js files. Note that it is common in large projects to not
// implement everything, when you know what is not going to actually be called
// (and don't want to mess with the existing buildsystem), and functions might
// be implemented later on, say in --pre-js, so you may want to build with -s
// WARN_ON_UNDEFINED_SYMBOLS=0 to disable the warnings if they annoy you.  See
// also ERROR_ON_UNDEFINED_SYMBOLS.  Any undefined symbols that are listed in-
// EXPORTED_FUNCTIONS will also be reported.
var WARN_ON_UNDEFINED_SYMBOLS = 1;

// If set to 1, we will give a link-time error on any undefined symbols (see
// WARN_ON_UNDEFINED_SYMBOLS). To allow undefined symbols at link time set this
// to 0, in which case if an undefined function is called a runtime error will
// occur.  Any undefined symbols that are listed in EXPORTED_FUNCTIONS will also
// be reported.
var ERROR_ON_UNDEFINED_SYMBOLS = 1;

// Use small chunk size for binary synchronous XHR's in Web Workers.  Used for
// testing.  See test_chunked_synchronous_xhr in runner.py and library.js.
var SMALL_XHR_CHUNKS = 0;

// If 1, will include shim code that tries to 'fake' a browser environment, in
// order to let you run a browser program (say, using SDL) in the shell.
// Obviously nothing is rendered, but this can be useful for benchmarking and
// debugging if actual rendering is not the issue. Note that the shim code is
// very partial - it is hard to fake a whole browser! - so keep your
// expectations low for this to work.
var HEADLESS = 0;

// If 1, we force Date.now(), Math.random, etc. to return deterministic results.
// Good for comparing builds for debugging purposes (and nothing else)
var DETERMINISTIC = 0;

// By default we emit all code in a straightforward way into the output
// .js file. That means that if you load that in a script tag in a web
// page, it will use the global scope. With `MODULARIZE` set, we instead emit
// the code wrapped in a function that returns a promise. The promise is
// resolved with the module instance when it is safe to run the compiled code,
// similar to the `onRuntimeInitialized` callback. You do not need to use the
// `onRuntimeInitialized` callback when using `MODULARIZE`.
// 
// The default name of the function is `Module`, but can be changed using the
// `EXPORT_NAME` option. We recommend renaming it to a more typical name for a
// factory function, e.g. `createModule`.
//
//
// You use the factory function like so:
//
//   const module = await EXPORT_NAME();
//   
// or:
//
//   let module;
//   EXPORT_NAME().then(instance => {
//     module = instance;
//   });
//   
//
// The factory function accepts 1 parameter, an object with default values for
// the module instance:
//
//   const module = await EXPORT_NAME({ option: value, ... });
//
// Note the parentheses - we are calling EXPORT_NAME in order to instantiate
// the module. This allows you to create multiple instances of the module.
//
// Note that in MODULARIZE mode we do *not* look for a global `Module` object
// for default values. Default values must be passed as a parameter to the
// factory function.
//
// The default .html shell file provided in MINIMAL_RUNTIME mode will create
// a singleton instance automatically, to run the application on the page.
// (Note that it does so without using the Promise API mentioned earlier, and
// so code for the Promise is not even emitted in the .js file if you tell
// emcc to emit an .html output.)
// The default .html shell file provided by traditional runtime mode is only
// compatible with MODULARIZE=0 mode, so when building with traditional
// runtime, you should provided your own html shell file to perform the
// instantiation when building with MODULARIZE=1. (For more details, see
// https://github.com/emscripten-core/emscripten/issues/7950)
//
// If you add --pre-js or --post-js files, they will be included inside
// the factory function with the rest of the emitted code in order to be
// optimized together with it.
//
// If you want to include code outside all of the generated code, including the
// factory function, you can use --extern-pre-js or --extern-post-js. While
// --pre-js and --post-js happen to do that in non-MODULARIZE mode, their
// intended usage is to add code that is optimized with the rest of the emitted
// code, allowing better dead code elimination and minification.
var MODULARIZE = 0;

// If we separate out asm.js with the --separate-asm option,
// this is the name of the variable where the generated asm.js
// Module is assigned to. This name can either be a property
// of Module, or a freestanding variable name, like "var asmJs".
// If you are XHRing in multiple asm.js built files, use this option to
// assign the generated asm.js modules to different variable names
// so that they do not conflict. Default name is 'Module["asm"]' if a custom
// name is not passed in.
var SEPARATE_ASM_MODULE_NAME = '';

// Export using an ES6 Module export rather than a UMD export.  MODULARIZE must
// be enabled for ES6 exports.
var EXPORT_ES6 = 0;

// Use the ES6 Module relative import feature 'import.meta.url'
// to auto-detect WASM Module path.
// It might not be supported on old browsers / toolchains
var USE_ES6_IMPORT_META = 1;

// If 1, will just time how long main() takes to execute, and not print out
// anything at all whatsoever. This is useful for benchmarking.
var BENCHMARK = 0;

// If 1, generate code in asm.js format. If 2, emits the same code except for
// omitting 'use asm'.
// [fastcomp-only]
var ASM_JS = 1;

// If 1, will finalize the final emitted code, including operations that prevent
// later js optimizer passes from running, like converting +5 into 5.0 (the js
// optimizer sees 5.0 as just 5).
// [fastcomp-only]
var FINALIZE_ASM_JS = 1;

// see emcc --separate-asm
// [fastcomp-only]
var SEPARATE_ASM = 0;

// JS library functions on this list are not converted to JS, and calls to them
// are turned into abort()s. This is potentially useful for reducing code size.
// If a dead function is actually called, you will get a runtime error.
//
// TODO: make this work on compiled methods as well, perhaps by adding a JS
// optimizer pass?
var DEAD_FUNCTIONS = [];

// Global variable to export the module as for environments without a
// standardized module loading system (e.g. the browser and SM shell).
var EXPORT_NAME = 'Module';

// When set to 0, we do not emit eval() and new Function(), which disables some functionality
// (causing runtime errors if attempted to be used), but allows the emitted code to be
// acceptable in places that disallow dynamic code execution (chrome packaged app,
// privileged firefox app, etc.). Pass this flag when developing an Emscripten application
// that is targeting a privileged or a certified execution environment, see
// Firefox Content Security Policy (CSP) webpage for details:
// https://developer.mozilla.org/en-US/Apps/Build/Building_apps_for_Firefox_OS/CSP
// When this flag is set, the following features (linker flags) are unavailable:
//  --closure 1: When using closure compiler, eval() would be needed to locate the Module object.
//  -s RELOCATABLE=1: the function Runtime.loadDynamicLibrary would need to eval().
//  --bind: Embind would need to eval().
// Additionally, the following Emscripten runtime functions are unavailable when
// DYNAMIC_EXECUTION=0 is set, and an attempt to call them will throw an exception:
// - emscripten_run_script(),
// - emscripten_run_script_int(),
// - emscripten_run_script_string(),
// - dlopen(),
// - the functions ccall() and cwrap() are still available, but they are restricted to only
//   being able to call functions that have been exported in the Module object in advance.
// When set to -s DYNAMIC_EXECUTION=2 flag is set, attempts to call to eval() are demoted
// to warnings instead of throwing an exception.
var DYNAMIC_EXECUTION = 1;

// Runs tools/emterpretify on the compiler output.
// [fastcomp-only]
var EMTERPRETIFY = 0;

// If defined, a file to write bytecode to, otherwise the default is to embed it
// in text JS arrays (which is less efficient).  When emitting HTML, we
// automatically generate code to load this file and set it to
// Module.emterpreterFile. If you emit JS, you need to make sure that
// Module.emterpreterFile contains an ArrayBuffer with the bytecode, when the
// code loads.  Note: You might need to quote twice in the shell, something like
// -s 'EMTERPRETIFY_FILE="waka"'
// [fastcomp-only]
var EMTERPRETIFY_FILE = '';

// Functions to not emterpret, that is, to run normally at full speed
// [fastcomp-only]
var EMTERPRETIFY_BLACKLIST = [];

// If this contains any functions, then only the functions in this list are
// emterpreted (as if all the rest are blacklisted; this overrides the
// BLACKLIST)
// [fastcomp-only]
var EMTERPRETIFY_WHITELIST = [];

// Allows sync code in the emterpreter, by saving the call stack, doing an async
// delay, and resuming it
// [fastcomp-only]
var EMTERPRETIFY_ASYNC = 0;

// Performs a static analysis to suggest which functions should be run in the
// emterpreter, as it appears they can be on the stack when a sync function is
// called in the EMTERPRETIFY_ASYNC option.  After showing the suggested list,
// compilation will halt. You can apply the provided list as an emcc argument
// when compiling later.
// [fastcomp-only]
var EMTERPRETIFY_ADVISE = 0;

// If you have additional custom synchronous functions, add them to this list
// and the advise mode will include them in its analysis.
// [fastcomp-only]
var EMTERPRETIFY_SYNCLIST = [];

// whether js opts will be run, after the main compiler
var RUNNING_JS_OPTS = 0;

// whether we are in the generate struct_info bootstrap phase
var BOOTSTRAPPING_STRUCT_INFO = 0;

// struct_info that is either generated or cached
var STRUCT_INFO = '';

// Add some calls to emscripten tracing APIs
var EMSCRIPTEN_TRACING = 0;

// Specify the GLFW version that is being linked against.  Only relevant, if you
// are linking against the GLFW library.  Valid options are 2 for GLFW2 and 3
// for GLFW3.
var USE_GLFW = 2;

// Whether to use compile code to WebAssembly. Set this to 0 to compile to
// asm.js in fastcomp, or JS in upstream.
//
// Note that in upstream, WASM=0 behaves very similarly to WASM=1, in particular
// startup can be either async or sync, so flags like WASM_ASYNC_COMPILATION
// still make sense there, see that option for more details.
//
// Specify -s WASM=2 to target both WebAssembly and JavaScript at the same time.
// In that build mode, two files a.wasm and a.wasm.js are produced, and at runtime
// the WebAssembly file is loaded if browser/shell supports it. Otherwise the
// .wasm.js fallback will be used.
var WASM = 1;

// STANDALONE_WASM indicates that we want to emit a wasm file that can run without
// JavaScript. The file will use standard APIs such as wasi as much as possible
// to achieve that.
//
// This option does not guarantee that the wasm can be used by itself - if you
// use APIs with no non-JS alternative, we will still use those (e.g., OpenGL
// at the time of writing this). This gives you the option to see which APIs
// are missing, and if you are compiling for a custom wasi embedding, to add
// those to your embedding.
//
// We may still emit JS with this flag, but the JS should only be a convenient
// way to run the wasm on the Web or in Node.js, and you can run the wasm by
// itself without that JS (again, unless you use APIs for which there is no
// non-JS alternative) in a wasm runtime like wasmer or wasmtime.
//
// Note that even without this option we try to use wasi etc. syscalls as much
// as possible. What this option changes is that we do so even when it means
// a tradeoff with JS size. For example, when this option is set we do not
// import the Memory - importing it is useful for JS, so that JS can start to
// use it before the wasm is even loaded, but in wasi and other wasm-only
// environments the expectation is to create the memory in the wasm itself.
// Doing so prevents some possible JS optimizations, so we only do it behind
// this flag.
var STANDALONE_WASM = 0;

// Whether to use the WebAssembly backend that is in development in LLVM.  You
// should not set this yourself, instead set EMCC_WASM_BACKEND=1 in the
// environment.
var WASM_BACKEND = 0;

// An optional comma-separated list of script hooks to run after binaryen,
// in binaryen's /scripts dir.
var BINARYEN_SCRIPTS = "";

// Whether to ignore implicit traps when optimizing in binaryen.  Implicit
// traps are the traps that happen in a load that is out of bounds, or
// div/rem of 0, etc. With this option set, the optimizer assumes that loads
// cannot trap, and therefore that they have no side effects at all. This
// is *not* safe in general, as you may have a load behind a condition which
// ensures it it is safe; but if the load is assumed to not have side effects it
// could be executed unconditionally. For that reason this option is generally
// not useful on large and complex projects, but in a small and simple enough
// codebase it may help reduce code size a little bit.
var BINARYEN_IGNORE_IMPLICIT_TRAPS = 0;

// How we handle wasm operations that may trap, which includes integer
// div/rem of 0 and float-to-int of values too large to fit in an int.
//   js: do exactly what js does. this can be slower.
//   clamp: avoid traps by clamping to a reasonable value. this can be
//          faster than "js".
//   allow: allow creating operations that can trap. this is the most
//          compact, as we just emit a single wasm operation, with no
//          guards to trapping values, and also often the fastest.
// [fastcomp-only]
var BINARYEN_TRAP_MODE = "allow";

// A comma-separated list of extra passes to run in the binaryen optimizer,
// Setting this does not override/replace the default passes. It is appended at
// the end of the list of passes.
var BINARYEN_EXTRA_PASSES = "";

// Whether to compile the wasm asynchronously, which is more efficient and does
// not block the main thread. This is currently required for all but the
// smallest modules to run in chrome.
//
// Note that this flag is still useful even if WASM=0 when using the upstream
// backend, as startup behaves the same there as WASM=1 (the implementation is
// of a fake WebAssembly.* object, so the startup code doesn't know it's JS
// and not wasm). That makes it easier to swap between JS and wasm builds,
// however, this is a difference from fastcomp in which WASM=0 always meant
// sync startup as asm.js (unless a mem init file was used or some other thing
// that forced async).
//
// (This option was formerly called BINARYEN_ASYNC_COMPILATION)
var WASM_ASYNC_COMPILATION = 1;

// WebAssembly integration with JavaScript BigInt. When enabled we don't need
// to legalize i64s into pairs of i32s, as the wasm VM will use a BigInt where
// an i64 is used.
var WASM_BIGINT = 0;

// WebAssembly defines a "producers section" which compilers and tools can
// annotate themselves in. Emscripten does not emit this by default, as it
// increases code size, and some users may not want information about their tools
// to be included in their builds for privacy or security reasons, see
// https://github.com/WebAssembly/tool-conventions/issues/93.
// TODO: currently this flag just controls whether we run the binaryen pass
//       to strip it out from the wasm (where the LLVM wasm backend may have
//       created it)
var EMIT_PRODUCERS_SECTION = 0;

// If set then generated WASM files will contain a custom
// "emscripten_metadata" section that contains information necessary
// to execute the file without the accompanying JS file.
var EMIT_EMSCRIPTEN_METADATA = 0;

// Emits emscripten license info in the JS output.
// [upstream-only]
var EMIT_EMSCRIPTEN_LICENSE = 0;

// Whether to legalize the JS FFI interfaces (imports/exports) by wrapping them
// to automatically demote i64 to i32 and promote f32 to f64. This is necessary
// in order to interface with JavaScript, both for asm.js and wasm.  For
// non-web/non-JS embeddings, setting this to 0 may be desirable.
// LEGALIZE_JS_FFI=0 is incompatible with RUNNING_JS_OPTS.
var LEGALIZE_JS_FFI = 1;

// Ports

// Specify the SDL version that is being linked against.
// 1, the default, is 1.3, which is implemented in JS
// 2 is a port of the SDL C code on emscripten-ports
var USE_SDL = 1;

// Specify the SDL_gfx version that is being linked against. Must match USE_SDL
var USE_SDL_GFX = 0;

// Specify the SDL_image version that is being linked against. Must match USE_SDL
var USE_SDL_IMAGE = 1;

// Specify the SDL_ttf version that is being linked against. Must match USE_SDL
var USE_SDL_TTF = 1;

// Specify the SDL_net version that is being linked against. Must match USE_SDL
var USE_SDL_NET = 1;

// 1 = use icu from emscripten-ports
var USE_ICU = 0;

// 1 = use zlib from emscripten-ports
var USE_ZLIB = 0;

// 1 = use bzip2 from emscripten-ports
var USE_BZIP2 = 0;

// 1 = use libjpeg from emscripten-ports
var USE_LIBJPEG = 0;

// 1 = use libpng from emscripten-ports
var USE_LIBPNG = 0;

// 1 = use Regal from emscripten-ports
var USE_REGAL = 0;

// 1 = use Boost headers from emscripten-ports
var USE_BOOST_HEADERS = 0;

// 1 = use bullet from emscripten-ports
var USE_BULLET = 0;

// 1 = use vorbis from emscripten-ports
var USE_VORBIS = 0;

// 1 = use ogg from emscripten-ports
var USE_OGG = 0;

// 1 = use freetype from emscripten-ports
var USE_FREETYPE = 0;

// Specify the SDL_mixer version that is being linked against.
// Doesn't *have* to match USE_SDL, but a good idea.
var USE_SDL_MIXER = 1;

// 1 = use harfbuzz from harfbuzz upstream
var USE_HARFBUZZ = 0;

// 3 = use cocos2d v3 from emscripten-ports
var USE_COCOS2D = 0;

// Formats to support in SDL2_image. Valid values: bmp, gif, lbm, pcx, png, pnm, tga, xcf, xpm, xv
var SDL2_IMAGE_FORMATS = [];

// The list of defines (C_DEFINES) was moved into struct_info.json in the same
// directory.  That file is automatically parsed by tools/gen_struct_info.py.
// If you modify the headers, just clear your cache and emscripten libc should
// see the new values.

// If true, the current build is performed for the Emscripten test harness.
var IN_TEST_HARNESS = 0;

// If true, enables support for pthreads.
// [compile+link] - affects user code at compile and system libraries at link
var USE_PTHREADS = 0;

// In web browsers, Workers cannot be created while the main browser thread
// is executing JS/Wasm code, but the main thread must regularly yield back
// to the browser event loop for Worker initialization to occur.
// This means that pthread_create() is essentially an asynchronous operation
// when called from the main browser thread, and the main thread must
// repeatedly yield back to the JS event loop in order for the thread to
// actually start.
// If your application needs to be able to synchronously create new threads,
// you can pre-create a pthread pool by specifying -s PTHREAD_POOL_SIZE=x,
// in which case the specified number of Workers will be preloaded into a pool
// before the application starts, and that many threads can then be available
// for synchronous creation.
// Note that this setting is a string, and will be emitted in the JS code
// (directly, with no extra quotes) so that if you set it to '5' then 5 workers
// will be used in the pool, and so forth. The benefit of this being a string
// is that you can set it to something like
// 'navigator.hardwareConcurrency' (which will use the number of cores the
// browser reports, and is how you can get exactly enough workers for a
// threadpool equal to the number of cores).
// [link] - affects generated JS runtime code at link time
var PTHREAD_POOL_SIZE = '';

// If your application does not need the ability to synchronously create
// threads, but it would still like to opportunistically speed up initial thread
// startup time by prewarming a pool of Workers, you can specify the size of
// the pool with -s PTHREAD_POOL_SIZE=x, but then also specify
// -s PTHREAD_POOL_DELAY_LOAD=1, which will cause the runtime to not wait up at
// startup for the Worker pool to finish loading. Instead, the runtime will
// immediately start up and the Worker pool will asynchronously spin up in
// parallel on the background. This can shorten the time that pthread_create()
// calls take to actually start a thread, but without actually slowing down
// main application startup speed. If PTHREAD_POOL_DELAY_LOAD=0 (default),
// then the runtime will wait for the pool to start up before running main().
// [link] - affects generated JS runtime code at link time
var PTHREAD_POOL_DELAY_LOAD = 0;

// If not explicitly specified, this is the stack size to use for newly created
// pthreads.  According to
// http://man7.org/linux/man-pages/man3/pthread_create.3.html, default stack
// size on Linux/x86-32 for a new thread is 2 megabytes, so follow the same
// convention. Use pthread_attr_setstacksize() at thread creation time to
// explicitly specify the stack size, in which case this value is ignored. Note
// that the asm.js/wasm function call control flow stack is separate from this
// stack, and this stack only contains certain function local variables, such as
// those that have their addresses taken, or ones that are too large to fit as
// local vars in asm.js/wasm code.
var DEFAULT_PTHREAD_STACK_SIZE = 2*1024*1024;

// True when building with --threadprofiler
var PTHREADS_PROFILING = 0;

// It is dangerous to call pthread_join or pthread_cond_wait
// on the main thread, as doing so can cause deadlocks on the Web (and also
// it works using a busy-wait which is expensive). See
// https://emscripten.org/docs/porting/pthreads.html#blocking-on-the-main-browser-thread
// This may become set to 0 by default in the future; for now, this just
// warns in the console.
var ALLOW_BLOCKING_ON_MAIN_THREAD = 1;

// If true, add in debug traces for diagnosing pthreads related issues.
var PTHREADS_DEBUG = 0;

// If true, building against Emscripten's asm.js/wasm heap memory profiler.
var MEMORYPROFILER = 0;

// Duplicate function elimination. This coalesces function bodies that are
// identical, which can happen e.g. if two methods have different C/C++ or LLVM
// types, but end up identical at the asm.js level (all pointers are the same as
// int32_t in asm.js, for example).
//
// This option is quite slow to run, as it processes and hashes all methods in
// the codebase in multiple passes.
//
// [fastcomp-only]
var ELIMINATE_DUPLICATE_FUNCTIONS = 0; // disabled by default
var ELIMINATE_DUPLICATE_FUNCTIONS_DUMP_EQUIVALENT_FUNCTIONS = 0;
var ELIMINATE_DUPLICATE_FUNCTIONS_PASSES = 5;

// This tries to evaluate global ctors at compile-time, applying their effects
// into the mem init file. This saves running code during startup, and also
// allows removing the global ctor functions and other code that only they used,
// so this is also good for reducing code size. However, this does make the
// compile step much slower.
//
// This basically runs the ctors during compile time, seeing if they execute
// safely in a sandbox. Any ffi access out of asm.js causes failure, as it could
// do something nondeterministic and/or alter some other state we don't see. If
// all the global ctor does is pure computation inside asm.js, it should be ok.
// Run with EMCC_DEBUG=1 in the env to see logging, and errors when it fails to
// eval (you'll see a message, or a stack trace; in the latter case, the
// functions on the stack should give you an idea of what ffi was called and
// why, and perhaps you can refactor your code to avoid it, e.g., remove
// mallocs, printfs in global ctors).
//
// This optimization can increase the size of the mem init file, because ctors
// can write to memory that would otherwise be in a zeroinit area. This may not
// be a significant increase after gzip, if there are mostly zeros in there, and
// in any case the mem init increase would be offset by a code size decrease.
// (Unless you have a small ctor that writes 'random' data to memory, which
// would reduce little code but add potentially lots of uncompressible data.)
//
// LLVM's GlobalOpt *almost* does this operation. It does in simple cases, where
// LLVM IR is not too complex for its logic to evaluate, but it isn't powerful
// enough for e.g. libc++ iostream ctors. It is just hard to do at the LLVM IR
// level - LLVM IR is complex and getting more complex, this would require
// GlobalOpt to have a full interpreter, plus a way to write back into LLVM IR
// global objects.  At the asm.js level, however, everything has been lowered
// into a simple low level, and we also just need to write bytes into an array,
// so this is easy for us to do, but not for LLVM. A further issue for LLVM is
// that it doesn't know that we will not link in further code, so it only tries
// to optimize ctors with lowest priority. We do know that, and can optimize all
// the ctors.
var EVAL_CTORS = 0;

// see http://kripken.github.io/emscripten-site/docs/debugging/CyberDWARF.html
// [fastcomp-only]
var CYBERDWARF = 0;

// Path to the CyberDWARF debug file passed to the compiler
// [fastcomp-only]
var BUNDLED_CD_DEBUG_FILE = "";

// Is enabled, use the JavaScript TextDecoder API for string marshalling.
// Enabled by default, set this to 0 to disable.
// If set to 2, we assume TextDecoder is present and usable, and do not emit
// any JS code to fall back if it is missing.
var TEXTDECODER = 1;

// Embind specific: If enabled, assume UTF-8 encoded data in std::string binding.
// Disable this to support binary data transfer.
var EMBIND_STD_STRING_IS_UTF8 = 1;

// If set to 1, enables support for transferring canvases to pthreads and
// creating WebGL contexts in them, as well as explicit swap control for GL
// contexts. This needs browser support for the OffscreenCanvas specification.
var OFFSCREENCANVAS_SUPPORT = 0;

// If you are using PROXY_TO_PTHREAD with OFFSCREENCANVAS_SUPPORT, then specify
// here a comma separated list of CSS ID selectors to canvases to proxy over
// to the pthread at program startup, e.g. '#canvas1, #canvas2'.
var OFFSCREENCANVASES_TO_PTHREAD = "#canvas";

// If set to 1, enables support for WebGL contexts to render to an offscreen
// render target, to avoid the implicit swap behavior of WebGL where exiting any
// event callback would automatically perform a "flip" to present rendered
// content on screen. When an Emscripten GL context has Offscreen Framebuffer
// enabled, a single frame can be composited from multiple event callbacks, and
// the swap function emscripten_webgl_commit_frame() is then explicitly called
// to present the rendered content on screen.
//
// The OffscreenCanvas feature also enables explicit GL frame swapping support,
// and also, -s OFFSCREEN_FRAMEBUFFER=1 feature can be used to polyfill support
// for accessing WebGL in multiple threads in the absence of OffscreenCanvas
// support in browser, at the cost of some performance and latency.
// OffscreenCanvas and Offscreen Framebuffer support can be enabled at the same
// time, and allows one to utilize OffscreenCanvas where available, and to fall
// back to Offscreen Framebuffer otherwise.
var OFFSCREEN_FRAMEBUFFER = 0;

// If nonzero, Fetch API (and hence ASMFS) supports backing to IndexedDB. If 0, IndexedDB is not utilized. Set to 0 if
// IndexedDB support is not interesting for target application, to save a few kBytes.
var FETCH_SUPPORT_INDEXEDDB = 1;

// If nonzero, prints out debugging information in library_fetch.js
var FETCH_DEBUG = 0;

// If nonzero, enables emscripten_fetch API.
var FETCH = 0;

// Whether to use an asm.js fetch worker when using FETCH. Note that this is
// only relevant for fastcomp, where we support asm.js. As a result, some
// synchronous fetch operations that depend on the fetch worker may not work
// with the wasm backend, like waiting or IndexedDB.
// Currently will always be set to 0 on WASM backend.
var USE_FETCH_WORKER = 1;

// If set to 1, uses the multithreaded filesystem that is implemented within the
// asm.js module, using emscripten_fetch. Implies -s FETCH=1.
var ASMFS = 0;

// If set to 1, embeds all subresources in the emitted file as base64 string
// literals. Embedded subresources may include (but aren't limited to) wasm,
// asm.js, and static memory initialization code.
//
// When using code that depends on this option, your Content Security Policy may
// need to be updated. Specifically, embedding asm.js requires the script-src
// directive to whitelist 'unsafe-inline', and using a Worker requires the
// child-src directive to whitelist blob:. If you aren't using Content Security
// Policy, or your CSP header doesn't include either script-src or child-src,
// then you can safely ignore this warning.
var SINGLE_FILE = 0;

// If set to 1, all JS libraries will be automatically available at link time.
// This gets set to 0 in STRICT mode (or with MINIMAL_RUNTIME) which mean you
// need to explicitly specify -lfoo.js in at link time in order to access
// library function in library_foo.js.
var AUTO_JS_LIBRARIES = 1;

// Specifies the oldest major version of Firefox to target. I.e. all Firefox
// versions >= MIN_FIREFOX_VERSION
// are desired to work. Pass -s MIN_FIREFOX_VERSION=majorVersion to drop support
// for Firefox versions older than < majorVersion.
// Firefox ESR 60.5 (Firefox 65) was released on 2019-01-29.
var MIN_FIREFOX_VERSION = 65;

// Specifies the oldest version of desktop Safari to target. Version is encoded
// in MMmmVV, e.g. 70101 denotes Safari 7.1.1.
// Safari 12.0.0 was released on September 17, 2018, bundled with macOS 10.14.0
// Mojave
var MIN_SAFARI_VERSION = 120000;

// Specifies the oldest version of Internet Explorer to target. E.g. pass -s
// MIN_IE_VERSION = 11 to drop support for IE 10 and older.
// Internet Explorer is at end of life and does not support WebAssembly.
// MAX_INT (0x7FFFFFFF) specifies that target is not supported.
var MIN_IE_VERSION = 0x7FFFFFFF;

// Specifies the oldest version of Edge (EdgeHTML, the non-Chromium based
// flavor) to target. E.g. pass -s MIN_EDGE_VERSION=40 to drop support for
// EdgeHTML 39 and older.
// Edge 44.17763 was released on November 13, 2018
var MIN_EDGE_VERSION = 44;

// Specifies the oldest version of Chrome. E.g. pass -s MIN_CHROME_VERSION=58 to
// drop support for Chrome 57 and older.
// Chrome 75.0.3770 was released on 2019-06-04
var MIN_CHROME_VERSION = 75;

// Tracks whether we are building with errno support enabled. Set to 0
// to disable compiling errno support in altogether. This saves a little
// bit of generated code size in applications that do not care about
// POSIX errno variable. Setting this to 0 also requires using --closure
// for effective code size optimizations to take place.
var SUPPORT_ERRNO = 1;

// If true, uses minimal sized runtime without POSIX features, Module,
// preRun/preInit/etc., Emscripten built-in XHR loading or library_browser.js.
// Enable this setting to target the smallest code size possible.  Set
// MINIMAL_RUNTIME=2 to further enable even more code size optimizations. These
// opts are quite hacky, and work around limitations in Closure and other parts
// of the build system, so they may not work in all generated programs (But can
// be useful for really small programs)
var MINIMAL_RUNTIME = 0;

// If set to 1, MINIMAL_RUNTIME will utilize streaming WebAssembly compilation,
// where WebAssembly module is compiled already while it is being downloaded.
// In order for this to work, the web server MUST properly serve the .wasm file
// with a HTTP response header "Content-Type: application/wasm". If this HTTP
// header is not present, e.g. Firefox 73 will fail with an error message
//    TypeError: Response has unsupported MIME type
// and Chrome 78 will fail with an error message
//    Uncaught (in promise) TypeError: Failed to execute 'compile' on
//    'WebAssembly': Incorrect response MIME type. Expected 'application/wasm'.
// If set to 0 (default), streaming WebAssembly compilation is disabled, which
// means that the WebAssembly Module will first be downloaded fully, and only
// then compilation starts.
// For large .wasm modules and production environments, this should be set to 1
// for faster startup speeds. However this setting is disabled by default
// since it requires server side configuration and for really small pages there
// is no observable difference (also has a ~100 byte impact to code size)
var MINIMAL_RUNTIME_STREAMING_WASM_COMPILATION = 0;

// If set to 1, MINIMAL_RUNTIME will utilize streaming WebAssembly instantiation,
// where WebAssembly module is compiled+instantiated already while it is being
// downloaded. Same restrictions/requirements apply as with
// MINIMAL_RUNTIME_STREAMING_WASM_COMPILATION.
// MINIMAL_RUNTIME_STREAMING_WASM_COMPILATION and
// MINIMAL_RUNTIME_STREAMING_WASM_INSTANTIATION cannot be simultaneously active.
// Which one of these two is faster depends on the size of the wasm module,
// the size of the JS runtime file, and the size of the preloaded data file
// to download, and the browser in question.
var MINIMAL_RUNTIME_STREAMING_WASM_INSTANTIATION = 0;

// If building with MINIMAL_RUNTIME=1 and application uses sbrk()/malloc(),
// enable this. If you are not using dynamic allocations, can set this to 0 to
// save code size. This setting is ignored when building with -s
// MINIMAL_RUNTIME=0.
var USES_DYNAMIC_ALLOC = 1;

// Advanced manual dead code elimination: Specifies the set of runtime JS
// functions that should be imported to the asm.js/wasm module.  Remove elements
// from this list to make build smaller if some of these are not needed.  In
// Wasm -O3/-Os builds, adjusting this is not necessary, as the Meta-DCE pass is
// able to remove these, but if you are targeting asm.js or doing a -O2 build or
// lower, then this can be beneficial.
var RUNTIME_FUNCS_TO_IMPORT = ['abort', 'setTempRet0', 'getTempRet0']

// If true, compiler supports setjmp() and longjmp(). If false, these APIs are
// not available.  If you are using C++ exceptions, but do not need
// setjmp()+longjmp() API, then you can set this to 0 to save a little bit of
// code size and performance when catching exceptions.
var SUPPORT_LONGJMP = 1;

// If set to 1, disables old deprecated HTML5 API event target lookup behavior.
// When enabled, there is no "Module.canvas" object, no magic "null" default
// handling, and DOM element 'target' parameters are taken to refer to CSS
// selectors, instead of referring to DOM IDs.
var DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR = 1;

// Certain browser DOM API operations, such as requesting fullscreen mode
// transition or pointer lock require that the request originates from within
// an user initiated event, such as mouse click or keyboard press. Refactoring
// an application to follow this kind of program structure can be difficult, so
// HTML5_SUPPORT_DEFERRING_USER_SENSITIVE_REQUESTS=1 flag allows transparent
// emulation of this by deferring synchronous fullscreen mode and pointer lock
// requests until a suitable event callback is generated. Set this to 0
// to disable support for deferring to save code space if your application does
// not need support for deferred calls.
var HTML5_SUPPORT_DEFERRING_USER_SENSITIVE_REQUESTS = 1;

// Specifies whether the generated .html file is run through html-minifier. The
// set of optimization passes run by html-minifier depends on debug and
// optimization levels. In -g2 and higher, no minification is performed. In -g1,
// minification is done, but whitespace is retained. Minification requires at
// least -O1 or -Os to be used. Pass -s MINIFY_HTML=0 to explicitly choose to
// disable HTML minification altogether.
var MINIFY_HTML = 1;

// Whether we *may* be using wasm2js. This compiles to wasm normally, but lets
// you run wasm2js *later* on the wasm, and you can pick between running the
// normal wasm or that wasm2js code. For details of how to do that, see the
// test_maybe_wasm2js test.  This option can be useful for debugging and
// bisecting.
var MAYBE_WASM2JS = 0;

// The size of our shadow memory.
// By default, we have 32 MiB. This supports 256 MiB of real memory.
var ASAN_SHADOW_SIZE = 33554432;

// Internal: Tracks whether Emscripten should link in exception throwing (C++
// 'throw') support library. This does not need to be set directly, but pass
// -fno-exceptions to the build disable exceptions support. (This is basically
// -fno-exceptions, but checked at final link time instead of individual .cpp
// file compile time) If the program *does* contain throwing code (some source
// files were not compiled with `-fno-exceptions`), and this flag is set at link
// time, then you will get errors on undefined symbols, as the exception
// throwing code is not linked in. If so you should either unset the option (if
// you do want exceptions) or fix the compilation of the source files so that
// indeed no exceptions are used).
// TODO(sbc): Move to settings_internal (current blocked due to use in test
// code).
var DISABLE_EXCEPTION_THROWING = 0;

// Whether we should use the offset converter.  This is needed for older
// versions of v8 (<7.7) that does not give the hex module offset into wasm
// binary in stack traces, as well as for avoiding using source map entries
// across function boundaries.
var USE_OFFSET_CONVERTER = 0;

// If set to 1, the JS compiler is run before wasm-ld so that the linker can
// report undefined symbols within the binary.  Without this option that linker
// doesn't know which symbols might be defined JS and so reporting of undefined
// symbols is delayed until the JS compiler is run.
// [link]
var LLD_REPORT_UNDEFINED = 0;

// Default to c++ mode even when run as `emcc` rather then `emc++`.
// When this is disabled `em++` is required when compiling and linking C++
// programs. This which matches the behaviour of gcc/g++ and clang/clang++.
var DEFAULT_TO_CXX = 1;

// While LLVM's wasm32 has long double = float128, we don't support printing
// that at full precision by default. Instead we print as 64-bit doubles, which
// saves libc code size. You can flip this option on to get a libc with full
// long double printing precision.
var PRINTF_LONG_DOUBLE = 0;

//===========================================
// Internal, used for testing only, from here
//===========================================

// Internal (testing only): Disables the blitOffscreenFramebuffer VAO path.
var OFFSCREEN_FRAMEBUFFER_FORBID_VAO_PATH = 0;

// Internal (testing only): Forces memory growing to fail.
var TEST_MEMORY_GROWTH_FAILS = 0;

// Advanced: Customize this array to reduce the set of asm.js runtime variables
// that are generated. This allows minifying extra bit of asm.js code from unused
// runtime code, if you know some of these are not needed.
// (think of this as advanced manual DCE)
// TODO(sbc): Move to settings_internal (current blocked due to use in test
// code).
var ASM_PRIMITIVE_VARS = ['__THREW__', 'threwValue', 'setjmpId', 'tempInt', 'tempBigInt', 'tempBigIntS', 'tempValue', 'tempDouble', 'tempFloat', 'tempDoublePtr', 'STACKTOP', 'STACK_MAX']

// Legacy settings that have been removed or renamed.
// For renamed settings the format is:
// [OLD_NAME, NEW_NAME]
// For removed settings (which now effectively have a fixed value and can no
// longer be changed) the format is:
// [OPTION_NAME, POSSIBLE_VALUES, ERROR_EXPLANATION], where POSSIBLE_VALUES is
// an array of values that will still be silently accepted by the compiler.
// First element in the list is the canonical/fixed value going forward.
// This allows existing build systems to keep specifying one of the supported
// settings, for backwards compatibility.
var LEGACY_SETTINGS = [
  ['BINARYEN', 'WASM'],
  ['BINARYEN_ASYNC_COMPILATION', 'WASM_ASYNC_COMPILATION'],
  ['UNALIGNED_MEMORY', [0], 'forced unaligned memory not supported in fastcomp'],
  ['FORCE_ALIGNED_MEMORY', [0], 'forced aligned memory is not supported in fastcomp'],
  ['PGO', [0], 'pgo no longer supported'],
  ['QUANTUM_SIZE', [4], 'altering the QUANTUM_SIZE is not supported'],
  ['FUNCTION_POINTER_ALIGNMENT', [2], 'Starting from Emscripten 1.37.29, no longer available (https://github.com/emscripten-core/emscripten/pull/6091)'],
  ['BUILD_AS_SHARED_LIB', [0], 'Starting from Emscripten 1.38.16, no longer available (https://github.com/emscripten-core/emscripten/pull/7433)'],
  ['SAFE_SPLIT_MEMORY', [0], 'Starting from Emscripten 1.38.19, SAFE_SPLIT_MEMORY codegen is no longer available (https://github.com/emscripten-core/emscripten/pull/7465)'],
  ['SPLIT_MEMORY', [0], 'Starting from Emscripten 1.38.19, SPLIT_MEMORY codegen is no longer available (https://github.com/emscripten-core/emscripten/pull/7465)'],
  ['BINARYEN_METHOD', ['native-wasm'], 'Starting from Emscripten 1.38.23, Emscripten now always builds either to Wasm (-s WASM=1 - default), or to asm.js (-s WASM=0), other methods are not supported (https://github.com/emscripten-core/emscripten/pull/7836)'],
  ['PRECISE_I64_MATH', [1, 2], 'Starting from Emscripten 1.38.26, PRECISE_I64_MATH is always enabled (https://github.com/emscripten-core/emscripten/pull/7935)'],
  ['MEMFS_APPEND_TO_TYPED_ARRAYS', [1], 'Starting from Emscripten 1.38.26, MEMFS_APPEND_TO_TYPED_ARRAYS=0 is no longer supported. MEMFS no longer supports using JS arrays for file data (https://github.com/emscripten-core/emscripten/pull/7918)'],
  ['ERROR_ON_MISSING_LIBRARIES', [1], 'missing libraries are always an error now'],
  ['EMITTING_JS', [1], 'The new STANDALONE_WASM flag replaces this (replace EMITTING_JS=0 with STANDALONE_WASM=1)'],
  ['SKIP_STACK_IN_SMALL', [0, 1], 'SKIP_STACK_IN_SMALL is no longer needed as the backend can optimize it directly'],
  ['SAFE_STACK', [0], 'Replace SAFE_STACK=1 with STACK_OVERFLOW_CHECK=2'],
  ['MEMORY_GROWTH_STEP', 'MEMORY_GROWTH_LINEAR_STEP'],
  // WASM_OBJECT_FILES is handled in emcc.py, supporting both 0 and 1 for now.
  ['WASM_OBJECT_FILES', [0, 1], 'For LTO, use -flto or -fto=thin instead; to disable LTO, just do not pass WASM_OBJECT_FILES=1 as 1 is the default anyhow'],
  ['TOTAL_MEMORY', 'INITIAL_MEMORY'],
  ['WASM_MEM_MAX', 'MAXIMUM_MEMORY'],
  ['BINARYEN_MEM_MAX', 'MAXIMUM_MEMORY'],
  ['BINARYEN_PASSES', [''], 'Use BINARYEN_EXTRA_PASSES to add additional passes'],
  ['SWAPPABLE_ASM_MODULE', [0], 'Fully swappable asm modules are no longer supported'],
];
