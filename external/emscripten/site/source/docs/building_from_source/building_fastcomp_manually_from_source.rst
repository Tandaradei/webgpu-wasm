.. _building-fastcomp-from-source:

======================================
Manually building Fastcomp from source
======================================

Fastcomp is the second compiler core used in Emscripten, after the original JS compiler and before the new LLVM wasm backend. We recommend the LLVM wasm backend, but fastcomp is still useful in some cases, and is installable using the emsdk. This article explains how you can build Fastcomp's sources using a fully manual process.

.. note:: If you are building a large project, you will need to create a 64-bit build of LLVM and Clang. Compiling and optimizing can take more memory than is available to the 32-bit build.


What you'll need
================

First verify you have :ref:`the compiler toolchain <compiler-toolchain>` for your platform, download and install it as necessary.

Then follow the instructions for your platform showing how to :ref:`manually build Emscripten from source <installing-from-source>`. This page helps you with the specific task of building fastcomp (LLVM + clang).


.. _building-fastcomp-from-source-building:

Building Fastcomp
=================

To build the Fastcomp code from source:

-  Create a directory to store the build. It doesn't matter where, because Emscripten gets the information from the :ref:`compiler configuration file (~/.emscripten) <compiler-configuration-file>`. We show how to update this file later in these instructions:

  ::

    mkdir myfastcomp
    cd myfastcomp


- Clone the fastcomp LLVM repository (https://github.com/emscripten-core/emscripten-fastcomp):

  ::

    git clone https://github.com/emscripten-core/emscripten-fastcomp



- Clone the `emscripten-core/emscripten-fastcomp-clang <https://github.com/emscripten-core/emscripten-fastcomp-clang>`_ repository into **emscripten-fastcomp/tools/clang**:

  ::

    cd emscripten-fastcomp
    git clone https://github.com/emscripten-core/emscripten-fastcomp-clang tools/clang

  .. warning:: You **must** clone it into a directory named **clang** as shown, so that :term:`Clang` is present in **tools/clang**!

- Create a *build* directory (inside the **emscripten-fastcomp** directory) and then navigate into it:

  ::

    mkdir build
    cd build

- Configure the build using *cmake*:

  ::

    cmake .. -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="host;JSBackend" -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_TESTS=OFF

  .. note:: On Windows you will need Visual Studio 2015 or newer to build.

- Determine the number of available cores on your system (Emscripten can run many operations in parallel, so using more cores may have a significant impact on compilation time):

  - On macOS you can get the number of cores using: **Apple menu | About this mac | More info | System report**. The **Hardware overview** on the resulting dialog includes a *Total number of cores* entry.
  - On Linux you can find the number of cores by entering the following command on the terminal: ``cat /proc/cpuinfo | grep "^cpu cores" | uniq``.
  - On Windows the number of cores is listed on the **Task Manager | Performance Tab**. You can open the *Task Manager* by entering **Ctrl + Shift + Esc** from the Desktop.

- Call *make* to build the sources, specifying the number of available cores:

  ::

    make -j4

  .. note:: If the build completes successfully, *clang*, *clang++*, and a number of other files will be created in the release directory (**<LLVM root>/build/Release/bin**).


.. _llvm-update-compiler-configuration-file:

-

  The final step is to update the :ref:`~/.emscripten <compiler-configuration-file>` file, specifying the location of *fastcomp* in the ``LLVM_ROOT`` variable.

  .. note:: If you're building the **whole** of Emscripten from source, following the platform-specific instructions in :ref:`installing-from-source`, you won't yet have Emscripten installed. In this case, skip this step and return to those instructions.

  If you already have an Emscripten environment (for example if you're building Fastcomp using the SDK), then set ``LLVM_ROOT`` to the location of the *clang* binary under the **build** directory. This will be something like **<LLVM root>/build/Release/bin** or **<LLVM root>/build/bin**:

    .. code-block:: none

      LLVM_ROOT='/home/ubuntu/yourpath/emscripten-fastcomp/build/bin'

.. _building-fastcomp-from-source-branches:

Branches
---------

You should use the **same** branch (*incoming*, or *master*) for building all three repositories:

- Emscripten: `emscripten <https://github.com/emscripten-core/emscripten>`_.
- Emscripten's LLVM fork: `emscripten-fastcomp <https://github.com/emscripten-core/emscripten-fastcomp>`_.
- Emscripten's *Clang* fork `emscripten-fastcomp-clang <https://github.com/emscripten-core/emscripten-fastcomp-clang>`_.

Mixing *incoming* and *master* branches may result in errors when building the three repositories.

Run ``emcc -v`` to check if the branches are synchronized.

.. note:: ``emcc -v`` checks the code in the repositories, not the builds. Before building make sure that you fetch the latest changes to LLVM and Clang.

Version numbers
++++++++++++++++

Bisecting across multiple git trees can be hard. We use version numbers to help synchronize points between them:

- `emscripten-version.txt <https://github.com/emscripten-core/emscripten/blob/master/emscripten-version.txt>`__ in Emscripten
- `emscripten-version.txt <https://github.com/emscripten-core/emscripten-fastcomp/blob/master/emscripten-version.txt>`__ in fastcomp (llvm)
- `emscripten-version.txt <https://github.com/emscripten-core/emscripten-fastcomp-clang/blob/master/emscripten-version.txt>`__ in fastcomp-clang (clang)

Version numbers are typically ``X.Y.Z`` where.


Backend code structure
----------------------

The backend is in `emscripten-fastcomp <https://github.com/emscripten-core/emscripten-fastcomp>`_. The main file is `JSBackend.cpp <https://github.com/emscripten-core/emscripten-fastcomp/blob/incoming/lib/Target/JSBackend/JSBackend.cpp>`_ but the other files in that directory (`lib/Target/JSBackend/ <https://github.com/emscripten-core/emscripten-fastcomp/tree/incoming/lib/Target/JSBackend>`_) are also important.


