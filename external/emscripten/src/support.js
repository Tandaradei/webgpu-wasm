/**
 * @license
 * Copyright 2017 The Emscripten Authors
 * SPDX-License-Identifier: MIT
 */

// {{PREAMBLE_ADDITIONS}}

var STACK_ALIGN = {{{ STACK_ALIGN }}};

#if ASSERTIONS
// stack management, and other functionality that is provided by the compiled code,
// should not be used before it is ready

#if DECLARE_ASM_MODULE_EXPORTS // These functions should not be defined with 'var' if DECLARE_ASM_MODULE_EXPORTS==0 (or the assignments won't be seen)
/** @suppress{duplicate} */
var stackSave;
/** @suppress{duplicate} */
var stackRestore;
/** @suppress{duplicate} */
var stackAlloc;
#endif

stackSave = stackRestore = stackAlloc = function() {
  abort('cannot use the stack before compiled code is ready to run, and has provided stack access');
};

function staticAlloc(size) {
  abort('staticAlloc is no longer available at runtime; instead, perform static allocations at compile time (using makeStaticAlloc)');
}
#endif

function dynamicAlloc(size) {
#if ASSERTIONS
  assert(DYNAMICTOP_PTR);
#if USE_PTHREADS
  assert(!ENVIRONMENT_IS_PTHREAD); // this function is not thread-safe
#endif
#endif
  var ret = HEAP32[DYNAMICTOP_PTR>>2];
  var end = (ret + size + 15) & -16;
#if ASSERTIONS
  assert(end <= HEAP8.length, 'failure to dynamicAlloc - memory growth etc. is not supported there, call malloc/sbrk directly');
#endif
  HEAP32[DYNAMICTOP_PTR>>2] = end;
  return ret;
}

{{{ alignMemory }}}

{{{ getNativeTypeSize }}}

function warnOnce(text) {
  if (!warnOnce.shown) warnOnce.shown = {};
  if (!warnOnce.shown[text]) {
    warnOnce.shown[text] = 1;
    err(text);
  }
}

#if !WASM_BACKEND
var asm2wasmImports = { // special asm2wasm imports
    "f64-rem": function(x, y) {
        return x % y;
    },
    "debugger": function() {
#if ASSERTIONS // Disable debugger; statement from being present in release builds to avoid Firefox deoptimizations, see https://bugzilla.mozilla.org/show_bug.cgi?id=1538375
        debugger;
#endif
    }
#if NEED_ALL_ASM2WASM_IMPORTS
    ,
    "f64-to-int": function(x) {
        return x | 0;
    },
    "i32s-div": function(x, y) {
        return ((x | 0) / (y | 0)) | 0;
    },
    "i32u-div": function(x, y) {
        return ((x >>> 0) / (y >>> 0)) >>> 0;
    },
    "i32s-rem": function(x, y) {
        return ((x | 0) % (y | 0)) | 0;
    },
    "i32u-rem": function(x, y) {
        return ((x >>> 0) % (y >>> 0)) >>> 0;
    }
#endif // NEED_ALL_ASM2WASM_IMPORTS
};
#endif

#if RELOCATABLE
// dynamic linker/loader (a-la ld.so on ELF systems)
var LDSO = {
  // next free handle to use for a loaded dso.
  // (handle=0 is avoided as it means "error" in dlopen)
  nextHandle: 1,

  loadedLibs: {         // handle -> dso [refcount, name, module, global]
    // program itself
    // XXX uglifyjs fails on "[-1]: {"
    '-1': {
      refcount: Infinity,   // = nodelete
      name:     '__self__',
      module:   Module,
      global:   true
    }
  },

  loadedLibNames: {     // name   -> handle
    // program itself
    '__self__': -1
  },
}

// fetchBinary fetches binaray data @ url. (async)
function fetchBinary(url) {
  return fetch(url, { credentials: 'same-origin' }).then(function(response) {
    if (!response['ok']) {
      throw "failed to load binary file at '" + url + "'";
    }
    return response['arrayBuffer']();
  }).then(function(buffer) {
    return new Uint8Array(buffer);
  });
}

// loadDynamicLibrary loads dynamic library @ lib URL / path and returns handle for loaded DSO.
//
// Several flags affect the loading:
//
// - if flags.global=true, symbols from the loaded library are merged into global
//   process namespace. Flags.global is thus similar to RTLD_GLOBAL in ELF.
//
// - if flags.nodelete=true, the library will be never unloaded. Flags.nodelete
//   is thus similar to RTLD_NODELETE in ELF.
//
// - if flags.loadAsync=true, the loading is performed asynchronously and
//   loadDynamicLibrary returns corresponding promise.
//
// - if flags.fs is provided, it is used as FS-like interface to load library data.
//   By default, when flags.fs=undefined, native loading capabilities of the
//   environment are used.
//
// If a library was already loaded, it is not loaded a second time. However
// flags.global and flags.nodelete are handled every time a load request is made.
// Once a library becomes "global" or "nodelete", it cannot be removed or unloaded.
function loadDynamicLibrary(lib, flags) {
  // when loadDynamicLibrary did not have flags, libraries were loaded globally & permanently
  flags = flags || {global: true, nodelete: true}

  var handle = LDSO.loadedLibNames[lib];
  var dso;
  if (handle) {
    // the library is being loaded or has been loaded already.
    //
    // however it could be previously loaded only locally and if we get
    // load request with global=true we have to make it globally visible now.
    dso = LDSO.loadedLibs[handle];
    if (flags.global && !dso.global) {
      dso.global = true;
      if (dso.module !== 'loading') {
        // ^^^ if module is 'loading' - symbols merging will be eventually done by the loader.
        mergeLibSymbols(dso.module)
      }
    }
    // same for "nodelete"
    if (flags.nodelete && dso.refcount !== Infinity) {
      dso.refcount = Infinity;
    }
    dso.refcount++
    return flags.loadAsync ? Promise.resolve(handle) : handle;
  }

  // allocate new DSO & handle
  handle = LDSO.nextHandle++;
  dso = {
    refcount: flags.nodelete ? Infinity : 1,
    name:     lib,
    module:   'loading',
    global:   flags.global,
  };
  LDSO.loadedLibNames[lib] = handle;
  LDSO.loadedLibs[handle] = dso;

  // libData <- libFile
  function loadLibData(libFile) {
#if WASM
    // for wasm, we can use fetch for async, but for fs mode we can only imitate it
    if (flags.fs) {
      var libData = flags.fs.readFile(libFile, {encoding: 'binary'});
      if (!(libData instanceof Uint8Array)) {
        libData = new Uint8Array(lib_data);
      }
      return flags.loadAsync ? Promise.resolve(libData) : libData;
    }

    if (flags.loadAsync) {
      return fetchBinary(libFile);
    }
    // load the binary synchronously
    return readBinary(libFile);
#else
    // for js we only imitate async for both native & fs modes.
    var libData;
    if (flags.fs) {
      libData = flags.fs.readFile(libFile, {encoding: 'utf8'});
    } else {
      libData = read_(libFile);
    }
    return flags.loadAsync ? Promise.resolve(libData) : libData;
#endif
  }

  // libModule <- libData
  function createLibModule(libData) {
#if WASM
    return loadWebAssemblyModule(libData, flags)
#else
    var libModule = /**@type{function(...)}*/(eval(libData))(
      alignFunctionTables(),
      Module
    );
    // load dynamic libraries that this js lib depends on
    // (wasm loads needed libraries _before_ lib in its own codepath)
    if (libModule.dynamicLibraries) {
      if (flags.loadAsync) {
        return Promise.all(libModule.dynamicLibraries.map(function(dynNeeded) {
          return loadDynamicLibrary(dynNeeded, flags);
        })).then(function() {
          return libModule;
        });
      }

      libModule.dynamicLibraries.forEach(function(dynNeeded) {
        loadDynamicLibrary(dynNeeded, flags);
      });
    }
    return libModule;
#endif
  }

  // libModule <- lib
  function getLibModule() {
    // lookup preloaded cache first
    if (Module['preloadedWasm'] !== undefined &&
        Module['preloadedWasm'][lib] !== undefined) {
      var libModule = Module['preloadedWasm'][lib];
      return flags.loadAsync ? Promise.resolve(libModule) : libModule;
    }

    // module not preloaded - load lib data and create new module from it
    if (flags.loadAsync) {
      return loadLibData(lib).then(function(libData) {
        return createLibModule(libData);
      });
    }

    return createLibModule(loadLibData(lib));
  }

  // Module.symbols <- libModule.symbols (flags.global handler)
  function mergeLibSymbols(libModule) {
    // add symbols into global namespace TODO: weak linking etc.
    for (var sym in libModule) {
      if (!libModule.hasOwnProperty(sym)) {
        continue;
      }

      // When RTLD_GLOBAL is enable, the symbols defined by this shared object will be made
      // available for symbol resolution of subsequently loaded shared objects.
      //
      // We should copy the symbols (which include methods and variables) from SIDE_MODULE to MAIN_MODULE.

      var module_sym = sym;
#if WASM_BACKEND
      module_sym = '_' + sym;
#else
      // Module of SIDE_MODULE has not only the symbols (which should be copied)
      // but also others (print*, asmGlobal*, FUNCTION_TABLE_**, NAMED_GLOBALS, and so on).
      //
      // When the symbol (which should be copied) is method, Module.* 's type becomes function.
      // When the symbol (which should be copied) is variable, Module.* 's type becomes number.
      // Except for the symbol prefix (_), there is no difference in the symbols (which should be copied) and others.
      // So this just copies over compiled symbols (which start with _).
      if (sym[0] !== '_') {
        continue;
      }
#endif

      if (!Module.hasOwnProperty(module_sym)) {
        Module[module_sym] = libModule[sym];
      }
#if ASSERTIONS == 2
      else {
        var curr = Module[sym], next = libModule[sym];
        // don't warn on functions - might be odr, linkonce_odr, etc.
        if (!(typeof curr === 'function' && typeof next === 'function')) {
          err("warning: symbol '" + sym + "' from '" + lib + "' already exists (duplicate symbol? or weak linking, which isn't supported yet?)"); // + [curr, ' vs ', next]);
        }
      }
#endif
    }
  }

  // module for lib is loaded - update the dso & global namespace
  function moduleLoaded(libModule) {
    if (dso.global) {
      mergeLibSymbols(libModule);
    }
    dso.module = libModule;
  }

  if (flags.loadAsync) {
    return getLibModule().then(function(libModule) {
      moduleLoaded(libModule);
      return handle;
    })
  }

  moduleLoaded(getLibModule());
  return handle;
}

#if WASM
// Applies relocations to exported things.
function relocateExports(exports, memoryBase, tableBase, moduleLocal) {
  var relocated = {};

  for (var e in exports) {
    var value = exports[e];
    if (typeof value === 'object') {
      // a breaking change in the wasm spec, globals are now objects
      // https://github.com/WebAssembly/mutable-global/issues/1
      value = value.value;
    }
    if (typeof value === 'number') {
      // relocate it - modules export the absolute value, they can't relocate before they export
#if EMULATE_FUNCTION_POINTER_CASTS
      // it may be a function pointer
      if (e.substr(0, 3) == 'fp$' && typeof exports[e.substr(3)] === 'function') {
        value += tableBase;
      } else {
#endif
        value += memoryBase;
#if EMULATE_FUNCTION_POINTER_CASTS
      }
#endif
    }
    relocated[e] = value;
    if (moduleLocal) {
#if WASM_BACKEND
      moduleLocal['_' + e] = value;
#else
      moduleLocal[e] = value;
#endif
    }
  }
  return relocated;
}

// Loads a side module from binary data
function loadWebAssemblyModule(binary, flags) {
  var int32View = new Uint32Array(new Uint8Array(binary.subarray(0, 24)).buffer);
  assert(int32View[0] == 0x6d736100, 'need to see wasm magic number'); // \0asm
  // we should see the dylink section right after the magic number and wasm version
  assert(binary[8] === 0, 'need the dylink section to be first')
  var next = 9;
  function getLEB() {
    var ret = 0;
    var mul = 1;
    while (1) {
      var byte = binary[next++];
      ret += ((byte & 0x7f) * mul);
      mul *= 0x80;
      if (!(byte & 0x80)) break;
    }
    return ret;
  }
  var sectionSize = getLEB();
  assert(binary[next] === 6);                 next++; // size of "dylink" string
  assert(binary[next] === 'd'.charCodeAt(0)); next++;
  assert(binary[next] === 'y'.charCodeAt(0)); next++;
  assert(binary[next] === 'l'.charCodeAt(0)); next++;
  assert(binary[next] === 'i'.charCodeAt(0)); next++;
  assert(binary[next] === 'n'.charCodeAt(0)); next++;
  assert(binary[next] === 'k'.charCodeAt(0)); next++;
  var memorySize = getLEB();
  var memoryAlign = getLEB();
  var tableSize = getLEB();
  var tableAlign = getLEB();

  // shared libraries this module needs. We need to load them first, so that
  // current module could resolve its imports. (see tools/shared.py
  // WebAssembly.make_shared_library() for "dylink" section extension format)
  var neededDynlibsCount = getLEB();
  var neededDynlibs = [];
  for (var i = 0; i < neededDynlibsCount; ++i) {
    var nameLen = getLEB();
    var nameUTF8 = binary.subarray(next, next + nameLen);
    next += nameLen;
    var name = UTF8ArrayToString(nameUTF8, 0);
    neededDynlibs.push(name);
  }

  // loadModule loads the wasm module after all its dependencies have been loaded.
  // can be called both sync/async.
  function loadModule() {
    // alignments are powers of 2
    memoryAlign = Math.pow(2, memoryAlign);
    tableAlign = Math.pow(2, tableAlign);
    // finalize alignments and verify them
    memoryAlign = Math.max(memoryAlign, STACK_ALIGN); // we at least need stack alignment
#if ASSERTIONS
    assert(tableAlign === 1, 'invalid tableAlign ' + tableAlign);
#endif
    // prepare memory
    var memoryBase = alignMemory(getMemory(memorySize + memoryAlign), memoryAlign); // TODO: add to cleanups
    // prepare env imports
    var env = asmLibraryArg;
    // TODO: use only __memory_base and __table_base, need to update asm.js backend
    var table = wasmTable;
    var tableBase = table.length;
    var originalTable = table;
    table.grow(tableSize);
    assert(table === originalTable);
    // zero-initialize memory and table
    // The static area consists of explicitly initialized data, followed by zero-initialized data.
    // The latter may need zeroing out if the MAIN_MODULE has already used this memory area before
    // dlopen'ing the SIDE_MODULE.  Since we don't know the size of the explicitly initialized data
    // here, we just zero the whole thing, which is suboptimal, but should at least resolve bugs
    // from uninitialized memory.
    for (var i = memoryBase; i < memoryBase + memorySize; i++) {
      HEAP8[i] = 0;
    }
    for (var i = tableBase; i < tableBase + tableSize; i++) {
      table.set(i, null);
    }

    // We resolve symbols against the global Module but failing that also
    // against the local symbols exported a side module.  This is because
    // a) Module sometime need to import their own symbols
    // b) Symbols from loaded modules are not always added to the global Module.
    var moduleLocal = {};

    var resolveSymbol = function(sym, type, legalized) {
#if WASM_BIGINT
      assert(!legalized);
#else
      if (legalized) {
        sym = 'orig$' + sym;
      }
#endif

      var resolved = Module["asm"][sym];
      if (!resolved) {
#if WASM_BACKEND
        sym = '_' + sym;
#endif
        resolved = Module[sym];
        if (!resolved) {
          resolved = moduleLocal[sym];
        }
#if ASSERTIONS
        assert(resolved, 'missing linked ' + type + ' `' + sym + '`. perhaps a side module was not linked in? if this global was expected to arrive from a system library, try to build the MAIN_MODULE with EMCC_FORCE_STDLIBS=1 in the environment');
#endif
      }
      return resolved;
    }

    // copy currently exported symbols so the new module can import them
    for (var x in Module) {
      if (!(x in env)) {
        env[x] = Module[x];
      }
    }

    // TODO kill ↓↓↓ (except "symbols local to this module", it will likely be
    // not needed if we require that if A wants symbols from B it has to link
    // to B explicitly: similarly to -Wl,--no-undefined)
    //
    // wasm dynamic libraries are pure wasm, so they cannot assist in
    // their own loading. When side module A wants to import something
    // provided by a side module B that is loaded later, we need to
    // add a layer of indirection, but worse, we can't even tell what
    // to add the indirection for, without inspecting what A's imports
    // are. To do that here, we use a JS proxy (another option would
    // be to inspect the binary directly).
    var proxyHandler = {
      'get': function(obj, prop) {
        // symbols that should be local to this module
        switch (prop) {
          case '__memory_base':
          case 'gb':
            return memoryBase;
          case '__table_base':
          case 'fb':
            return tableBase;
        }

        if (prop in obj) {
          return obj[prop]; // already present
        }
        if (prop.startsWith('g$')) {
          // a global. the g$ function returns the global address.
          var name = prop.substr(2); // without g$ prefix
          return obj[prop] = function() {
            return resolveSymbol(name, 'global');
          };
        }
        if (prop.startsWith('fp$')) {
          // the fp$ function returns the address (table index) of the function
          var parts = prop.split('$');
          assert(parts.length == 3)
          var name = parts[1];
          var sig = parts[2];
#if WASM_BIGINT
          var legalized = false;
#else
          var legalized = sig.indexOf('j') >= 0; // check for i64s
#endif
          var fp = 0;
          return obj[prop] = function() {
            if (!fp) {
              var f = resolveSymbol(name, 'function', legalized);
              fp = addFunction(f, sig);
            }
            return fp;
          };
        }
        if (prop.startsWith('invoke_')) {
          // A missing invoke, i.e., an invoke for a function type
          // present in the dynamic library but not in the main JS,
          // and the dynamic library cannot provide JS for it. Use
          // the generic "X" invoke for it.
          return obj[prop] = invoke_X;
        }
        // otherwise this is regular function import - call it indirectly
        return obj[prop] = function() {
          return resolveSymbol(prop, 'function').apply(null, arguments);
        };
      }
    };
    var proxy = new Proxy(env, proxyHandler);
    var info = {
      global: {
        'NaN': NaN,
        'Infinity': Infinity,
      },
      'global.Math': Math,
      env: proxy,
      {{{ WASI_MODULE_NAME }}}: proxy,
#if !WASM_BACKEND
      'asm2wasm': asm2wasmImports
#endif
    };
#if ASSERTIONS
    var oldTable = [];
    for (var i = 0; i < tableBase; i++) {
      oldTable.push(table.get(i));
    }
#endif

    function postInstantiation(instance, moduleLocal) {
#if ASSERTIONS
      // the table should be unchanged
      assert(table === originalTable);
      assert(table === wasmTable);
      // the old part of the table should be unchanged
      for (var i = 0; i < tableBase; i++) {
        assert(table.get(i) === oldTable[i], 'old table entries must remain the same');
      }
      // verify that the new table region was filled in
      for (var i = 0; i < tableSize; i++) {
        assert(table.get(tableBase + i) !== undefined, 'table entry was not filled in');
      }
#endif
      var exports = relocateExports(instance.exports, memoryBase, tableBase, moduleLocal);
      // initialize the module
      var init = exports['__post_instantiate'];
      if (init) {
        if (runtimeInitialized) {
          init();
        } else {
          // we aren't ready to run compiled code yet
          __ATINIT__.push(init);
        }
      }
      return exports;
    }

    if (flags.loadAsync) {
      return WebAssembly.instantiate(binary, info).then(function(result) {
        return postInstantiation(result.instance, moduleLocal);
      });
    } else {
      var instance = new WebAssembly.Instance(new WebAssembly.Module(binary), info);
      return postInstantiation(instance, moduleLocal);
    }
  }

  // now load needed libraries and the module itself.
  if (flags.loadAsync) {
    return Promise.all(neededDynlibs.map(function(dynNeeded) {
      return loadDynamicLibrary(dynNeeded, flags);
    })).then(function() {
      return loadModule();
    });
  }

  neededDynlibs.forEach(function(dynNeeded) {
    loadDynamicLibrary(dynNeeded, flags);
  });
  return loadModule();
}
Module['loadWebAssemblyModule'] = loadWebAssemblyModule;

#endif // WASM
#endif // RELOCATABLE

#if EMULATED_FUNCTION_POINTERS
#if WASM == 0
/** @param {Object=} module */
function getFunctionTables(module) {
  if (!module) module = Module;
  var tables = {};
  for (var t in module) {
    if (/^FUNCTION_TABLE_.*/.test(t)) {
      var table = module[t];
      if (typeof table === 'object') tables[t.substr('FUNCTION_TABLE_'.length)] = table;
    }
  }
  return tables;
}

/** @param {Object=} module */
function alignFunctionTables(module) {
  var tables = getFunctionTables(module);
  var maxx = 0;
  for (var sig in tables) {
    maxx = Math.max(maxx, tables[sig].length);
  }
  assert(maxx >= 0);
  for (var sig in tables) {
    var table = tables[sig];
    while (table.length < maxx) table.push(0);
  }
  return maxx;
}
#endif // WASM == 0

#if RELOCATABLE
// register functions from a new module being loaded
function registerFunctions(sigs, newModule) {
  sigs.forEach(function(sig) {
    if (!Module['FUNCTION_TABLE_' + sig]) {
      Module['FUNCTION_TABLE_' + sig] = [];
    }
  });
  var oldMaxx = alignFunctionTables(); // align the new tables we may have just added
  var newMaxx = alignFunctionTables(newModule);
  var maxx = oldMaxx + newMaxx;
  sigs.forEach(function(sig) {
    var newTable = newModule['FUNCTION_TABLE_' + sig];
    var oldTable = Module['FUNCTION_TABLE_' + sig];
    assert(newTable !== oldTable);
    assert(oldTable.length === oldMaxx);
    for (var i = 0; i < newTable.length; i++) {
      oldTable.push(newTable[i]);
    }
    assert(oldTable.length === maxx);
  });
  assert(maxx === alignFunctionTables()); // align the ones we didn't touch
}
// export this so side modules can use it
Module['registerFunctions'] = registerFunctions;
#endif // RELOCATABLE
#endif // EMULATED_FUNCTION_POINTERS

#include "runtime_functions.js"

var funcWrappers = {};

function getFuncWrapper(func, sig) {
  if (!func) return; // on null pointer, return undefined
  assert(sig);
  if (!funcWrappers[sig]) {
    funcWrappers[sig] = {};
  }
  var sigCache = funcWrappers[sig];
  if (!sigCache[func]) {
    // optimize away arguments usage in common cases
    if (sig.length === 1) {
      sigCache[func] = function dynCall_wrapper() {
        return dynCall(sig, func);
      };
    } else if (sig.length === 2) {
      sigCache[func] = function dynCall_wrapper(arg) {
        return dynCall(sig, func, [arg]);
      };
    } else {
      // general case
      sigCache[func] = function dynCall_wrapper() {
        return dynCall(sig, func, Array.prototype.slice.call(arguments));
      };
    }
  }
  return sigCache[func];
}

#include "runtime_debug.js"

function makeBigInt(low, high, unsigned) {
  return unsigned ? ((+((low>>>0)))+((+((high>>>0)))*4294967296.0)) : ((+((low>>>0)))+((+((high|0)))*4294967296.0));
}

/** @param {Array=} args */
function dynCall(sig, ptr, args) {
  if (args && args.length) {
#if ASSERTIONS
    // j (64-bit integer) must be passed in as two numbers [low 32, high 32].
    assert(args.length === sig.substring(1).replace(/j/g, '--').length);
#endif
#if ASSERTIONS
    assert(('dynCall_' + sig) in Module, 'bad function pointer type - no table for sig \'' + sig + '\'');
#endif
    return Module['dynCall_' + sig].apply(null, [ptr].concat(args));
  } else {
#if ASSERTIONS
    assert(sig.length == 1);
#endif
#if ASSERTIONS
    assert(('dynCall_' + sig) in Module, 'bad function pointer type - no table for sig \'' + sig + '\'');
#endif
    return Module['dynCall_' + sig].call(null, ptr);
  }
}

var tempRet0 = 0;

var setTempRet0 = function(value) {
  tempRet0 = value;
};

var getTempRet0 = function() {
  return tempRet0;
};

#if RETAIN_COMPILER_SETTINGS
var compilerSettings = {{{ JSON.stringify(makeRetainedCompilerSettings()) }}} ;

function getCompilerSetting(name) {
  if (!(name in compilerSettings)) return 'invalid compiler setting: ' + name;
  return compilerSettings[name];
}
#else // RETAIN_COMPILER_SETTINGS
#if ASSERTIONS
function getCompilerSetting(name) {
  throw 'You must build with -s RETAIN_COMPILER_SETTINGS=1 for getCompilerSetting or emscripten_get_compiler_setting to work';
}
#endif // ASSERTIONS
#endif // RETAIN_COMPILER_SETTINGS

// The address globals begin at. Very low in memory, for code size and optimization opportunities.
// Above 0 is static memory, starting with globals.
// Then the stack.
// Then 'dynamic' memory for sbrk.
var GLOBAL_BASE = {{{ GLOBAL_BASE }}};

#if RELOCATABLE
GLOBAL_BASE = alignMemory(GLOBAL_BASE, {{{ MAX_GLOBAL_ALIGN || 1 }}});
#endif

#if WASM_BACKEND && USE_PTHREADS
// JS library code refers to Atomics in the manner used from asm.js, provide
// the same API here.
var Atomics_load = Atomics.load;
var Atomics_store = Atomics.store;
var Atomics_compareExchange = Atomics.compareExchange;
#endif
