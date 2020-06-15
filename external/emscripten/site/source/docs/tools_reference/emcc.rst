.. _emccdoc:

===================================
Emscripten Compiler Frontend (emcc)
===================================

The Emscripten Compiler Frontend (``emcc``) is used to call the Emscripten compiler from the command line. It is effectively a drop-in replacement for a standard compiler like *gcc* or *clang*.


Command line syntax
===================

::

  emcc [options] file...

(Note that you will need ``./emcc`` if you want to run emcc from your current directory.)

The input file(s) can be either source code files that *Clang* can handle (C or C++), LLVM bitcode in binary form, or LLVM assembly files in human-readable form.


Arguments
---------

Most `clang options <http://linux.die.net/man/1/clang>`_ will work, as will `gcc options <https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html#Option-Summary>`_, for example: ::

  # Display this information
  emcc --help

  Display compiler version information
  emcc --version


To see the full list of *Clang* options supported on the version of *Clang* used by Emscripten, run ``clang --help``.

Options that are modified or new in *emcc* are listed below:

.. _emcc-compiler-optimization-options:

.. _emcc-O0:

``-O0``
  No optimizations (default). This is the recommended setting for starting to port a project, as it includes various assertions.

  This and other optimization settings are meaningful both during compile and
  during link. During compile it affects LLVM optimizations, and during link it
  affects final optimization of the code in Binaryen as well as optimization of
  the JS. (For fast incremental builds ``-O0`` is best, while for release you
  should link with something higher.)

.. _emcc-O1:

``-O1``
  Simple optimizations. During the compile step these include LLVM ``-O1`` optimizations. During the link step this removes various runtime assertions in JS and also runs the Binaryen optimizer (that makes link slower, so even if you compiled with a higher optimization level, you may want to link with ``-O0`` for fast incremental builds).

.. _emcc-O2:

``-O2``
  Like ``-O1``, but enables more optimizations. During link this will also enable various JavaScript optimizations.

  .. note:: These JavaScript optimizations can reduce code size by removing things that the compiler does not see being used, in particular, parts of the runtime may be stripped if they are not exported on the ``Module`` object. The compiler is aware of code in :ref:`--pre-js <emcc-pre-js>` and :ref:`--post-js <emcc-post-js>`, so you can safely use the runtime from there. Alternatively, you can use ``EXTRA_EXPORTED_RUNTIME_METHODS``, see `src/settings.js <https://github.com/emscripten-core/emscripten/blob/master/src/settings.js>`_.

.. _emcc-O3:

``-O3``
  Like ``-O2``, but with additional optimizations that may take longer to run.

  .. note:: This is a good setting for a release build.

.. _emcc-Os:

``-Os``
  Like ``-O3``, but focuses more on code size (and may make tradeoffs with speed). This can affect both wasm and JavaScript.

.. _emcc-Oz:

``-Oz``
  Like ``-Os``, but reduces code size even further, and may take longer to run. This can affect both wasm and JavaScript.

  .. note:: For more tips on optimizing your code, see :ref:`Optimizing-Code`.

.. _emcc-s-option-value:

``-s OPTION[=VALUE]``
  JavaScript code generation option passed into the Emscripten compiler. For the available options, see `src/settings.js <https://github.com/emscripten-core/emscripten/blob/master/src/settings.js>`_.

  .. note:: You can prefix boolean options with ``NO_`` to reverse them. For example, ``-s EXIT_RUNTIME=1`` is the same as ``-s NO_EXIT_RUNTIME=0``.

  .. note:: If no value is specifed it will default to ``1``.

  .. note:: For options that are lists, you need quotation marks (") around the list in most shells (to avoid errors being raised). Two examples are shown below:

    ::

      -s RUNTIME_LINKED_LIBS="['liblib.so']"
      -s "RUNTIME_LINKED_LIBS=['liblib.so']"

  You can also specify that the value of an option will be read from a specified JSON-formatted file. For example, the following option sets the ``DEAD_FUNCTIONS`` option with the contents of the file at **path/to/file**.

  ::

    -s DEAD_FUNCTIONS=@/path/to/file

  .. note::

    - In this case the file might contain a JSON-formatted list of functions: ``["_func1", "func2"]``.
    - The specified file path must be absolute, not relative.

.. _emcc-g:

``-g``
  Preserve debug information.

  - When compiling to object files, this is the same as in *Clang* and *gcc*, it adds debug information to the object files.
  - When linking, this is equivalent to :ref:`-g3 <emcc-g3>`.

``-gseparate-dwarf[=FILENAME]``
  Preserve debug information, but in a separate file on the side. This is the
  same as ``-g``, but the main file will contain no debug info, while debug
  info will be present in a file on the side (``FILENAME`` if provided,
  otherwise the same as the wasm file but with suffix ``.debug.wasm``).

.. _emcc-gN:

``-g<level>``
  Controls the level of debuggability. Each level builds on the previous one:

    -
      .. _emcc-g0:

      ``-g0``: Make no effort to keep code debuggable.

    -
      .. _emcc-g1:

      ``-g1``: When linking, preserve whitespace in JavaScript.

    -
      .. _emcc-g2:

      ``-g2``: When linking, preserve function names in compiled code.

    -
      .. _emcc-g3:

      ``-g3``: When compiling to object files, keep debug info, including JS whitespace, function names, and LLVM debug info if any (this is the same as :ref:`-g <emcc-g>`).

    .. _emcc-g4:

    - ``-g4``: When linking, generate a source map using LLVM debug information (which must be present in object files, i.e., they should have been compiled with ``-g``).

      .. note::

        - Source maps allow you to view and debug the *C/C++ source code* in your browser's debugger!
        - This debugging level may make compilation significantly slower (this is why we only do it on ``-g4``).

.. _emcc-profiling:

``--profiling``
  Use reasonable defaults when emitting JavaScript to make the build readable but still useful for profiling. This sets ``-g2`` (preserve whitespace and function names) and may also enable optimizations that affect performance and otherwise might not be performed in ``-g2``.

``--profiling-funcs``
  Preserve function names in profiling, but otherwise minify whitespace and names as we normally do in optimized builds. This is useful if you want to look at profiler results based on function names, but do *not* intend to read the emitted code.

``--tracing``
  Enable the :ref:`Emscripten Tracing API <trace-h>`.

.. _emcc-emit-symbol-map:

``--emit-symbol-map``
  Save a map file between the minified global names and the original function names. This allows you, for example, to reconstruct meaningful stack traces.

  .. note:: This is only relevant when :term:`minifying` global names, which happens in ``-O2`` and above, and when no ``-g`` option was specified to prevent minification.

.. _emcc-js-opts:

``--js-opts <level>``
  Enables JavaScript optimizations, relevant when we generate JavaScript. Possible ``level`` values are:

    - ``0``: Prevent JavaScript optimizer from running.
    - ``1``: Use JavaScript optimizer (default).

  You normally don't need to specify this option, as ``-O`` with an optimization level will set a good value.

  .. note:: Some options might override this flag (e.g. ``EMTERPRETIFY``, ``DEAD_FUNCTIONS``, ``SAFE_HEAP`` and ``SPLIT_MEMORY`` override the value with ``js-opts=1``), because they depend on the js-optimizer.

.. _emcc-llvm-opts:

``--llvm-opts <level>``
  Enables LLVM optimizations, relevant when we call the LLVM optimizer (which is done when building source files to object files / bitcode). Possible ``level`` values are:

    - ``0``: No LLVM optimizations (default in -O0).
    - ``1``: LLVM ``-O1`` optimizations (default in -O1).
    - ``2``: LLVM ``-O2`` optimizations.
    - ``3``: LLVM ``-O3`` optimizations (default in -O2+).

  You can also specify arbitrary LLVM options, e.g.::

    --llvm-opts "['-O3', '-somethingelse']"

  You normally don't need to specify this option, as ``-O`` with an optimization level will set a good value.

.. _emcc-llvm-lto:

``--llvm-lto <level>``
  Enables LLVM link-time optimizations (LTO). Possible ``level`` values are:

    - ``0``: No LLVM LTO (default).
    - ``1``: LLVM LTO is performed.
    - ``2``: Combine all the bitcode and run LLVM opt on it using the specified ``--llvm-opts``. This optimizes across modules, but is not the same as normal LTO.
    - ``3``: Does level ``2`` and then level ``1``.

  .. note::

    - If LLVM optimizations are not run (see ``--llvm-opts``), this setting has no effect.
    - LLVM LTO is not perfectly stable yet, and can cause code to behave incorrectly.

.. _emcc-closure:

``--closure <on>``
  Runs the :term:`Closure Compiler`. Possible ``on`` values are:

    - ``0``: No closure compiler (default in ``-O2`` and below).
    - ``1``: Run closure compiler. This greatly reduces the size of the support JavaScript code (everything but the WebAssembly or asm.js). Note that this increases compile time significantly.
    - ``2``: Run closure compiler on *all* the emitted code, even on **asm.js** output in **asm.js** mode. This can further reduce code size, but does prevent a significant amount of **asm.js** optimizations, so it is not recommended unless you want to reduce code size at all costs.

  .. note::

    - Consider using ``-s MODULARIZE=1`` when using closure, as it minifies globals to names that might conflict with others in the global scope. ``MODULARIZE`` puts all the output into a function (see ``src/settings.js``).
    - Closure will minify the name of `Module` itself, by default! Using ``MODULARIZE`` will solve that as well. Another solution is to make sure a global variable called `Module` already exists before the closure-compiled code runs, because then it will reuse that variable.
    - If closure compiler hits an out-of-memory, try adjusting ``JAVA_HEAP_SIZE`` in the environment (for example, to 4096m for 4GB).
    - Closure is only run if JavaScript opts are being done (``-O2`` or above, or ``--js-opts 1``).


.. _emcc-pre-js:

``--pre-js <file>``
  Specify a file whose contents are added before the emitted code and optimized together with it. Note that this might not literally be the very first thing in the JS output, for example if ``MODULARIZE`` is used (see ``src/settings.js``). If you want that, you can just prepend to the output from emscripten; the benefit of ``--pre-js`` is that it optimizes the code with the rest of the emscripten output, which allows better dead code elimination and minification, and it should only be used for that purpose. In particular, ``--pre-js`` code should not alter the main output from emscripten in ways that could confuse the optimizer, such as using ``--pre-js`` + ``--post-js`` to put all the output in an inner function scope (see ``MODULARIZE`` for that).

  `--pre-js` (but not `--post-js`) is also useful for specifying things on the ``Module`` object, as it appears before the JS looks at ``Module`` (for example, you can define ``Module['print']`` there).

.. _emcc-post-js:

``--post-js <file>``
  Like ``--pre-js``, but emits a file *after* the emitted code.

``--extern-pre-js <file>``
  Specify a file whose contents are prepended to the JavaScript output. This
  file is prepended to the final JavaScript output, *after* all other
  work has been done, including optimization, optional ``MODULARIZE``-ation,
  instrumentation like ``SAFE_HEAP``, etc. This is the same as prepending
  this file after ``emcc`` finishes running, and is just a convenient
  way to do that. (For comparison, ``--pre-js`` and ``--post-js`` optimize the
  code together with everything else, keep it in the same scope if running
  `MODULARIZE`, etc.).

``--extern-post-js <file>``
  Like ``--extern-pre-js``, but appends to the end.

.. _emcc-embed-file:

``--embed-file <file>``
  Specify a file (with path) to embed inside the generated JavaScript. The path is relative to the current directory at compile time. If a directory is passed here, its entire contents will be embedded.

  For example, if the command includes ``--embed-file dir/file.dat``, then ``dir/file.dat`` must exist relative to the directory where you run *emcc*.

  .. note:: Embedding files is much less efficient than :ref:`preloading <emcc-preload-file>` them. You should only use it for small files, in small numbers. Instead use ``--preload-file``, which emits efficient binary data.

  For more information about the ``--embed-file`` options, see :ref:`packaging-files`.

.. _emcc-preload-file:

``--preload-file <name>``
  Specify a file to preload before running the compiled code asynchronously. The path is relative to the current directory at compile time. If a directory is passed here, its entire contents will be embedded.

  Preloaded files are stored in **filename.data**, where **filename.html** is the main file you are compiling to. To run your code, you will need both the **.html** and the **.data**.

  .. note:: This option is similar to :ref:`--embed-file <emcc-embed-file>`, except that it is only relevant when generating HTML (it uses asynchronous binary :term:`XHRs <XHR>`), or JavaScript that will be used in a web page.

  *emcc* runs `tools/file_packager.py <https://github.com/emscripten-core/emscripten/blob/master/tools/file_packager.py>`_ to do the actual packaging of embedded and preloaded files. You can run the file packager yourself if you want (see :ref:`packaging-files-file-packager`). You should then put the output of the file packager in an emcc ``--pre-js``, so that it executes before your main compiled code.

  For more information about the ``--preload-file`` options, see :ref:`packaging-files`.


.. _emcc-exclude-file:

``--exclude-file <name>``
  Files and directories to be excluded from :ref:`--embed-file <emcc-embed-file>` and :ref:`--preload-file <emcc-preload-file>`. Wildcards (*) are supported.

``--use-preload-plugins``
  Tells the file packager to run preload plugins on the files as they are loaded. This performs tasks like decoding images and audio using the browser's codecs.

.. _emcc-shell-file:

``--shell-file <path>``
  The path name to a skeleton HTML file used when generating HTML output. The shell file used needs to have this token inside it: ``{{{ SCRIPT }}}``.

  .. note::

    - See `src/shell.html <https://github.com/emscripten-core/emscripten/blob/master/src/shell.html>`_ and `src/shell_minimal.html <https://github.com/emscripten-core/emscripten/blob/master/src/shell_minimal.html>`_ for examples.
    - This argument is ignored if a target other than HTML is specified using the ``-o`` option.

.. _emcc-source-map-base:

``--source-map-base <base-url>``
  The URL for the location where WebAssembly source maps will be published. When this option is provided, the **.wasm** file is updated to have a ``sourceMappingURL`` section. The resulting URL will have format: ``<base-url>`` + ``<wasm-file-name>`` + ``.map``.

.. _emcc-minify:

``--minify 0``
  Identical to ``-g1``.

``--js-transform <cmd>``
  Specifies a ``<cmd>`` to be called on the generated code before it is optimized. This lets you modify the JavaScript, for example adding or removing some code, in a way that those modifications will be optimized together with the generated code.

  ``<cmd>`` will be called with the file name of the generated code as a parameter. To modify the code, you can read the original data and then append to it or overwrite it with the modified data.

  ``<cmd>`` is interpreted as a space-separated list of arguments, for example, ``<cmd>`` of **python processor.py** will cause a Python script to be run.

.. _emcc-bind:

``--bind``
  Compiles the source code using the :ref:`embind` bindings to connect C/C++ and JavaScript.

``--ignore-dynamic-linking``
  Tells the compiler to ignore dynamic linking (the user will need to manually link to the shared libraries later on).

  Normally *emcc* will simply link in code from the dynamic library as though it were statically linked, which will fail if the same dynamic library is linked more than once. With this option, dynamic linking is ignored, which allows the build system to proceed without errors.

.. _emcc-js-library:

``--js-library <lib>``
  A JavaScript library to use in addition to those in Emscripten's core libraries (src/library_*).

.. _emcc-verbose:

``-v``
  Turns on verbose output.

  This will pass ``-v`` to *Clang*, and also enable ``EMCC_DEBUG`` to generate intermediate files for the compiler's various stages. It will also run Emscripten's internal sanity checks on the toolchain, etc.

  .. tip:: ``emcc -v`` is a useful tool for diagnosing errors. It works with or without other arguments.

.. _emcc-cache:

``--cache``
  Sets the directory to use as the Emscripten cache. The Emscripten cache
  is used to store pre-built versions of ``libc``, ``libcxx`` and other
  libraries.

  If using this in combination with ``--clear-cache``, be sure to specify
  this argument first.

  The Emscripten cache defaults to being located in the path name stored
  in the ``EM_CACHE`` environment variable or ``~/.emscripten_cache``.

.. _emcc-clear-cache:

``--clear-cache``
  Manually clears the cache of compiled Emscripten system libraries (libc++, libc++abi, libc).

  This is normally handled automatically, but if you update LLVM in-place (instead of having a different directory for a new version), the caching mechanism can get confused. Clearing the cache can fix weird problems related to cache incompatibilities, like *Clang* failing to link with library files. This also clears other cached data. After the cache is cleared, this process will exit.

.. _emcc-clear-ports:

``--clear-ports``
  Manually clears the local copies of ports from the Emscripten Ports repos (sdl2, etc.). This also clears the cache, to remove their builds.

  You should only need to do this if a problem happens and you want all ports that you use to be downloaded and built from scratch. After this operation is complete, this process will exit.

.. _emcc-show-ports:

``--show-ports``
  Shows the list of available projects in the Emscripten Ports repos. After this operation is complete, this process will exit.

.. _emcc-save-bc:

``--save-bc PATH``
  When compiling to JavaScript or HTML, this option will save a copy of the bitcode to the specified path. The bitcode will include all files being linked after link-time optimizations have been performed (if any), including standard libraries.

.. _emcc-memory-init-file:

``--memory-init-file <on>``
  Specifies whether to emit a separate memory initialization file.

      .. note:: Note that this is only relevant when *not* emitting wasm, as wasm embeds the memory init data in the wasm binary.

  Possible ``on`` values are:

    - ``0``: Do not emit a separate memory initialization file. Instead keep the static initialization inside the generated JavaScript as text. This is the default setting if compiling with -O0 or -O1 link-time optimization flags.
    - ``1``: Emit a separate memory initialization file in binary format. This is more efficient than storing it as text inside JavaScript, but does mean you have another file to publish. The binary file will also be loaded asynchronously, which means ``main()`` will not be called until the file is downloaded and applied; you cannot call any C functions until it arrives. This is the default setting when compiling with -O2 or higher.

      .. note:: The :ref:`safest way <faq-when-safe-to-call-compiled-functions>` to ensure that it is safe to call C functions (the initialisation file has loaded) is to call a notifier function from ``main()``.

      .. note:: If you assign a network request to ``Module.memoryInitializerRequest`` (before the script runs), then it will use that request instead of automatically starting a download for you. This is beneficial in that you can, in your HTML, fire off a request for the memory init file before the script actually arrives. For this to work, the network request should be an XMLHttpRequest with responseType set to ``'arraybuffer'``. (You can also put any other object here, all it must provide is a ``.response`` property containing an ArrayBuffer.)


``-Wwarn-absolute-paths``
  Enables warnings about the use of absolute paths in ``-I`` and ``-L`` command line directives. This is used to warn against unintentional use of absolute paths, which is sometimes dangerous when referring to nonportable local system headers.

.. _proxy-to-worker:

``--proxy-to-worker``
  Runs the main application code in a worker, proxying events to it and output from it. If emitting HTML, this emits a **.html** file, and a separate **.js** file containing the JavaScript to be run in a worker. If emitting JavaScript, the target file name contains the part to be run on the main thread, while a second **.js** file with suffix ".worker.js" will contain the worker portion.

.. _emcc-emrun:

``--emrun``
  Enables the generated output to be aware of the :ref:`emrun <Running-html-files-with-emrun>` command line tool. This allows ``stdout``, ``stderr`` and ``exit(returncode)`` capture when running the generated application through *emrun*. (This enables `EXIT_RUNTIME=1`, allowing normal runtime exiting with return code passing.)

``--cpuprofiler``
  Embeds a simple CPU profiler onto the generated page. Use this to perform cursory interactive performance profiling.

``--memoryprofiler``
  Embeds a memory allocation tracker onto the generated page. Use this to profile the application usage of the Emscripten HEAP.

``--threadprofiler``
  Embeds a thread activity profiler onto the generated page. Use this to profile the application usage of pthreads when targeting multithreaded builds (-s USE_PTHREADS=1/2).

.. _emcc-config:

``--em-config``
  Specifies the location of the **.emscripten** configuration file for the current compiler run. If not specified, the environment variable ``EM_CONFIG`` is first read for this location. If neither are specified, the default location **~/.emscripten** is used.

``--default-obj-ext .ext``
  Specifies the file suffix to generate if the location of a directory name is passed to the ``-o`` directive.

  For example, consider the following command, which will by default generate an output name **dir/a.o**. With ``--default-obj-ext .ext`` the generated file has the custom suffix *dir/a.ext*.

  ::

    emcc -c a.c -o dir/


``--valid-abspath path``
  Whitelist an absolute path to prevent warnings about absolute include paths.

.. _emcc-o-target:

``-o <target>``
  The ``target`` file name extension defines the output type to be generated:

    - <name> **.js** : JavaScript (+ separate **<name>.wasm** file if emitting WebAssembly). (default)
    - <name> **.mjs** : ES6 JavaScript module (+ separate **<name>.wasm** file if emitting WebAssembly).
    - <name> **.html** : HTML + separate JavaScript file (**<name>.js**; + separate **<name>.wasm** file if emitting WebAssembly).
    - <name> **.bc** : LLVM bitcode.
    - <name> **.o** : WebAssembly object file (unless fastcomp or -flto is used in which case it will be in LLVM bitcode format).
    - <name> **.wasm** : WebAssembly without JavaScript support code ("standalone wasm"; this enables ``STANDALONE_WASM``).

  .. note:: If ``--memory-init-file`` is used, a **.mem** file will be created in addition to the generated **.js** and/or **.html** file.

.. _emcc-c:

``-c``
  Tells *emcc* to generate LLVM bitcode (which can then be linked with other bitcode files), instead of compiling all the way to JavaScript.

``--separate-asm``
  Emits asm.js in one file, and the rest of the code in another, and emits HTML that loads the asm.js first, in order to reduce memory load during startup. See :ref:`optimizing-code-separating_asm`.

``--output_eol windows|linux``
  Specifies the line ending to generate for the text files that are outputted. If "--output_eol windows" is passed, the final output files will have Windows \r\n line endings in them. With "--output_eol linux", the final generated files will be written with Unix \n line endings.

``--cflags``
  Prints out the flags ``emcc`` would pass to ``clang`` to compile source code to object/bitcode form. You can use this to invoke clang yourself, and then run ``emcc`` on those outputs just for the final linking+conversion to JS.

.. _emcc-environment-variables:

Environment variables
=====================

*emcc* is affected by several environment variables, as listed below:

  - ``EMMAKEN_JUST_CONFIGURE``
  - ``EMMAKEN_COMPILER``
  - ``EMMAKEN_CFLAGS``
  - ``EMCC_DEBUG``
  - ``EMCC_CLOSURE_ARGS`` : arguments to be passed to *Closure Compiler*

Search for 'os.environ' in `emcc.py <https://github.com/emscripten-core/emscripten/blob/master/emcc.py>`_ to see how these are used. The most interesting is possibly ``EMCC_DEBUG``, which forces the compiler to dump its build and temporary files to a temporary directory where they can be reviewed.


.. todo:: In case we choose to document them properly in future, below are some of the :ref:`-s <emcc-s-option-value>` options that are documented in the site are listed below. Note that this is not exhaustive by any means:

  - ``-s FULL_ES2=1``
  - ``-s LEGACY_GL_EMULATION=1``:

    - ``-s GL_UNSAFE_OPTS=1``
    - ``-s GL_FFP_ONLY=1``

  - ASSERTIONS
  - SAFE_HEAP
  - AGGRESSIVE_VARIABLE_ELIMINATION=1
  - -s DISABLE_EXCEPTION_CATCHING=0.
  - INLINING_LIMIT=

