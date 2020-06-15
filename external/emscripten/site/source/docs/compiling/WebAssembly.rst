.. _WebAssembly:

=======================
Building to WebAssembly
=======================

WebAssembly is a binary format for executing code on the web, allowing fast start times (smaller download and much faster parsing in browsers when compared to JS or asm.js). Emscripten compiles to WebAssembly by default, but you can also compile to JS for older browsers.

For some historical background, see

- `these slides <https://kripken.github.io/talks/wasm.html>`_ and
- `this blogpost <https://hacks.mozilla.org/2015/12/compiling-to-webassembly-its-happening/>`_.

Setup
=====

WebAssembly is emitted by default, without the need for any special flags.

.. note:: If you **don't** want WebAssembly, you can disable it with something like

::

  emcc [..args..] -s WASM=0

.. note:: Emscripten's WebAssembly support depends on `Binaryen <https://github.com/WebAssembly/binaryen>`_, which is provided by the emsdk (if you don't use the emsdk, you need to build it and set it up in your ``.emscripten`` file).
.. note:: Deciding to compile to wasm or JS can be done at the linking stage: it doesn't affect the object files.

Backends
--------

Emscripten emits WebAssembly using the **upstream LLVM wasm backend**, since
version ``1.39.0`` (October 2019), and the old **fastcomp** backend is
deprecated (you can still use the deprecated fastcomp backend by getting
``latest-fastcomp`` instead of the normal ``latest``, or ``1.39.0-fastcomp``
instead of ``1.39.0``, etc.).

There are some differences you may notice between the two backends, if you
upgrade from fastcomp to upstream:

* The wasm backend is strict about linking files with different features sets -
  for example, if one file was built with atomics but another was not, it will
  error at link time. This prevents possible bugs, but may mean you need to make
  some build system fixes.

* ``WASM=0`` behaves differently in the two backends. In fastcomp we emit
  asm.js, while in upstream we emit JS (since not all wasm constructs can be
  expressed in asm.js). Also, the JS support implements the same external
  ``WebAssembly.*`` API, so in particular startup will be async just like wasm
  by default, and you can control that with ``WASM_ASYNC_COMPILATION`` (even
  though ``WASM=0``).

* The wasm backend uses wasm object files by default. That means that it does
  codegen at the compile step, which makes the link step much faster - like a
  normal native compiler. For comparison, in fastcomp the compile step emits
  LLVM IR in object files.

  * You normally wouldn't notice this, but some compiler flags affect codegen,
    like ``DISABLE_EXCEPTION_CATCHING``. Such flags must be passed during
    codegen. The simple and safe thing is to pass all ``-s`` flags at both
    compile and link time.

  * You can enable Link Time Optimization (LTO) with the usual llvm flags
    (``-flto``, ``-flto=full``, ``-flto=thin``, at both compile and link times).
    These flags will make the wasm backend behave more like fastcomp.

  * With fastcomp LTO optimization passes will not be run by default;
    for that you must pass ``--llvm-lto 1``.  With the llvm backend
    LTO passes will be run on any object files that are in bitcode format.

  * Another thing you might notice is that fastcomp's link stage is able to
    perform some minor types of link time optimization even without LTO being
    set. The LLVM backend requires actually setting LTO for those things.

* `wasm-ld`, the linker used by the wasm backend, requires libraries (`.a`
  archives) to contain symbol indexes.  This matches the behaviour the native
  GNU linker.  While `emar` will create such indexes by default, native tools
  such as GNU `ar` and GNU `strip` are not aware of the WebAssembly object
  format and cannot create archive indexes.  In particular, if you run GNU
  `strip` on an archive file that contains WebAssembly object files it will
  remove the index which makes the archive unusable at link time.

* Fastcomp emits asm.js and so has some limitations on function pointers. For
  example, the ``RESERVED_FUNCTION_POINTERS`` setting exists there to work
  around the fact that we can't grow the table. In the upstream backend table
  growth is easy, and you can just enable ``ALLOW_TABLE_GROWTH``.

* Fastcomp and upstream use very different LLVM and clang versions (fastcomp
  has been stuck on LLVM 6, upstream is many releases after). This affects
  optimizations, usually by making the upstream version faster and smaller.
  However, in rare cases you may see a regression (for example, in some cases
  *UN*-optimized code may be
  `less optimal in upstream <https://github.com/emscripten-core/emscripten/issues/10753#issuecomment-603486677>`_,
  so make sure to optimize both when compiling and when linking).

* Also see the `blocker bugs on the wasm backend <https://github.com/emscripten-core/emscripten/projects/1>`_, and the `wasm backend tagged issues <https://github.com/emscripten-core/emscripten/issues?utf8=✓&q=is%3Aissue+is%3Aopen+label%3A"LLVM+wasm+backend">`_.

Binaryen codegen options
========================

Trapping
--------

WebAssembly can trap - throw an exception - on things like division by zero, rounding a very large float to an int, and so forth. In asm.js such things were silently ignored, as in JavaScript they do not throw, so this is a difference between JavaScript and WebAssembly that you may notice, with the browser reporting an error like ``float unrepresentable in integer range``, ``integer result unrepresentable``, ``integer overflow``, or ``Out of bounds Trunc operation``.


Fastcomp/asm2wasm
~~~~~~~~~~~~~~~~~

In fastcomp/asm2wasm, emscripten will emit code that is optimized for size and speed, which means it emits code that may trap on the things mentioned before. That mode is called ``allow``. The other modes are ``clamp``, which will avoid traps by clamping values to a reasonable range, and ``js``, which ensures the exact same behavior as JavaScript does (which also does clamping, but makes sure to clamp exactly like JavaScript does, and also do other things JavaScript would).

In general, using ``clamp`` is safest, as whether such a trap occurs depends on how the LLVM optimizer optimizes code. In other words, there is no guarantee that this will not be an issue, and updating LLVM can make a problem appear or vanish (the wasm spec process has recognized this problem and intends to standardize `new operations that avoid it <https://github.com/WebAssembly/design/issues/1143>`_). Also, there is not a big downside to using ``clamp``: it is only slightly larger and slower than the default ``allow``, in most cases. To do so, build with

 ::

  -s "BINARYEN_TRAP_MODE='clamp'"


However, if the default (to allow traps) works in your codebase, then it may be worth keeping it that way, for the (small) benefits. Note that ``js``, which preserves the exact same behavior as JavaScript does, adds a large amount of overhead, so unless you really need that, use ``clamp`` (``js`` is often useful for debugging, though).

LLVM wasm backend
~~~~~~~~~~~~~~~~~

The LLVM wasm backend avoids traps by adding more code around each possible trap (basically clamping the value if it would trap). This can increase code size and decrease speed, if you don't need that extra code. The proper solution for this is to use newer wasm instructions that do not trap, by calling emcc or clang with ``-mnontrapping-fptoint``. That code may not run in older VMs, though.

Compiler output
===============

When using ``emcc`` to build to WebAssembly, you will see a ``.wasm`` file containing that code, as well as the usual ``.js`` file that is the main target of compilation. Those two are built to work together: run the ``.js`` (or ``.html``, if that's what you asked for) file, and it will load and set up the WebAssembly code for you, properly setting up imports and exports for it, etc. Basically, you don't need to care about whether the compiled code is asm.js or WebAssembly, it's just a compiler flag, and otherwise everything should just work (except the WebAssembly should be faster).

- Note that the ``.wasm`` file is not standalone - it's not easy to manually run it without that ``.js`` code, as it depends on getting the proper imports that integrate with JS. For example, it receives imports for syscalls so that it can do things like print to the console. There is work in progress towards ways to create standalone ``.wasm`` files, see the `WebAssembly Standalone page <https://github.com/emscripten-core/emscripten/wiki/WebAssembly-Standalone>`_.

You may also see additional files generated, like a ``.data`` file if you are preloading files into the virtual filesystem. All that is exactly the same as when building to asm.js. One difference you may notice is the lack of a ``.mem file``, which for asm.js contains the static memory initialization data, which in WebAssembly we can pack more efficiently into the WebAssembly binary itself.

Testing native WebAssembly in browsers
======================================

WebAssembly support is enabled by default as of Firefox 52, Chrome 57 and Opera 44. On Edge 15 you can enable it via "Experimental JavaScript Features" flag.

Debugging
=========

asm.js support is considered very stable now, and you can change between it and wasm with ``-s WASM=0``, so if you see something odd in a wasm build, comparing to a parallel asm.js build can help. In general, any difference between the two could be a compiler bug or browser bug, but there are a few legitimate causes of different behavior between the two, that you may want to rule out:

- wasm allows unaligned accesses, i.e. it will load 4 bytes from an unaligned address the same way x86 does (it doesn't care it's unaligned). asm.js works more like ARM CPUs which mostly don't accept such things (but they often trap, while asm.js just returns a wrong result). To rule this out, you can build with ``-s SAFE_HEAP=1``, that will catch all such invalid accesses.
- Timing issues - wasm might run faster or slower. To some extent you can mitigate that by building with ``-s DETERMINISTIC=1``.
- Trap mode. As mentioned above, we can generate wasm that traps or that avoids traps. Make sure the trap mode is ``"js"`` when comparing builds. The ``"js"`` trap mode is also useful in a single build, as otherwise operations like division or float-to-int may trap, and the optimizer may happen to change whether a trap occurs or not, which can be confusing (for example, enabling ``SAFE_HEAP`` may prevent some optimizations, and a trap may start to occur). Instead, in the ``"js"`` trap mode there are no traps and all operations are deterministically defined as identical to JavaScript.
- Minor libc and runtime differences exist between wasm and asm.js. We used to have a way to emit more compatable builds (``-s "BINARYEN_METHOD='asmjs,native-wasm'"`` etc.) but due to its complexity and low value it was removed.
- Floating-point behavior: WebAssembly uses 32-bit floats in a standard way, while asm.js by default implements floats using doubles. That can lead to differences in the precision of results. You can force 32-bit float behavior in asm.js with ``-s PRECISE_F32=1``, in which case it should be identical to wasm.
- Browser instability: It's worth testing multiple browsers, as one might have a wasm bug that another doesn't. You can also test the Binaryen interpreter (e.g. using the ``interpret-binary`` method, as discussed above).

If you find that an asm.js build has the same behavior as a wasm one, then it is currently easier to debug the asm.js build: you can edit the source easily (add debug printouts, etc.), there is debug info and source maps support, etc.

Debugging WebAssembly
---------------------

When you do need to debug a WebAssembly build, the following tips might help you.

WebAssembly doesn't have source maps support yet, but building with ``-g`` will emit both a text and a binary wasm, and it will include function names in both, and also include source file and line number information in the text, for example, building hello world might produce this ``.wat``:

.. code-block:: none

    ;; tests/hello_world.c:4
    (drop
      (call $_printf
        (i32.const 1144)
        (get_local $$vararg_buffer)
      )
    )
    ;; tests/hello_world.c:5
    (return
      (i32.const 0)
    )

This indicates that the ``printf`` call comes from line 4, and the return from line 5, of ``hello_world.c``.

``.wasm`` files and compilation
===============================

WebAssembly code is prepared somewhat differently than asm.js. asm.js can be bundled inside the main JS file, while as mentioned earlier WebAssembly is a binary file on the side, so you will have more than one file to distribute.

Another noticeable effect is that WebAssembly is compiled asynchronously by default, which means you must wait for compilation to complete before calling compiled code (by waiting for ``main()``, or the ``onRuntimeInitialized`` callback, etc., which you also need to do when you have anything else that makes startup async, like a ``.mem`` file for asm.js, or preloaded file data, etc.). You can turn off async compilation by setting ``WASM_ASYNC_COMPILATION=0``, but that may not work in Chrome due to current limitations there.

- Note that even with async compilation turned off, fetching the WebAssembly binary may need to be an asynchronous operation (since the Web does not allow synchronous binary downloads on the main thread). If you can fetch the binary yourself, you can set ``Module['wasmBinary']`` and it will be used from there, and then (with async compilation off) compilation should be synchronous.

Web server setup
================

To serve wasm in the most efficient way over the network, make sure your web server has the proper MIME time for ``.wasm`` files, which is application/wasm. That will allow streaming compilation, where the browser can start to compile code as it downloads.

In Apache, you can do this with

.. code-block:: none

    AddType application/wasm .wasm

Also make sure that gzip is enabled:

.. code-block:: none

    AddOutputFilterByType DEFLATE application/wasm

If you serve large ``.wasm`` files, the webserver will consume CPU compressing them on the fly at each request.
Instead you can pre-compress them to ``.wasm.gz`` and use content negotiation:

.. code-block:: none

    Options Multiviews
    RemoveType .gz
    AddEncoding x-gzip .gz
    AddType application/wasm .wasm

