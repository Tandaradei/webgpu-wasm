#!/usr/bin/env python3
# Copyright 2014 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

"""Tool to manage building of system libraries and other components.
such as ports and the native optimizer.

In general emcc will build them automatically on demand, so you do not
strictly need to use this tool, but it gives you more control over the
process (in particular, if emcc does this automatically, and you are
running multiple build commands in parallel, confusion can occur).
"""

from __future__ import print_function
import argparse
import logging
import os
import subprocess
import sys

from tools import shared
from tools import system_libs

C_BARE = 'int main() {}'

SYSTEM_LIBRARIES = system_libs.Library.get_all_variations()
SYSTEM_TASKS = list(SYSTEM_LIBRARIES.keys())

# This is needed to build the generated_struct_info.json file.
# It is not a system library, but it needs to be built before running with FROZEN_CACHE.
SYSTEM_TASKS += ['struct_info']

# Minimal subset of SYSTEM_TASKS used by CI systems to build enough to useful
MINIMAL_TASKS = [
    'libcompiler_rt',
    'libc',
    'libc++abi',
    'libc++abi-except',
    'libc++abi-noexcept',
    'libc++',
    'libc++-except',
    'libc++-noexcept',
    'libal',
    'libdlmalloc',
    'libdlmalloc-debug',
    'libemmalloc',
    'libemmalloc-64bit',
    'libpthread_stub',
    'libc_rt_wasm',
    'struct_info',
    'libc-wasm',
    'libstandalonewasm',
    'crt1',
    'libunwind-except'
]

USER_TASKS = [
    'binaryen',
    'boost_headers',
    'bullet',
    'bzip2',
    'cocos2d',
    'freetype',
    'harfbuzz',
    'icu',
    'libjpeg',
    'libpng',
    'ogg',
    'regal',
    'regal-mt',
    'sdl2',
    'sdl2-mt',
    'sdl2-gfx',
    'sdl2-image',
    'sdl2-image-png',
    'sdl2-image-jpg',
    'sdl2-mixer',
    'sdl2-net',
    'sdl2-ttf',
    'vorbis',
    'zlib',
]

temp_files = shared.configuration.get_temp_files()
logger = logging.getLogger('embuilder')
force = False


def get_help():
  all_tasks = SYSTEM_TASKS + USER_TASKS
  all_tasks.sort()
  return '''
Available targets:

  build %s

Issuing 'embuilder.py build ALL' causes each task to be built.

It is also possible to build native_optimizer manually by using CMake. To
do that, run

   1. cd $EMSCRIPTEN/tools/optimizer
   2. cmake . -DCMAKE_BUILD_TYPE=Release
   3. make (or mingw32-make/vcbuild/msbuild on Windows)

and set up the location to the native optimizer in ~/.emscripten
''' % '\n        '.join(all_tasks)


def build(src, result_libs, args=[]):
  if shared.Settings.LTO:
    args += ['-flto=' + shared.Settings.LTO]
  if shared.Settings.RELOCATABLE:
    args += ['-s', 'RELOCATABLE']
  # build in order to generate the libraries
  # do it all in a temp dir where everything will be cleaned up
  temp_dir = temp_files.get_dir()
  cpp = os.path.join(temp_dir, 'src.cpp')
  open(cpp, 'w').write(src)
  temp_js = os.path.join(temp_dir, 'out.js')
  if force:
    for lib in result_libs:
      shared.Cache.erase_file(lib)

  try:
    shared.Building.emcc(cpp, args, output_filename=temp_js)
  except subprocess.CalledProcessError as e:
    shared.exit_with_error("embuilder: emcc command failed with %d: '%s'", e.returncode, ' '.join(e.cmd))

  for lib in result_libs:
    if not os.path.exists(shared.Cache.get_path(lib)):
      shared.exit_with_error('not seeing that requested library %s has been built because file %s does not exist' % (lib, shared.Cache.get_path(lib)))


def build_port(port_name, lib_name, params, extra_source=''):
  build(extra_source + '\n' + C_BARE, [lib_name] if lib_name else [], params)


def main():
  global force
  parser = argparse.ArgumentParser(description=__doc__,
                                   formatter_class=argparse.RawDescriptionHelpFormatter,
                                   epilog=get_help())
  parser.add_argument('--lto', action='store_true', help='build bitcode object for LTO')
  parser.add_argument('--pic', action='store_true',
                      help='build relocatable objects for suitable for dynamic linking')
  parser.add_argument('--force', action='store_true',
                      help='force rebuild of target (by removing it first)')
  parser.add_argument('operation', help='currently only "build" is supported')
  parser.add_argument('targets', nargs='+', help='see below')
  args = parser.parse_args()

  if args.operation != 'build':
    shared.exit_with_error('unfamiliar operation: ' + args.operation)

  # process flags

  # Check sanity so that if settings file has changed, the cache is cleared here.
  # Otherwise, the cache will clear in an emcc process, which is invoked while building
  # a system library into the cache, causing trouble.
  shared.check_sanity()

  if args.lto:
    shared.Settings.LTO = "full"
    # Reconfigure the cache dir to reflect the change
    shared.reconfigure_cache()

  if args.pic:
    shared.Settings.RELOCATABLE = 1
    # Reconfigure the cache dir to reflect the change
    shared.reconfigure_cache()

  if args.force:
    force = True

  # process tasks
  libname = system_libs.Ports.get_lib_name

  auto_tasks = False
  tasks = args.targets
  if 'SYSTEM' in tasks:
    tasks = SYSTEM_TASKS
    auto_tasks = True
  elif 'USER' in tasks:
    tasks = USER_TASKS
    auto_tasks = True
  elif 'MINIMAL' in tasks:
    tasks = MINIMAL_TASKS
    auto_tasks = True
  elif 'ALL' in tasks:
    tasks = SYSTEM_TASKS + USER_TASKS
    auto_tasks = True
  if auto_tasks:
    if shared.Settings.WASM_BACKEND:
      skip_tasks = []
      if shared.Settings.RELOCATABLE:
        # we don't support PIC + pthreads yet
        skip_tasks += [task for task in SYSTEM_TASKS + USER_TASKS if '-mt' in task or 'thread' in task]
      # cocos2d: must be ported, errors on
      # "Cannot recognize the target platform; are you targeting an unsupported platform?"
      skip_tasks += ['cocos2d']
      print('Skipping building of %s, because WebAssembly does not support pthreads.' % ', '.join(skip_tasks))
      tasks = [x for x in tasks if x not in skip_tasks]
    else:
      if os.environ.get('EMSCRIPTEN_NATIVE_OPTIMIZER'):
        print('Skipping building of native-optimizer; EMSCRIPTEN_NATIVE_OPTIMIZER is environment.')
      elif shared.EMSCRIPTEN_NATIVE_OPTIMIZER:
        print('Skipping building of native-optimizer; EMSCRIPTEN_NATIVE_OPTIMIZER set in .emscripten config.')
      else:
        tasks += ['native_optimizer']
    print('Building targets: %s' % ' '.join(tasks))
  for what in tasks:
    logger.info('building and verifying ' + what)
    if what in SYSTEM_LIBRARIES:
      library = SYSTEM_LIBRARIES[what]
      if force:
        library.erase()
      library.get_path()
    elif what == 'struct_info':
      build(C_BARE, ['generated_struct_info.json'])
    elif what == 'native_optimizer':
      build(C_BARE, ['optimizer.2.exe'], ['-O2', '-s', 'WASM=0'])
    elif what == 'icu':
      build_port('icu', libname('libicuuc'), ['-s', 'USE_ICU=1'], '#include "unicode/ustring.h"')
    elif what == 'zlib':
      build_port('zlib', 'libz.a', ['-s', 'USE_ZLIB=1'])
    elif what == 'bzip2':
      build_port('bzip2', 'libbz2.a', ['-s', 'USE_BZIP2=1'])
    elif what == 'bullet':
      build_port('bullet', libname('libbullet'), ['-s', 'USE_BULLET=1'])
    elif what == 'vorbis':
      build_port('vorbis', libname('libvorbis'), ['-s', 'USE_VORBIS=1'])
    elif what == 'ogg':
      build_port('ogg', libname('libogg'), ['-s', 'USE_OGG=1'])
    elif what == 'libjpeg':
      build_port('libjpeg', libname('libjpeg'), ['-s', 'USE_LIBJPEG=1'])
    elif what == 'libpng':
      build_port('libpng', libname('libpng'), ['-s', 'USE_ZLIB=1', '-s', 'USE_LIBPNG=1'])
    elif what == 'sdl2':
      build_port('sdl2', libname('libSDL2'), ['-s', 'USE_SDL=2'])
    elif what == 'sdl2-mt':
      build_port('sdl2', libname('libSDL2-mt'), ['-s', 'USE_SDL=2', '-s', 'USE_PTHREADS=1'])
    elif what == 'sdl2-gfx':
      build_port('sdl2-gfx', libname('libSDL2_gfx'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_IMAGE=2', '-s', 'USE_SDL_GFX=2'])
    elif what == 'sdl2-image':
      build_port('sdl2-image', libname('libSDL2_image'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_IMAGE=2'])
    elif what == 'sdl2-image-png':
      build_port('sdl2-image', libname('libSDL2_image_png'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_IMAGE=2', '-s', 'SDL2_IMAGE_FORMATS=["png"]'])
    elif what == 'sdl2-image-jpg':
      build_port('sdl2-image', libname('libSDL2_image_jpg'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_IMAGE=2', '-s', 'SDL2_IMAGE_FORMATS=["jpg"]'])
    elif what == 'sdl2-net':
      build_port('sdl2-net', libname('libSDL2_net'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_NET=2'])
    elif what == 'sdl2-mixer':
      build_port('sdl2-mixer', libname('libSDL2_mixer'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_MIXER=2', '-s', 'USE_VORBIS=1'])
    elif what == 'freetype':
      build_port('freetype', 'libfreetype.a', ['-s', 'USE_FREETYPE=1'])
    elif what == 'harfbuzz':
      build_port('harfbuzz', 'libharfbuzz.a', ['-s', 'USE_HARFBUZZ=1'])
    elif what == 'harfbuzz-mt':
      build_port('harfbuzz-mt', 'libharfbuzz-mt.a', ['-s', 'USE_HARFBUZZ=1', '-s', 'USE_PTHREADS=1'])
    elif what == 'sdl2-ttf':
      build_port('sdl2-ttf', libname('libSDL2_ttf'), ['-s', 'USE_SDL=2', '-s', 'USE_SDL_TTF=2', '-s', 'USE_FREETYPE=1'])
    elif what == 'binaryen':
      build_port('binaryen', None, ['-s', 'WASM=1'])
    elif what == 'cocos2d':
      build_port('cocos2d', libname('libcocos2d'), ['-s', 'USE_COCOS2D=3', '-s', 'USE_ZLIB=1', '-s', 'USE_LIBPNG=1', '-s', 'ERROR_ON_UNDEFINED_SYMBOLS=0'])
    elif what == 'regal':
      build_port('regal', libname('libregal'), ['-s', 'USE_REGAL=1'])
    elif what == 'regal-mt':
      build_port('regal', libname('libregal'), ['-s', 'USE_REGAL=1', '-s', 'USE_PTHREADS=1', '-pthread'])
    elif what == 'boost_headers':
      build_port('boost_headers', libname('libboost_headers'), ['-s', 'USE_BOOST_HEADERS=1'])
    elif what == 'libsockets':
      build('''
        #include <sys/socket.h>
        int main() {
          return socket(0,0,0);
        }
      ''', [libname('libsockets')])
    elif what == 'libsockets_proxy':
      build('''
        #include <sys/socket.h>
        int main() {
          return socket(0,0,0);
        }
      ''', [libname('libsockets_proxy')], ['-s', 'PROXY_POSIX_SOCKETS=1', '-s', 'USE_PTHREADS=1', '-s', 'PROXY_TO_PTHREAD=1'])
    else:
      logger.error('unfamiliar build target: ' + what)
      return 1

    logger.info('...success')
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main())
  except KeyboardInterrupt:
    logger.warning("KeyboardInterrupt")
    sys.exit(1)
