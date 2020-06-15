#!/usr/bin/env python3
# Copyright 2011 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

"""emcc - compiler helper script
=============================

emcc is a drop-in replacement for a compiler like gcc or clang.

See  emcc --help  for details.

emcc can be influenced by a few environment variables:

  EMCC_DEBUG - "1" will log out useful information during compilation, as well as
               save each compiler step as an emcc-* file in the temp dir
               (by default /tmp/emscripten_temp). "2" will save additional emcc-*
               steps, that would normally not be separately produced (so this
               slows down compilation).

  EMMAKEN_NO_SDK - Will tell emcc *not* to use the emscripten headers. Instead
                   your system headers will be used.

  EMMAKEN_COMPILER - The compiler to be used, if you don't want the default clang.
"""

from __future__ import print_function

import json
import logging
import os
import re
import shlex
import shutil
import stat
import sys
import time
import base64
from subprocess import PIPE

import emscripten
from tools import shared, system_libs, client_mods, js_optimizer, jsrun, colored_logger, diagnostics
from tools.shared import unsuffixed, unsuffixed_basename, WINDOWS, safe_move, run_process, asbytes, read_and_preprocess, exit_with_error, DEBUG
from tools.response_file import substitute_response_files
from tools.minimal_runtime_shell import generate_minimal_runtime_html
import tools.line_endings
from tools.toolchain_profiler import ToolchainProfiler
if __name__ == '__main__':
  ToolchainProfiler.record_process_start()

try:
  from urllib.parse import quote
except ImportError:
  # Python 2 compatibility
  from urllib import quote

logger = logging.getLogger('emcc')

DEV_NULL = '/dev/null' if not WINDOWS else 'NUL'

# endings = dot + a suffix, safe to test by  filename.endswith(endings)
C_ENDINGS = ('.c', '.i')
CXX_ENDINGS = ('.cpp', '.cxx', '.cc', '.c++', '.CPP', '.CXX', '.C', '.CC', '.C++', '.ii')
OBJC_ENDINGS = ('.m', '.mi')
OBJCXX_ENDINGS = ('.mm', '.mii')
ASSEMBLY_CPP_ENDINGS = ('.S',)
SPECIAL_ENDINGLESS_FILENAMES = (DEV_NULL,)

SOURCE_ENDINGS = C_ENDINGS + CXX_ENDINGS + OBJC_ENDINGS + OBJCXX_ENDINGS + SPECIAL_ENDINGLESS_FILENAMES + ASSEMBLY_CPP_ENDINGS
C_ENDINGS = C_ENDINGS + SPECIAL_ENDINGLESS_FILENAMES # consider the special endingless filenames like /dev/null to be C

JS_CONTAINING_ENDINGS = ('.js', '.mjs', '.html')
OBJECT_FILE_ENDINGS = ('.bc', '.o', '.obj', '.lo')
DYNAMICLIB_ENDINGS = ('.dylib', '.so') # Windows .dll suffix is not included in this list, since those are never linked to directly on the command line.
STATICLIB_ENDINGS = ('.a',)
ASSEMBLY_ENDINGS = ('.ll', '.s')
HEADER_ENDINGS = ('.h', '.hxx', '.hpp', '.hh', '.H', '.HXX', '.HPP', '.HH')
WASM_ENDINGS = ('.wasm',)

# Supported LLD flags which we will pass through to the linker.
SUPPORTED_LINKER_FLAGS = (
    '--start-group', '--end-group',
    '-(', '-)',
    '--whole-archive', '--no-whole-archive',
    '-whole-archive', '-no-whole-archive'
)

# Unsupported LLD flags which we will ignore.
# Maps to true if the flag takes an argument.
UNSUPPORTED_LLD_FLAGS = {
    # macOS-specific linker flag that libtool (ltmain.sh) will if macOS is detected.
    '-bind_at_load': False,
    # wasm-ld doesn't support map files yet.
    '--print-map': False,
    '-M': False,
    # wasm-ld doesn't support soname or other dynamic linking flags (yet).   Ignore them
    # in order to aid build systems that want to pass these flags.
    '-soname': True,
    '--allow-shlib-undefined': False,
    '-rpath': True,
    '-rpath-link': True
}

LIB_PREFIXES = ('', 'lib')

DEFERRED_RESPONSE_FILES = ('EMTERPRETIFY_BLACKLIST', 'EMTERPRETIFY_WHITELIST', 'EMTERPRETIFY_SYNCLIST')

DEFAULT_ASYNCIFY_IMPORTS = [
  'emscripten_sleep', 'emscripten_wget', 'emscripten_wget_data', 'emscripten_idb_load',
  'emscripten_idb_store', 'emscripten_idb_delete', 'emscripten_idb_exists',
  'emscripten_idb_load_blob', 'emscripten_idb_store_blob', 'SDL_Delay',
  'emscripten_scan_registers', 'emscripten_lazy_load_code',
  'emscripten_fiber_swap',
  'wasi_snapshot_preview1.fd_sync', '__wasi_fd_sync']

# Mapping of emcc opt levels to llvm opt levels. We use llvm opt level 3 in emcc
# opt levels 2 and 3 (emcc 3 is unsafe opts, so unsuitable for the only level to
# get llvm opt level 3, and speed-wise emcc level 2 is already the slowest/most
# optimizing level)
LLVM_OPT_LEVEL = {
  0: ['-O0'],
  1: ['-O1'],
  2: ['-O3'],
  3: ['-O3'],
}

# Target options
final = None

UBSAN_SANITIZERS = {
  'alignment',
  'bool',
  'builtin',
  'bounds',
  'enum',
  'float-cast-overflow',
  'float-divide-by-zero',
  'function',
  'implicit-unsigned-integer-truncation',
  'implicit-signed-integer-truncation',
  'implicit-integer-sign-change',
  'integer-divide-by-zero',
  'nonnull-attribute',
  'null',
  'nullability-arg',
  'nullability-assign',
  'nullability-return',
  'object-size',
  'pointer-overflow',
  'return',
  'returns-nonnull-attribute',
  'shift',
  'signed-integer-overflow',
  'unreachable',
  'unsigned-integer-overflow',
  'vla-bound',
  'vptr',
  'undefined',
  'undefined-trap',
  'implicit-integer-truncation',
  'implicit-integer-arithmetic-value-change',
  'implicit-conversion',
  'integer',
  'nullability',
}


# this function uses the global 'final' variable, which contains the current
# final output file. if a method alters final, and calls this method, then it
# must modify final globally (i.e. it can't receive final as a param and
# return it)
# TODO: refactor all this, a singleton that abstracts over the final output
#       and saving of intermediates
def save_intermediate(name, suffix='js'):
  if not DEBUG:
    return
  if isinstance(final, list):
    logger.debug('(not saving intermediate %s because deferring linking)' % name)
    return
  shared.Building.save_intermediate(final, name + '.' + suffix)


def save_intermediate_with_wasm(name, wasm_binary):
  if not DEBUG:
    return
  save_intermediate(name) # save the js
  shared.Building.save_intermediate(wasm_binary, name + '.wasm')


class TimeLogger(object):
  last = time.time()

  @staticmethod
  def update():
    TimeLogger.last = time.time()


def log_time(name):
  """Log out times for emcc stages"""
  if DEBUG:
    now = time.time()
    logger.debug('emcc step "%s" took %.2f seconds', name, now - TimeLogger.last)
    TimeLogger.update()


def base64_encode(b):
  b64 = base64.b64encode(b)
  if type(b64) == bytes:
    return b64.decode('ascii')
  else:
    return b64


class EmccOptions(object):
  def __init__(self):
    self.requested_debug = ''
    self.profiling = False
    self.profiling_funcs = False
    self.tracing = False
    self.emit_symbol_map = False
    self.js_opts = None
    self.force_js_opts = False
    self.llvm_opts = None
    self.llvm_lto = None
    self.use_closure_compiler = None
    self.closure_args = []
    self.js_transform = None
    self.pre_js = '' # before all js
    self.post_js = '' # after all js
    self.extern_pre_js = '' # before all js, external to optimized code
    self.extern_post_js = '' # after all js, external to optimized code
    self.preload_files = []
    self.embed_files = []
    self.exclude_files = []
    self.ignore_dynamic_linking = False
    self.shell_path = shared.path_from_root('src', 'shell.html')
    self.source_map_base = ''
    self.emrun = False
    self.cpu_profiler = False
    self.thread_profiler = False
    self.memory_profiler = False
    self.save_bc = False
    self.memory_init_file = None
    self.use_preload_cache = False
    self.no_heap_copy = False
    self.use_preload_plugins = False
    self.proxy_to_worker = False
    self.default_object_extension = '.o'
    self.valid_abspaths = []
    self.separate_asm = False
    self.cfi = False
    # Specifies the line ending format to use for all generated text files.
    # Defaults to using the native EOL on each platform (\r\n on Windows, \n on
    # Linux & MacOS)
    self.output_eol = os.linesep
    self.binaryen_passes = []
    # Whether we will expand the full path of any input files to remove any
    # symlinks.
    self.expand_symlinks = True
    self.no_entry = False


def use_source_map(options):
  return shared.Settings.DEBUG_LEVEL >= 4


def will_metadce(options):
  return shared.Settings.OPT_LEVEL >= 3 or shared.Settings.SHRINK_LEVEL >= 1


class JSOptimizer(object):
  def __init__(self, target, options, js_transform_tempfiles, in_temp):
    self.queue = []
    self.extra_info = {}
    self.queue_history = []
    self.blacklist = os.environ.get('EMCC_JSOPT_BLACKLIST', '').split(',')
    self.minify_whitespace = False
    self.cleanup_shell = False

    self.target = target
    self.emit_symbol_map = options.emit_symbol_map
    self.profiling_funcs = options.profiling_funcs
    self.use_closure_compiler = options.use_closure_compiler
    self.closure_args = options.closure_args

    self.js_transform_tempfiles = js_transform_tempfiles
    self.in_temp = in_temp

  def flush(self, title='js_opts'):
    self.queue = [p for p in self.queue if p not in self.blacklist]

    assert not shared.Settings.WASM_BACKEND, 'JSOptimizer should not run with pure wasm output'

    if self.extra_info is not None and len(self.extra_info) == 0:
      self.extra_info = None

    if len(self.queue) and not(not shared.Settings.ASM_JS and len(self.queue) == 1 and self.queue[0] == 'last'):
      passes = self.queue[:]

      if DEBUG != 2 or len(passes) < 2:
        # by assumption, our input is JS, and our output is JS. If a pass is going to run in the native optimizer in C++, then we
        # must give it JSON and receive from it JSON
        chunks = []
        curr = []
        for p in passes:
          if len(curr) == 0:
            curr.append(p)
          else:
            native = js_optimizer.use_native(p, source_map=use_source_map(self))
            last_native = js_optimizer.use_native(curr[-1], source_map=use_source_map(self))
            if native == last_native:
              curr.append(p)
            else:
              curr.append('emitJSON')
              chunks.append(curr)
              curr = ['receiveJSON', p]
        if len(curr):
          chunks.append(curr)
        if len(chunks) == 1:
          self.run_passes(chunks[0], title, just_split=False, just_concat=False)
        else:
          for i, chunk in enumerate(chunks):
            self.run_passes(chunk, 'js_opts_' + str(i),
                            just_split='receiveJSON' in chunk,
                            just_concat='emitJSON' in chunk)
      else:
        # DEBUG 2, run each pass separately
        extra_info = self.extra_info
        for p in passes:
          self.queue = [p]
          self.flush(p)
          self.extra_info = extra_info # flush wipes it
          log_time('part of js opts')
      self.queue_history += self.queue
      self.queue = []
    self.extra_info = {}

  def run_passes(self, passes, title, just_split, just_concat):
    global final
    passes = ['asm'] + passes
    if shared.Settings.PRECISE_F32:
      passes = ['asmPreciseF32'] + passes
    if (self.emit_symbol_map or shared.Settings.CYBERDWARF) and 'minifyNames' in passes:
      passes += ['symbolMap=' + shared.replace_or_append_suffix(self.target, '.symbols')]
    if self.profiling_funcs and 'minifyNames' in passes:
      passes += ['profilingFuncs']
    if self.minify_whitespace and 'last' in passes:
      passes += ['minifyWhitespace']
    if self.cleanup_shell and 'last' in passes:
      passes += ['cleanup']
    logger.debug('applying js optimization passes: %s', ' '.join(passes))
    final = shared.Building.js_optimizer(final, passes, use_source_map(self),
                                         self.extra_info, just_split=just_split,
                                         just_concat=just_concat,
                                         output_filename=self.in_temp(os.path.basename(final) + '.jsopted.js'),
                                         extra_closure_args=self.closure_args)
    self.js_transform_tempfiles.append(final)
    save_intermediate(title, suffix='js' if 'emitJSON' not in passes else 'json')

  def do_minify(self):
    """minifies the code.

    this is also when we do certain optimizations that must be done right before or after minification
    """
    if shared.Settings.OPT_LEVEL >= 2:
      if shared.Settings.DEBUG_LEVEL < 2 and not self.use_closure_compiler == 2:
        self.queue += ['minifyNames']
      if shared.Settings.DEBUG_LEVEL == 0:
        self.minify_whitespace = True

    if self.use_closure_compiler == 1:
      self.queue += ['closure']
    elif shared.Settings.DEBUG_LEVEL <= 2 and shared.Settings.FINALIZE_ASM_JS and not self.use_closure_compiler:
      self.cleanup_shell = True


def embed_memfile(options):
  return (shared.Settings.SINGLE_FILE or
          (shared.Settings.MEM_INIT_METHOD == 0 and
           (not shared.Settings.MAIN_MODULE and
            not shared.Settings.SIDE_MODULE and
            not use_source_map(options))))


def apply_settings(changes):
  """Take a list of settings in form `NAME=VALUE` and apply them to the global
  Settings object.
  """

  def standardize_setting_change(key, value):
    # boolean NO_X settings are aliases for X
    # (note that *non*-boolean setting values have special meanings,
    # and we can't just flip them, so leave them as-is to be
    # handled in a special way later)
    if key.startswith('NO_') and value in ('0', '1'):
      key = key[3:]
      value = str(1 - int(value))
    return key, value

  for change in changes:
    key, value = change.split('=', 1)
    key, value = standardize_setting_change(key, value)

    if key in shared.Settings.internal_settings:
      exit_with_error('%s is an internal setting and cannot be set from command line', key)

    # map legacy settings which have aliases to the new names
    # but keep the original key so errors are correctly reported via the `setattr` below
    user_key = key
    if key in shared.Settings.legacy_settings and key in shared.Settings.alt_names:
      key = shared.Settings.alt_names[key]

    # In those settings fields that represent amount of memory, translate suffixes to multiples of 1024.
    if key in ('TOTAL_STACK', 'INITIAL_MEMORY', 'MEMORY_GROWTH_LINEAR_STEP', 'MEMORY_GROWTH_GEOMETRIC_STEP',
               'GL_MAX_TEMP_BUFFER_SIZE', 'MAXIMUM_MEMORY', 'DEFAULT_PTHREAD_STACK_SIZE'):
      value = str(shared.expand_byte_size_suffixes(value))

    if value[0] == '@':
      if key not in DEFERRED_RESPONSE_FILES:
        filename = value[1:]
        if not os.path.exists(filename):
          exit_with_error('%s: file not found parsing argument: %s' % (filename, change))
        value = open(filename).read()
    else:
      value = value.replace('\\', '\\\\')
    try:
      value = parse_value(value)
    except Exception as e:
      exit_with_error('a problem occurred in evaluating the content after a "-s", specifically "%s": %s', change, str(e))

    # Do some basic type checking by comparing to the existing settings.
    # Sadly we can't do this generically in the SettingsManager since there are settings
    # that so change types internally over time.
    existing = getattr(shared.Settings, user_key, None)
    if existing is not None:
      # We only currently worry about lists vs non-lists.
      if (type(existing) == list) != (type(value) == list):
        exit_with_error('setting `%s` expects `%s` but got `%s`' % (user_key, type(existing), type(value)))
    setattr(shared.Settings, user_key, value)

    if shared.Settings.WASM_BACKEND and key == 'BINARYEN_TRAP_MODE':
      exit_with_error('BINARYEN_TRAP_MODE is not supported by the LLVM wasm backend')

    if key == 'EXPORTED_FUNCTIONS':
      # used for warnings in emscripten.py
      shared.Settings.USER_EXPORTED_FUNCTIONS = shared.Settings.EXPORTED_FUNCTIONS[:]

    # TODO(sbc): Remove this legacy way.
    if key == 'WASM_OBJECT_FILES':
      shared.Settings.LTO = 0 if value else 'full'


def find_output_arg(args):
  """Find and remove any -o arguments.  The final one takes precedence.
  Return the final -o target along with the remaining (non-o) arguments.
  """
  outargs = []
  specified_target = None
  use_next = False
  for arg in args:
    if use_next:
      specified_target = arg
      use_next = False
      continue
    if arg == '-o':
      use_next = True
    elif arg.startswith('-o'):
      specified_target = arg[2:]
    else:
      outargs.append(arg)
  return specified_target, outargs


def do_emscripten(infile, memfile):
  # Run Emscripten
  outfile = infile + '.o.js'
  with ToolchainProfiler.profile_block('emscripten.py'):
    emscripten.run(infile, outfile, memfile)

  # Detect compilation crashes and errors
  assert os.path.exists(outfile), 'Emscripten failed to generate .js'

  return outfile


def is_ar_file_with_missing_index(archive_file):
  # We parse the archive header outselves because llvm-nm --print-armap is slower and less
  # reliable.
  # See: https://github.com/emscripten-core/emscripten/issues/10195
  archive_header = b'!<arch>\n'
  file_header_size = 60

  with open(archive_file, 'rb') as f:
    header = f.read(len(archive_header))
    if header != archive_header:
      # This is not even an ar file
      return False
    file_header = f.read(file_header_size)
    if len(file_header) != file_header_size:
      # We don't have any file entires at all so we don't consider the index missing
      return False

  name = file_header[:16].strip()
  # If '/' is the name of the first file we have an index
  return name != b'/'


def ensure_archive_index(archive_file):
  # Fastcomp linking works without archive indexes.
  if not shared.Settings.WASM_BACKEND or not shared.Settings.AUTO_ARCHIVE_INDEXES:
    return
  if is_ar_file_with_missing_index(archive_file):
    diagnostics.warning('emcc', '%s: archive is missing an index; Use emar when creating libraries to ensure an index is created', archive_file)
    diagnostics.warning('emcc', '%s: adding index', archive_file)
    run_process([shared.LLVM_RANLIB, archive_file])


def get_all_js_syms(temp_files):
  # Runs the js compiler to generate a list of all symbols available in the JS
  # libraries.  This must be done separately for each linker invokation since the
  # list of symbols depends on what settings are used.
  # TODO(sbc): Find a way to optimize this.  Potentially we could add a super-set
  # mode of the js compiler that would generate a list of all possible symbols
  # that could be checked in.
  old_full = shared.Settings.INCLUDE_FULL_LIBRARY
  try:
    # Temporarily define INCLUDE_FULL_LIBRARY since we want a full list
    # of all available JS library functions.
    shared.Settings.INCLUDE_FULL_LIBRARY = True
    shared.Settings.ONLY_CALC_JS_SYMBOLS = True
    emscripten.generate_struct_info()
    glue, forwarded_data = emscripten.compile_settings(temp_files)
    forwarded_json = json.loads(forwarded_data)
    library_fns = forwarded_json['Functions']['libraryFunctions']
    library_fns_list = []
    for name in library_fns:
      if shared.is_c_symbol(name):
        name = shared.demangle_c_symbol_name(name)
        library_fns_list.append(name)
  finally:
    shared.Settings.ONLY_CALC_JS_SYMBOLS = False
    shared.Settings.INCLUDE_FULL_LIBRARY = old_full

  return library_fns_list


def filter_link_flags(flags, using_lld):
  def is_supported(f):
    if using_lld:
      for flag, takes_arg in UNSUPPORTED_LLD_FLAGS.items():
        if f.startswith(flag):
          diagnostics.warning('linkflags', 'ignoring unsupported linker flag: `%s`', f)
          return False, takes_arg
      return True, False
    else:
      if f in SUPPORTED_LINKER_FLAGS:
        return True, False
      # Silently ignore -l/-L flags when not using lld.  If using lld allow
      # them to pass through the linker
      if f.startswith('-l') or f.startswith('-L'):
        return False, False
      diagnostics.warning('linkflags', 'ignoring unsupported linker flag: `%s`', f)
      return False, False

  results = []
  skip_next = False
  for f in flags:
    if skip_next:
      skip_next = False
      continue
    keep, skip_next = is_supported(f[1])
    if keep:
      results.append(f)

  return results


def fix_windows_newlines(text):
  # Avoid duplicating \r\n to \r\r\n when writing out text.
  if WINDOWS:
    text = text.replace('\r\n', '\n')
  return text


def cxx_to_c_compiler(cxx):
  # Convert C++ compiler name into C compiler name
  dirname, basename = os.path.split(cxx)
  basename = basename.replace('clang++', 'clang').replace('g++', 'gcc').replace('em++', 'emcc')
  return os.path.join(dirname, basename)


run_via_emxx = False


#
# Main run() function
#
def run(args):
  global final
  target = None

  # Additional compiler flags that we treat as if they were passed to us on the
  # commandline
  EMCC_CFLAGS = os.environ.get('EMCC_CFLAGS')
  if DEBUG:
    cmd = ' '.join(args)
    if EMCC_CFLAGS:
      cmd += ' + ' + EMCC_CFLAGS
    logger.warning('invocation: ' + cmd + '  (in ' + os.getcwd() + ')')
  if EMCC_CFLAGS:
    args.extend(shlex.split(EMCC_CFLAGS))

  # Strip args[0] (program name)
  args = args[1:]

  misc_temp_files = shared.configuration.get_temp_files()

  # Handle some global flags

  # read response files very early on
  try:
    args = substitute_response_files(args)
  except IOError as e:
    exit_with_error(e)

  if '--help' in args:
    # Documentation for emcc and its options must be updated in:
    #    site/source/docs/tools_reference/emcc.rst
    # This then gets built (via: `make -C site text`) to:
    #    site/build/text/docs/tools_reference/emcc.txt
    # This then needs to be copied to its final home in docs/emcc.txt from where
    # we read it here.  We have CI rules that ensure its always up-to-date.
    with open(shared.path_from_root('docs', 'emcc.txt'), 'r') as f:
      print(f.read())

    print('''
------------------------------------------------------------------

emcc: supported targets: llvm bitcode, javascript, NOT elf
(autoconf likes to see elf above to enable shared object support)
''')
    return 0

  if '--version' in args:
    # if the emscripten folder is not a git repo, don't run git show - that can
    # look up and find the revision in a parent directory that is a git repo
    revision = ''
    if os.path.exists(shared.path_from_root('.git')):
      revision = run_process(['git', 'rev-parse', 'HEAD'], stdout=PIPE, stderr=PIPE, cwd=shared.path_from_root()).stdout.strip()
    elif os.path.exists(shared.path_from_root('emscripten-revision.txt')):
      revision = open(shared.path_from_root('emscripten-revision.txt')).read().strip()
    if revision:
      revision = ' (%s)' % revision
    print('''emcc (Emscripten gcc/clang-like replacement) %s%s
Copyright (C) 2014 the Emscripten authors (see AUTHORS.txt)
This is free and open source software under the MIT license.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  ''' % (shared.EMSCRIPTEN_VERSION, revision))
    return 0

  if len(args) == 1 and args[0] == '-v': # -v with no inputs
    # autoconf likes to see 'GNU' in the output to enable shared object support
    print('emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) %s' % shared.EMSCRIPTEN_VERSION, file=sys.stderr)
    code = run_process([shared.CLANG_CC, '-v'], check=False).returncode
    shared.check_sanity(force=True)
    return code

  shared.check_sanity(force=DEBUG)

  # This check comes after check_sanity because test_sanity expects this.
  if not args:
    logger.warning('no input files')
    return 1

  if '-dumpmachine' in args:
    print(shared.get_llvm_target())
    return 0

  if '-dumpversion' in args: # gcc's doc states "Print the compiler version [...] and don't do anything else."
    print(shared.EMSCRIPTEN_VERSION)
    return 0

  if '--cflags' in args:
    # fake running the command, to see the full args we pass to clang
    debug_env = os.environ.copy()
    args = [x for x in args if x != '--cflags']
    with misc_temp_files.get_file(suffix='.o') as temp_target:
      input_file = 'hello_world.c'
      cmd = [shared.PYTHON, sys.argv[0], shared.path_from_root('tests', input_file), '-v', '-c', '-o', temp_target] + args
      proc = run_process(cmd, stderr=PIPE, env=debug_env, check=False)
      if proc.returncode != 0:
        print(proc.stderr)
        exit_with_error('error getting cflags')
      lines = [x for x in proc.stderr.splitlines() if shared.CLANG_CC in x and input_file in x]
      parts = shlex.split(lines[0].replace('\\', '\\\\'))
      parts = [x for x in parts if x not in ['-c', '-o', '-v', '-emit-llvm'] and input_file not in x and temp_target not in x]
      print(' '.join(shared.Building.doublequote_spaces(parts[1:])))
    return 0

  def get_language_mode(args):
    return_next = False
    for item in args:
      if return_next:
        return item
      if item == '-x':
        return_next = True
        continue
      if item.startswith('-x'):
        return item[2:]
    return None

  language_mode = get_language_mode(args)
  has_fixed_language_mode = language_mode is not None

  def is_minus_s_for_emcc(args, i):
    # -s OPT=VALUE or -s OPT are interpreted as emscripten flags.
    # -s by itself is a linker option (alias for --strip-all)
    assert args[i] == '-s'
    if len(args) > i + 1:
      arg = args[i + 1]
      if arg.split('=')[0].isupper():
        return True

    logger.debug('treating -s as linker option and not as -s OPT=VALUE for js compilation')
    return False

  CONFIGURE_CONFIG = os.environ.get('EMMAKEN_JUST_CONFIGURE') or 'conftest.c' in args
  if CONFIGURE_CONFIG and not os.environ.get('EMMAKEN_JUST_CONFIGURE_RECURSE'):
    # XXX use this to debug configure stuff. ./configure's generally hide our
    # normal output including stderr so we write to a file
    debug_configure = 0

    if debug_configure:
      tempout = '/tmp/emscripten_temp/out'
      if not os.path.exists(tempout):
        open(tempout, 'w').write('//\n')

    if debug_configure:
      for arg in args:
        if arg.endswith(SOURCE_ENDINGS):
          try:
            src = open(arg).read()
            open(tempout, 'a').write('============= ' + arg + '\n' + src + '\n=============\n\n')
          except IOError:
            pass

    if run_via_emxx:
      compiler = [shared.PYTHON, shared.EMXX]
    else:
      compiler = [shared.PYTHON, shared.EMCC]

    cmd = compiler + [a for a in args if a != '--tracing']
    # configure tests want a more shell-like style, where we emit return codes on exit()
    cmd += ['-s', 'EXIT_RUNTIME=1']
    # use node.js raw filesystem access, to behave just like a native executable
    cmd += ['-s', 'NODERAWFS=1']

    logger.debug('just configuring: ' + ' '.join(cmd))
    if debug_configure:
      open(tempout, 'a').write('emcc, just configuring: ' + ' '.join(cmd) + '\n\n')

    linking = '-c' not in cmd
    # Last -o directive should take precedence, if multiple are specified
    if linking:
      target = 'a.out.js'
      for i in reversed(range(len(cmd) - 1)):
        if cmd[i] == '-o':
          if linking:
            cmd[i + 1] += '.js'
          target = cmd[i + 1]
          break
    env = os.environ.copy()
    env['EMMAKEN_JUST_CONFIGURE_RECURSE'] = '1'
    ret = run_process(cmd, check=False, env=env).returncode
    if ret == 0 and linking:
      if target.endswith('.js'):
        shutil.copyfile(target, unsuffixed(target))
        target = unsuffixed(target)
      src = open(target).read()
      full_node = ' '.join(shared.NODE_JS)
      if os.path.sep not in full_node:
        full_node = '/usr/bin/' + full_node # TODO: use whereis etc. And how about non-*NIX?
      open(target, 'w').write('#!' + full_node + '\n' + src) # add shebang
      try:
        os.chmod(target, stat.S_IMODE(os.stat(target).st_mode) | stat.S_IXUSR) # make executable
      except OSError:
        pass # can fail if e.g. writing the executable to /dev/null
    return ret

  CXX = os.environ.get('EMMAKEN_COMPILER', shared.CLANG_CXX)
  CC = cxx_to_c_compiler(CXX)

  EMMAKEN_CFLAGS = os.environ.get('EMMAKEN_CFLAGS')
  if EMMAKEN_CFLAGS:
    args += shlex.split(EMMAKEN_CFLAGS)

  # ---------------- Utilities ---------------

  def suffix(name):
    """Return the file extension"""
    return os.path.splitext(name)[1]

  seen_names = {}

  def uniquename(name):
    if name not in seen_names:
      seen_names[name] = str(len(seen_names))
    return unsuffixed(name) + '_' + seen_names[name] + suffix(name)

  # ---------------- End configs -------------

  # Check if a target is specified on the command line
  specified_target, args = find_output_arg(args)

  # specified_target is the user-specified one, target is what we will generate
  if specified_target:
    target = specified_target
    # check for the existence of the output directory now, to avoid having
    # to do so repeatedly when each of the various output files (.mem, .wasm,
    # etc) are written. This gives a more useful error message than the
    # IOError and python backtrace that users would otherwise see.
    dirname = os.path.dirname(target)
    if dirname and not os.path.isdir(dirname):
      exit_with_error("specified output file (%s) is in a directory that does not exist" % target)
  else:
    target = 'a.out.js'

  shared.Settings.TARGET_BASENAME = target_basename = unsuffixed_basename(target)

  final_suffix = suffix(target)

  temp_dir = shared.get_emscripten_temp_dir()

  def in_temp(name):
    return os.path.join(temp_dir, os.path.basename(name))

  def get_file_suffix(filename):
    """Parses the essential suffix of a filename, discarding Unix-style version
    numbers in the name. For example for 'libz.so.1.2.8' returns '.so'"""
    if filename in SPECIAL_ENDINGLESS_FILENAMES:
      return filename
    while filename:
      filename, suffix = os.path.splitext(filename)
      if not suffix[1:].isdigit():
        return suffix
    return ''

  def optimizing(opts):
    return '-O0' not in opts

  def need_llvm_debug_info(options):
    return shared.Settings.DEBUG_LEVEL >= 3 or shared.Settings.CYBERDWARF

  with ToolchainProfiler.profile_block('parse arguments and setup'):
    ## Parse args

    newargs = list(args)

    # Scan and strip emscripten specific cmdline warning flags.
    # This needs to run before other cmdline flags have been parsed, so that
    # warnings are properly printed during arg parse.
    newargs = diagnostics.capture_warnings(newargs)

    for i in range(len(newargs)):
      if newargs[i] in ('-l', '-L', '-I'):
        # Scan for individual -l/-L/-I arguments and concatenate the next arg on
        # if there is no suffix
        newargs[i] += newargs[i + 1]
        newargs[i + 1] = ''

    options, settings_changes, newargs = parse_args(newargs)

    if '-print-search-dirs' in newargs:
      return run_process([CC, '-print-search-dirs'], check=False).returncode

    if options.emrun:
      options.pre_js += open(shared.path_from_root('src', 'emrun_prejs.js')).read() + '\n'
      options.post_js += open(shared.path_from_root('src', 'emrun_postjs.js')).read() + '\n'
      # emrun mode waits on program exit
      shared.Settings.EXIT_RUNTIME = 1

    if options.cpu_profiler:
      options.post_js += open(shared.path_from_root('src', 'cpuprofiler.js')).read() + '\n'

    if options.memory_profiler:
      shared.Settings.MEMORYPROFILER = 1

    if options.thread_profiler:
      options.post_js += open(shared.path_from_root('src', 'threadprofiler.js')).read() + '\n'

    if options.js_opts is None:
      options.js_opts = shared.Settings.OPT_LEVEL >= 2

    if options.llvm_opts is None:
      options.llvm_opts = LLVM_OPT_LEVEL[shared.Settings.OPT_LEVEL]
    elif type(options.llvm_opts) == int:
      options.llvm_opts = ['-O%d' % options.llvm_opts]

    if options.memory_init_file is None:
      options.memory_init_file = shared.Settings.OPT_LEVEL >= 2

    # TODO: support source maps with js_transform
    if options.js_transform and use_source_map(options):
      logger.warning('disabling source maps because a js transform is being done')
      shared.Settings.DEBUG_LEVEL = 3

    if DEBUG:
      start_time = time.time() # done after parsing arguments, which might affect debug state

    for i in range(len(newargs)):
      if newargs[i] == '-s':
        if is_minus_s_for_emcc(newargs, i):
          key = newargs[i + 1]
          # If not = is specified default to 1
          if '=' not in key:
            key += '=1'

          # Special handling of browser version targets. A version -1 means that the specific version
          # is not supported at all. Replace those with INT32_MAX to make it possible to compare e.g.
          # #if MIN_FIREFOX_VERSION < 68
          try:
            if re.match(r'MIN_.*_VERSION(=.*)?', key) and int(key.split('=')[1]) < 0:
              key = key.split('=')[0] + '=0x7FFFFFFF'
          except Exception:
            pass

          settings_changes.append(key)
          newargs[i] = newargs[i + 1] = ''
    newargs = [arg for arg in newargs if arg]

    settings_key_changes = set()
    for s in settings_changes:
      key, value = s.split('=', 1)
      settings_key_changes.add(key)
      if key == 'WASM_BACKEND':
        exit_with_error('do not set -s WASM_BACKEND, this is detected based on the llvm version in use')

    # Find input files

    # These three arrays are used to store arguments of different types for
    # type-specific processing. In order to shuffle the arguments back together
    # after processing, all of these arrays hold tuples (original_index, value).
    # Note that the index part of the tuple can have a fractional part for input
    # arguments that expand into multiple processed arguments, as in -Wl,-f1,-f2.
    input_files = []
    libs = []
    link_flags = []

    # All of the above arg lists entries contain indexes into the full argument
    # list. In order to add extra implicit args (embind.cc, etc) below, we keep a
    # counter for the next index that should be used.
    next_arg_index = len(newargs)

    has_header_inputs = False
    lib_dirs = []

    has_dash_c = '-c' in newargs
    has_dash_S = '-S' in newargs
    has_dash_E = '-E' in newargs
    link_to_object = False
    compile_only = has_dash_c or has_dash_S or has_dash_E

    def add_link_flag(i, f):
      # Filter out libraries that musl includes in libc itself, or which we
      # otherwise provide implicitly.
      if f in ('-lm', '-lrt', '-ldl', '-lpthread'):
        return
      if f.startswith('-l'):
        libs.append((i, f[2:]))
      if f.startswith('-L'):
        lib_dirs.append(f[2:])

      link_flags.append((i, f))

    # find input files with a simple heuristic. we should really analyze
    # based on a full understanding of gcc params, right now we just assume that
    # what is left contains no more |-x OPT| things
    skip = False
    for i in range(len(newargs)):
      if skip:
        skip = False
        continue

      arg = newargs[i]
      if arg in ('-MT', '-MF', '-MQ', '-D', '-U', '-o', '-x',
                 '-Xpreprocessor', '-include', '-imacros', '-idirafter',
                 '-iprefix', '-iwithprefix', '-iwithprefixbefore',
                 '-isysroot', '-imultilib', '-A', '-isystem', '-iquote',
                 '-install_name', '-compatibility_version',
                 '-current_version', '-I', '-L', '-include-pch',
                 '-Xlinker'):
        skip = True

      if options.expand_symlinks and os.path.islink(arg) and get_file_suffix(os.path.realpath(arg)) in SOURCE_ENDINGS + OBJECT_FILE_ENDINGS + DYNAMICLIB_ENDINGS + ASSEMBLY_ENDINGS + HEADER_ENDINGS:
        arg = os.path.realpath(arg)

      if not arg.startswith('-'):
        if not os.path.exists(arg):
          exit_with_error('%s: No such file or directory ("%s" was expected to be an input file, based on the commandline arguments provided)', arg, arg)

        file_suffix = get_file_suffix(arg)
        if file_suffix in SOURCE_ENDINGS + OBJECT_FILE_ENDINGS + DYNAMICLIB_ENDINGS + ASSEMBLY_ENDINGS + HEADER_ENDINGS or shared.Building.is_ar(arg): # we already removed -o <target>, so all these should be inputs
          newargs[i] = ''
          if file_suffix in SOURCE_ENDINGS or (has_dash_c and file_suffix == '.bc'):
            input_files.append((i, arg))
          elif file_suffix in HEADER_ENDINGS:
            input_files.append((i, arg))
            has_header_inputs = True
          elif file_suffix in ASSEMBLY_ENDINGS or shared.Building.is_bitcode(arg) or shared.Building.is_ar(arg):
            input_files.append((i, arg))
          elif shared.Building.is_wasm(arg):
            if not shared.Settings.WASM_BACKEND:
              exit_with_error('fastcomp is not compatible with wasm object files:' + arg)
            input_files.append((i, arg))
          elif file_suffix in (STATICLIB_ENDINGS + DYNAMICLIB_ENDINGS):
            # if it's not, and it's a library, just add it to libs to find later
            libname = unsuffixed_basename(arg)
            for prefix in LIB_PREFIXES:
              if not prefix:
                continue
              if libname.startswith(prefix):
                libname = libname[len(prefix):]
                break
            libs.append((i, libname))
            newargs[i] = ''
          else:
            diagnostics.warning('invalid-input', arg + ' is not a valid input file')
        elif file_suffix in STATICLIB_ENDINGS:
          if not shared.Building.is_ar(arg):
            if shared.Building.is_bitcode(arg):
              message = arg + ': File has a suffix of a static library ' + str(STATICLIB_ENDINGS) + ', but instead is an LLVM bitcode file! When linking LLVM bitcode files, use one of the suffixes ' + str(OBJECT_FILE_ENDINGS)
            else:
              message = arg + ': Unknown format, not a static library!'
            exit_with_error(message)
        else:
          if has_fixed_language_mode:
            newargs[i] = ''
            input_files.append((i, arg))
          else:
            exit_with_error(arg + ": Input file has an unknown suffix, don't know what to do with it!")
      elif arg == '-r':
        link_to_object = True
        newargs[i] = ''
      elif arg.startswith('-L'):
        add_link_flag(i, arg)
        newargs[i] = ''
      elif arg.startswith('-l'):
        add_link_flag(i, arg)
        newargs[i] = ''
      elif arg.startswith('-Wl,'):
        # Multiple comma separated link flags can be specified. Create fake
        # fractional indices for these: -Wl,a,b,c,d at index 4 becomes:
        # (4, a), (4.25, b), (4.5, c), (4.75, d)
        link_flags_to_add = arg.split(',')[1:]
        for flag_index, flag in enumerate(link_flags_to_add):
          add_link_flag(i + float(flag_index) / len(link_flags_to_add), flag)
        newargs[i] = ''
      elif arg == '-Xlinker':
        add_link_flag(i + 1, newargs[i + 1])
        newargs[i] = ''
        newargs[i + 1] = ''
      elif arg == '-s':
        # -s and some other compiler flags are normally passed onto the linker
        # TODO(sbc): Pass this and other flags through when using lld
        # link_flags.append((i, arg))
        newargs[i] = ''
      elif arg == '-':
        input_files.append((i, arg))
        newargs[i] = ''
    newargs = [a for a in newargs if a]

    if has_dash_c or has_dash_S:
      if has_dash_c:
        if '-emit-llvm' in newargs:
          final_suffix = '.bc'
        else:
          final_suffix = options.default_object_extension
      elif has_dash_S:
        if '-emit-llvm' in newargs:
          final_suffix = '.ll'
        else:
          final_suffix = '.s'
      target = target_basename + final_suffix

      if len(input_files) > 1 and specified_target:
        exit_with_error('cannot specify -o with -c/-S and multiple source files')

    if has_dash_E:
      final_suffix = '.eout' # not bitcode, not js; but just result from preprocessing stage of the input file
    if '-M' in newargs or '-MM' in newargs:
      final_suffix = '.mout' # not bitcode, not js; but just dependency rule of the input file

    # target is now finalized, can finalize other _target s
    if final_suffix == '.mjs':
      shared.Settings.EXPORT_ES6 = 1
      shared.Settings.MODULARIZE = 1

    if final_suffix in ('.mjs', '.js', ''):
      js_target = target
    else:
      js_target = unsuffixed(target) + '.js'

    asm_target = unsuffixed(js_target) + '.asm.js' # might not be used, but if it is, this is the name
    wasm_text_target = asm_target.replace('.asm.js', '.wat') # ditto, might not be used
    wasm_binary_target = asm_target.replace('.asm.js', '.wasm') # ditto, might not be used
    wasm_source_map_target = shared.replace_or_append_suffix(wasm_binary_target, '.map')

    if final_suffix == '.html' and not options.separate_asm and 'PRECISE_F32=2' in settings_changes:
      options.separate_asm = True
      diagnostics.warning('emcc', 'forcing separate asm output (--separate-asm), because -s PRECISE_F32=2 was passed.')
    if options.separate_asm:
      shared.Settings.SEPARATE_ASM = shared.JS.get_subresource_location(asm_target)

    # Libraries are searched before settings_changes are applied, so apply the
    # value for STRICT from command line already now.

    def get_last_setting_change(setting):
      return ([None] + [x for x in settings_changes if x.startswith(setting + '=')])[-1]

    strict_cmdline = get_last_setting_change('STRICT')
    if strict_cmdline:
      shared.Settings.STRICT = int(strict_cmdline.split('=', 1)[1])

    if options.separate_asm and final_suffix != '.html':
      diagnostics.warning('separate-asm', "--separate-asm works best when compiling to HTML.  Otherwise, you must yourself load the '.asm.js' file that is emitted separately, and must do so before loading the main '.js' file.")

    # Apply optimization level settings
    shared.Settings.apply_opt_level(opt_level=shared.Settings.OPT_LEVEL, shrink_level=shared.Settings.SHRINK_LEVEL, noisy=True)

    # For users that opt out of WARN_ON_UNDEFINED_SYMBOLS we assume they also
    # want to opt out of ERROR_ON_UNDEFINED_SYMBOLS.
    if 'WARN_ON_UNDEFINED_SYMBOLS=0' in settings_changes:
      shared.Settings.ERROR_ON_UNDEFINED_SYMBOLS = 0

    if not shared.Settings.WASM_BACKEND:
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['memset', 'memcpy', 'emscripten_get_heap_size']

    if shared.Settings.MINIMAL_RUNTIME or 'MINIMAL_RUNTIME=1' in settings_changes or 'MINIMAL_RUNTIME=2' in settings_changes:
      # Remove the default exported functions 'malloc', 'free', etc. those should only be linked in if used
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE = []

    # Set ASM_JS default here so that we can override it from the command line.
    shared.Settings.ASM_JS = 1 if shared.Settings.OPT_LEVEL > 0 else 2

    # Remove the default _main function from shared.Settings.EXPORTED_FUNCTIONS.
    # We do this before the user settings are applied so it affects the default value only and a
    # user could use `--no-entry` and still export main too.
    if options.no_entry:
      shared.Settings.EXPORTED_FUNCTIONS.remove('_main')

    # Apply -s settings in newargs here (after optimization levels, so they can override them)
    apply_settings(settings_changes)

    shared.verify_settings()

    if options.no_entry or '_main' not in shared.Settings.EXPORTED_FUNCTIONS:
      shared.Settings.EXPECT_MAIN = 0

    if shared.Settings.STANDALONE_WASM:
      # In STANDALONE_WASM mode we either build a command or a reactor.
      # See https://github.com/WebAssembly/WASI/blob/master/design/application-abi.md
      # For a command we always want EXIT_RUNTIME=1
      # For a reactor we always want EXIT_RUNTIME=0
      if 'EXIT_RUNTIME' in settings_changes:
        exit_with_error('Explictly setting EXIT_RUNTIME not compatible with STANDALONE_WASM.  EXIT_RUNTIME will always be True for programs (with a main function) and False for reactors (not main function).')
      shared.Settings.EXIT_RUNTIME = not shared.Settings.EXPECT_MAIN

    def filter_out_dynamic_libs(inputs):
      # If not compiling to JS, then we are compiling to an intermediate bitcode
      # objects or library, so ignore dynamic linking, since multiple dynamic
      # linkings can interfere with each other
      if get_file_suffix(target) not in JS_CONTAINING_ENDINGS or options.ignore_dynamic_linking:
        def check(input_file):
          if get_file_suffix(input_file) in DYNAMICLIB_ENDINGS:
            if not options.ignore_dynamic_linking:
              diagnostics.warning('emcc', 'ignoring dynamic library %s because not compiling to JS or HTML, remember to link it when compiling to JS or HTML at the end', os.path.basename(input_file))
            return False
          else:
            return True
        return [f for f in inputs if check(f[1])]
      return inputs

    input_files = filter_out_dynamic_libs(input_files)

    if not input_files and not link_flags:
      exit_with_error('no input files\nnote that input files without a known suffix are ignored, make sure your input files end with one of: ' + str(SOURCE_ENDINGS + OBJECT_FILE_ENDINGS + DYNAMICLIB_ENDINGS + STATICLIB_ENDINGS + ASSEMBLY_ENDINGS + HEADER_ENDINGS))

    # Note the exports the user requested
    shared.Building.user_requested_exports = shared.Settings.EXPORTED_FUNCTIONS[:]

    # -s ASSERTIONS=1 implies the heaviest stack overflow check mode. Set the implication here explicitly to avoid having to
    # do preprocessor "#if defined(ASSERTIONS) || defined(STACK_OVERFLOW_CHECK)" in .js files, which is not supported.
    if shared.Settings.ASSERTIONS:
      shared.Settings.STACK_OVERFLOW_CHECK = 2

    if shared.Settings.LLD_REPORT_UNDEFINED:
      # Reporting undefined symbols at wasm-ld time requires us to know if we have a `main` function
      # or not.
      shared.Settings.IGNORE_MISSING_MAIN = 0

    if shared.Settings.STRICT:
      shared.Settings.STRICT_JS = 1
      shared.Settings.AUTO_JS_LIBRARIES = 0
      shared.Settings.AUTO_ARCHIVE_INDEXES = 0
      shared.Settings.IGNORE_MISSING_MAIN = 0
      shared.Settings.DEFAULT_TO_CXX = 0

    # If set to 1, we will run the autodebugger (the automatic debugging tool, see
    # tools/autodebugger).  Note that this will disable inclusion of libraries. This
    # is useful because including dlmalloc makes it hard to compare native and js
    # builds
    if os.environ.get('EMCC_AUTODEBUG'):
      shared.Settings.AUTODEBUG = 1

    # Use settings

    if shared.Settings.DEBUG_LEVEL > 1 and options.use_closure_compiler:
      diagnostics.warning('emcc', 'disabling closure because debug info was requested')
      options.use_closure_compiler = False

    if shared.Settings.EMTERPRETIFY_FILE and shared.Settings.SINGLE_FILE:
      exit_with_error('cannot have both EMTERPRETIFY_FILE and SINGLE_FILE enabled at the same time')

    if shared.Settings.WASM == 2 and shared.Settings.SINGLE_FILE:
      exit_with_error('cannot have both WASM=2 and SINGLE_FILE enabled at the same time (pick either JS to target with -s WASM=0 or Wasm to target with -s WASM=1)')

    if shared.Settings.SEPARATE_DWARF and shared.Settings.WASM2JS:
      exit_with_error('cannot have both SEPARATE_DWARF and WASM2JS at the same time (as there is no wasm file)')

    if shared.Settings.MINIMAL_RUNTIME_STREAMING_WASM_COMPILATION and shared.Settings.MINIMAL_RUNTIME_STREAMING_WASM_INSTANTIATION:
      exit_with_error('MINIMAL_RUNTIME_STREAMING_WASM_COMPILATION and MINIMAL_RUNTIME_STREAMING_WASM_INSTANTIATION are mutually exclusive!')

    if options.emrun:
      if shared.Settings.MINIMAL_RUNTIME:
        exit_with_error('--emrun is not compatible with -s MINIMAL_RUNTIME=1')
      shared.Settings.EXPORTED_RUNTIME_METHODS.append('addOnExit')

    if options.use_closure_compiler:
      shared.Settings.USE_CLOSURE_COMPILER = options.use_closure_compiler
      # when we emit asm.js, closure 2 would break that, so warn (note that
      # with wasm2js in the wasm backend, we don't emit asm.js anyhow)
      if options.use_closure_compiler == 2 and shared.Settings.ASM_JS == 1 and not shared.Settings.WASM_BACKEND:
        diagnostics.warning('almost-asm', 'not all asm.js optimizations are possible with --closure 2, disabling those - your code will be run more slowly')
        shared.Settings.ASM_JS = 2

    if shared.Settings.CLOSURE_WARNINGS not in ['quiet', 'warn', 'error']:
      exit_with_error('Invalid option -s CLOSURE_WARNINGS=%s specified! Allowed values are "quiet", "warn" or "error".' % shared.Settings.CLOSURE_WARNINGS)

    if shared.Settings.MAIN_MODULE:
      assert not shared.Settings.SIDE_MODULE
      if shared.Settings.MAIN_MODULE == 1:
        shared.Settings.INCLUDE_FULL_LIBRARY = 1
    elif shared.Settings.SIDE_MODULE:
      assert not shared.Settings.MAIN_MODULE
      # memory init file is not supported with asm.js side modules, must be executable synchronously (for dlopen)
      options.memory_init_file = False

    if shared.Settings.MAIN_MODULE or shared.Settings.SIDE_MODULE:
      assert shared.Settings.ASM_JS, 'module linking requires asm.js output (-s ASM_JS=1)'
      if shared.Settings.MAIN_MODULE == 1 or shared.Settings.SIDE_MODULE == 1:
        shared.Settings.LINKABLE = 1
        shared.Settings.EXPORT_ALL = 1
      shared.Settings.RELOCATABLE = 1
      assert not options.use_closure_compiler, 'cannot use closure compiler on shared modules'
      # shared modules need memory utilities to allocate their memory
      shared.Settings.EXPORTED_RUNTIME_METHODS += [
        'allocate',
        'getMemory',
      ]

    if shared.Settings.RELOCATABLE:
      shared.Settings.ALLOW_TABLE_GROWTH = 1

    # Reconfigure the cache now that settings have been applied. Some settings
    # such as LTO and SIDE_MODULE/MAIN_MODULE effect which cache directory we use.
    shared.reconfigure_cache()

    def has_c_source(args):
      for a in args:
        if a[0] != '-' and a.endswith(C_ENDINGS + OBJC_ENDINGS):
          return True
      return False

    use_cxx = False
    if has_fixed_language_mode:
      if 'c++' in language_mode:
        use_cxx = True
    elif run_via_emxx:
      use_cxx = True
    elif shared.Settings.DEFAULT_TO_CXX and not has_c_source(args):
      # Default to using C++ even when run as `emcc`.
      # This means that emcc will act as a C++ linker when no source files are
      # specified.
      # This differs to clang and gcc where the default is always C unless run as
      # clang++/g++.
      use_cxx = True
    shared.Settings.USE_CXX = use_cxx

    if not compile_only:
      ldflags = shared.emsdk_ldflags(newargs)
      for f in ldflags:
        newargs.append(f)
        add_link_flag(len(newargs), f)

    # Flags we pass to the compiler when building C/C++ code
    # We add these to the user's flags (newargs), but not when building .s or .S assembly files
    cflags = shared.get_cflags(newargs)

    if not shared.Settings.STRICT:
      # The preprocessor define EMSCRIPTEN is deprecated. Don't pass it to code
      # in strict mode. Code should use the define __EMSCRIPTEN__ instead.
      cflags.append('-DEMSCRIPTEN')

    # Treat the empty extension as an executable, to handle the commond case of `emcc -o foo foo.c`
    executable_endings = JS_CONTAINING_ENDINGS + WASM_ENDINGS + ('',)
    if not link_to_object and not compile_only and final_suffix not in executable_endings:
      # TODO(sbc): Remove this emscripten-specific special case.  We should only generate object
      # file output with an explicit `-c` or `-r`.
      diagnostics.warning('emcc', 'Assuming object file output in the absence of `-c`, based on output filename. Add with `-c` or `-r` to avoid this warning')
      link_to_object = True

    if shared.Settings.STACK_OVERFLOW_CHECK:
      if shared.Settings.MINIMAL_RUNTIME:
        shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$abortStackOverflow']
        shared.Settings.EXPORTED_RUNTIME_METHODS += ['writeStackCookie', 'checkStackCookie']
      else:
        shared.Settings.EXPORTED_RUNTIME_METHODS += ['writeStackCookie', 'checkStackCookie', 'abortStackOverflow']

    if shared.Settings.MODULARIZE:
      assert not options.proxy_to_worker, '-s MODULARIZE=1 is not compatible with --proxy-to-worker (if you want to run in a worker with -s MODULARIZE=1, you likely want to do the worker side setup manually)'
      # in MINIMAL_RUNTIME we may not need to emit the Promise code, as the
      # HTML output creates a singleton instance, and it does so without the
      # Promise. However, in Pthreads mode the Promise is used for worker
      # creation.
      if shared.Settings.MINIMAL_RUNTIME and final_suffix == '.html' and \
         not shared.Settings.USE_PTHREADS:
        shared.Settings.EXPORT_READY_PROMISE = 0

    if shared.Settings.EMULATE_FUNCTION_POINTER_CASTS:
      shared.Settings.ALIASING_FUNCTION_POINTERS = 0

    if shared.Settings.LEGACY_VM_SUPPORT:
      if not shared.Settings.WASM or shared.Settings.WASM2JS:
        shared.Settings.POLYFILL_OLD_MATH_FUNCTIONS = 1

      # Support all old browser versions
      shared.Settings.MIN_FIREFOX_VERSION = 0
      shared.Settings.MIN_SAFARI_VERSION = 0
      shared.Settings.MIN_IE_VERSION = 0
      shared.Settings.MIN_EDGE_VERSION = 0
      shared.Settings.MIN_CHROME_VERSION = 0

    if shared.Settings.MIN_SAFARI_VERSION <= 9 and (not shared.Settings.WASM or shared.Settings.WASM2JS):
      shared.Settings.WORKAROUND_IOS_9_RIGHT_SHIFT_BUG = 1

    if shared.Settings.MIN_CHROME_VERSION <= 37:
      shared.Settings.WORKAROUND_OLD_WEBGL_UNIFORM_UPLOAD_IGNORED_OFFSET_BUG = 1

    # Silently drop any individual backwards compatibility emulation flags that are known never to occur on browsers that support WebAssembly.
    if shared.Settings.WASM and not shared.Settings.WASM2JS:
      shared.Settings.POLYFILL_OLD_MATH_FUNCTIONS = 0
      shared.Settings.WORKAROUND_IOS_9_RIGHT_SHIFT_BUG = 0
      shared.Settings.WORKAROUND_OLD_WEBGL_UNIFORM_UPLOAD_IGNORED_OFFSET_BUG = 0

    if shared.Settings.STB_IMAGE and final_suffix in JS_CONTAINING_ENDINGS:
      input_files.append((next_arg_index, shared.path_from_root('third_party', 'stb_image.c')))
      next_arg_index += 1
      shared.Settings.EXPORTED_FUNCTIONS += ['_stbi_load', '_stbi_load_from_memory', '_stbi_image_free']
      # stb_image 2.x need to have STB_IMAGE_IMPLEMENTATION defined to include the implementation when compiling
      cflags.append('-DSTB_IMAGE_IMPLEMENTATION')

    if shared.Settings.USE_WEBGL2:
      shared.Settings.MAX_WEBGL_VERSION = 2

    if not shared.Settings.GL_SUPPORT_SIMPLE_ENABLE_EXTENSIONS and shared.Settings.GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS:
      exit_with_error('-s GL_SUPPORT_SIMPLE_ENABLE_EXTENSIONS=0 only makes sense with -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0!')

    forced_stdlibs = []

    if shared.Settings.ASMFS and final_suffix in JS_CONTAINING_ENDINGS:
      forced_stdlibs.append('libasmfs')
      cflags.append('-D__EMSCRIPTEN_ASMFS__=1')
      next_arg_index += 1
      shared.Settings.FILESYSTEM = 0
      shared.Settings.SYSCALLS_REQUIRE_FILESYSTEM = 0
      shared.Settings.FETCH = 1
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_asmfs.js')))

    # Explicitly drop linking in a malloc implementation if program is not using any dynamic allocation calls.
    if not shared.Settings.USES_DYNAMIC_ALLOC:
      shared.Settings.MALLOC = 'none'

    if shared.Settings.MALLOC == 'emmalloc':
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_emmalloc.js')))

    if shared.Settings.FETCH and final_suffix in JS_CONTAINING_ENDINGS:
      forced_stdlibs.append('libfetch')
      next_arg_index += 1
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_fetch.js')))
      if shared.Settings.USE_PTHREADS:
        shared.Settings.FETCH_WORKER_FILE = unsuffixed(os.path.basename(target)) + '.fetch.js'

    if not shared.Settings.USE_PTHREADS or not shared.Settings.FETCH:
      shared.Settings.USE_FETCH_WORKER = 0

    # In asm.js+pthreads we can use a fetch worker, which is made from the main
    # asm.js code. That lets us do sync operations by blocking on the worker etc.
    # In the wasm backend we don't have a fetch worker implemented yet, however,
    # we can still do basic synchronous fetches in the same places: if we can
    # block on another thread then we aren't the main thread, and if we aren't
    # the main thread then synchronous xhrs are legitimate.
    if shared.Settings.FETCH and shared.Settings.WASM_BACKEND:
      shared.Settings.USE_FETCH_WORKER = 0

    if shared.Settings.DEMANGLE_SUPPORT:
      shared.Settings.EXPORTED_FUNCTIONS += ['___cxa_demangle']

    if shared.Settings.FULL_ES3:
      shared.Settings.FULL_ES2 = 1
      shared.Settings.MAX_WEBGL_VERSION = max(2, shared.Settings.MAX_WEBGL_VERSION)

    if shared.Settings.EMBIND:
      forced_stdlibs.append('libembind')

    if not shared.Settings.MINIMAL_RUNTIME and not shared.Settings.STANDALONE_WASM:
      # The normal JS runtime depends on malloc and free so always keep them alive.
      # MINIMAL_RUNTIME avoids this dependency as does STANDALONE_WASM mode (since it has no
      # JS runtime at all).
      shared.Settings.EXPORTED_FUNCTIONS += ['_malloc', '_free']

    if shared.Settings.WASM_BACKEND:
      # We need to preserve the __data_end symbol so that wasm-emscripten-finalize can determine
      # the STATIC_BUMP value.
      shared.Settings.EXPORTED_FUNCTIONS += ['___data_end']
      if not shared.Settings.STANDALONE_WASM:
        # in standalone mode, crt1 will call the constructors from inside the wasm
        shared.Settings.EXPORTED_FUNCTIONS.append('___wasm_call_ctors')

    if shared.Settings.RELOCATABLE and not shared.Settings.DYNAMIC_EXECUTION:
      exit_with_error('cannot have both DYNAMIC_EXECUTION=0 and RELOCATABLE enabled at the same time, since RELOCATABLE needs to eval()')

    if shared.Settings.SIDE_MODULE and shared.Settings.GLOBAL_BASE != -1:
      exit_with_error('Cannot set GLOBAL_BASE when building SIDE_MODULE')

    if shared.Settings.RELOCATABLE:
      if 'EMULATED_FUNCTION_POINTERS' not in settings_key_changes and not shared.Settings.WASM_BACKEND:
        shared.Settings.EMULATED_FUNCTION_POINTERS = 2 # by default, use optimized function pointer emulation
      shared.Settings.ERROR_ON_UNDEFINED_SYMBOLS = 0
      shared.Settings.WARN_ON_UNDEFINED_SYMBOLS = 0

    if shared.Settings.WARN_ON_UNDEFINED_SYMBOLS:
      diagnostics.enable_warning('undefined', shared.Settings.ERROR_ON_UNDEFINED_SYMBOLS)
    else:
      diagnostics.disable_warning('undefined')

    if shared.Settings.ASYNCIFY:
      if not shared.Settings.WASM_BACKEND:
        exit_with_error('ASYNCIFY has been removed from fastcomp. There is a new implementation which can be used in the upstream wasm backend.')

    if shared.Settings.EMTERPRETIFY:
      diagnostics.warning('emterpreter', 'emterpreter is soon to be removed.  If you depend on this feature please reach out on github for help transitioning.')
      shared.Settings.FINALIZE_ASM_JS = 0
      shared.Settings.SIMPLIFY_IFS = 0 # this is just harmful for emterpreting
      shared.Settings.EXPORTED_FUNCTIONS += ['emterpret']
      if not options.js_opts:
        logger.debug('enabling js opts for EMTERPRETIFY')
        options.js_opts = True
      options.force_js_opts = True
      if options.use_closure_compiler == 2:
         exit_with_error('EMTERPRETIFY requires valid asm.js, and is incompatible with closure 2 which disables that')
      assert not use_source_map(options), 'EMTERPRETIFY is not compatible with source maps (maps are not useful in emterpreted code, and splitting out non-emterpreted source maps is not yet implemented)'

    if shared.Settings.DISABLE_EXCEPTION_THROWING and not shared.Settings.DISABLE_EXCEPTION_CATCHING:
      exit_with_error("DISABLE_EXCEPTION_THROWING was set (probably from -fno-exceptions) but is not compatible with enabling exception catching (DISABLE_EXCEPTION_CATCHING=0). If you don't want exceptions, set DISABLE_EXCEPTION_CATCHING to 1; if you do want exceptions, don't link with -fno-exceptions")

    # if exception catching is disabled, we can prevent that code from being
    # generated in the frontend
    if shared.Settings.DISABLE_EXCEPTION_CATCHING == 1 and shared.Settings.WASM_BACKEND and not shared.Settings.EXCEPTION_HANDLING:
      cflags.append('-fignore-exceptions')

    if shared.Settings.DEAD_FUNCTIONS:
      if not options.js_opts:
        logger.debug('enabling js opts for DEAD_FUNCTIONS')
        options.js_opts = True
      options.force_js_opts = True

    if options.proxy_to_worker:
      shared.Settings.PROXY_TO_WORKER = 1

    if options.use_preload_plugins or len(options.preload_files) or len(options.embed_files):
      if shared.Settings.NODERAWFS:
        exit_with_error('--preload-file and --embed-file cannot be used with NODERAWFS which disables virtual filesystem')
      # if we include any files, or intend to use preload plugins, then we definitely need filesystem support
      shared.Settings.FORCE_FILESYSTEM = 1

    if options.proxy_to_worker or options.use_preload_plugins:
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$Browser']

    if not shared.Settings.MINIMAL_RUNTIME:
      # In non-MINIMAL_RUNTIME, the core runtime depends on these functions to be present. (In MINIMAL_RUNTIME, they are
      # no longer always bundled in)
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$demangle', '$demangleAll', '$jsStackTrace', '$stackTrace']

    if shared.Settings.FILESYSTEM:
      # to flush streams on FS exit, we need to be able to call fflush
      # we only include it if the runtime is exitable, or when ASSERTIONS
      # (ASSERTIONS will check that streams do not need to be flushed,
      # helping people see when they should have enabled EXIT_RUNTIME)
      if shared.Settings.EXIT_RUNTIME or shared.Settings.ASSERTIONS:
        shared.Settings.EXPORTED_FUNCTIONS += ['_fflush']

    if shared.Settings.SUPPORT_ERRNO:
      # so setErrNo JS library function can report errno back to C
      shared.Settings.EXPORTED_FUNCTIONS += ['___errno_location']

    if shared.Settings.GLOBAL_BASE < 0:
      # default if nothing else sets it
      if shared.Settings.WASM:
        # a higher global base is useful for optimizing load/store offsets, as it
        # enables the --post-emscripten pass
        shared.Settings.GLOBAL_BASE = 1024
      else:
        shared.Settings.GLOBAL_BASE = 8

    if shared.Settings.SAFE_HEAP:
      # SAFE_HEAP check includes calling emscripten_get_sbrk_ptr().
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['emscripten_get_sbrk_ptr']

    if not shared.Settings.DECLARE_ASM_MODULE_EXPORTS:
      shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$exportAsmFunctions']

    if shared.Settings.ALLOW_MEMORY_GROWTH and 'ABORTING_MALLOC=1' not in settings_changes:
      # Setting ALLOW_MEMORY_GROWTH turns off ABORTING_MALLOC, as in that mode we default to
      # the behavior of trying to grow and returning 0 from malloc on failure, like
      # a standard system would. However, if the user sets the flag it
      # overrides that.
      shared.Settings.ABORTING_MALLOC = 0

    if shared.Settings.USE_PTHREADS:
      if shared.Settings.USE_PTHREADS == 2:
        exit_with_error('USE_PTHREADS=2 is not longer supported')
      if shared.Settings.ALLOW_MEMORY_GROWTH:
        if not shared.Settings.WASM:
          exit_with_error('Memory growth is not supported with pthreads without wasm')
        else:
          logging.warning('USE_PTHREADS + ALLOW_MEMORY_GROWTH may run non-wasm code slowly, see https://github.com/WebAssembly/design/issues/1271')
      # UTF8Decoder.decode doesn't work with a view of a SharedArrayBuffer
      shared.Settings.TEXTDECODER = 0
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_pthread.js')))
      cflags.append('-D__EMSCRIPTEN_PTHREADS__=1')
      if shared.Settings.WASM_BACKEND:
        newargs += ['-pthread']
        # some pthreads code is in asm.js library functions, which are auto-exported; for the wasm backend, we must
        # manually export them

        shared.Settings.EXPORTED_FUNCTIONS += [
          '_emscripten_get_global_libc', '___pthread_tsd_run_dtors',
          'registerPthreadPtr', '_pthread_self',
          '___emscripten_pthread_data_constructor', '_emscripten_futex_wake']

      # set location of worker.js
      shared.Settings.PTHREAD_WORKER_FILE = unsuffixed(os.path.basename(target)) + '.worker.js'
    else:
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_pthread_stub.js')))

    if shared.Settings.FORCE_FILESYSTEM and not shared.Settings.MINIMAL_RUNTIME:
      # when the filesystem is forced, we export by default methods that filesystem usage
      # may need, including filesystem usage from standalone file packager output (i.e.
      # file packages not built together with emcc, but that are loaded at runtime
      # separately, and they need emcc's output to contain the support they need)
      if not shared.Settings.ASMFS:
        shared.Settings.EXPORTED_RUNTIME_METHODS += [
          'FS_createFolder',
          'FS_createPath',
          'FS_createDataFile',
          'FS_createPreloadedFile',
          'FS_createLazyFile',
          'FS_createLink',
          'FS_createDevice',
          'FS_unlink'
        ]

      shared.Settings.EXPORTED_RUNTIME_METHODS += [
        'getMemory',
        'addRunDependency',
        'removeRunDependency',
      ]

    if shared.Settings.USE_PTHREADS:
      # memalign is used to ensure allocated thread stacks are aligned.
      shared.Settings.EXPORTED_FUNCTIONS += ['_memalign', '_malloc']

      # dynCall_ii is used to call pthread entry points in worker.js (as
      # metadce does not consider worker.js, which is external, we must
      # consider it a user export, i.e., one which can never be removed).
      shared.Building.user_requested_exports += ['dynCall_ii']

      if shared.Settings.MINIMAL_RUNTIME:
        shared.Building.user_requested_exports += ['exit']

      if shared.Settings.PROXY_TO_PTHREAD:
        shared.Settings.EXPORTED_FUNCTIONS += ['_proxy_main']

      # pthread stack setup and other necessary utilities
      def include_and_export(name):
        shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$' + name]
        shared.Settings.EXPORTED_FUNCTIONS += [name]

      include_and_export('establishStackSpace')
      if not shared.Settings.MINIMAL_RUNTIME:
        # noExitRuntime does not apply to MINIMAL_RUNTIME.
        include_and_export('getNoExitRuntime')

      if shared.Settings.MODULARIZE:
        # MODULARIZE+USE_PTHREADS mode requires extra exports out to Module so that worker.js
        # can access them:

        # general threading variables:
        shared.Settings.EXPORTED_RUNTIME_METHODS += ['PThread']

        # To keep code size to minimum, MINIMAL_RUNTIME does not utilize the global ExitStatus
        # object, only regular runtime has it.
        if not shared.Settings.MINIMAL_RUNTIME:
          shared.Settings.EXPORTED_RUNTIME_METHODS += ['ExitStatus']

        # stack check:
        if shared.Settings.STACK_OVERFLOW_CHECK:
          shared.Settings.EXPORTED_RUNTIME_METHODS += ['writeStackCookie', 'checkStackCookie']

      if shared.Settings.LINKABLE:
        exit_with_error('-s LINKABLE=1 is not supported with -s USE_PTHREADS>0!')
      if shared.Settings.SIDE_MODULE:
        exit_with_error('-s SIDE_MODULE=1 is not supported with -s USE_PTHREADS>0!')
      if shared.Settings.MAIN_MODULE:
        exit_with_error('-s MAIN_MODULE=1 is not supported with -s USE_PTHREADS>0!')
      if shared.Settings.EMTERPRETIFY:
        exit_with_error('-s EMTERPRETIFY=1 is not supported with -s USE_PTHREADS>0!')
      if shared.Settings.PROXY_TO_WORKER:
        exit_with_error('--proxy-to-worker is not supported with -s USE_PTHREADS>0! Use the option -s PROXY_TO_PTHREAD=1 if you want to run the main thread of a multithreaded application in a web worker.')
    else:
      if shared.Settings.PROXY_TO_PTHREAD:
        exit_with_error('-s PROXY_TO_PTHREAD=1 requires -s USE_PTHREADS to work!')

    # Enable minification of asm.js imports on -O1 and higher if -g1 or lower is used.
    if shared.Settings.OPT_LEVEL >= 1 and shared.Settings.DEBUG_LEVEL < 2 and not shared.Settings.WASM:
      shared.Settings.MINIFY_ASMJS_IMPORT_NAMES = 1

    if shared.Settings.WASM:
      if not shared.Building.need_asm_js_file():
        asm_target = asm_target.replace('.asm.js', '.temp.asm.js')
        misc_temp_files.note(asm_target)

    if shared.Settings.WASM:
      if shared.Settings.INITIAL_MEMORY % 65536 != 0:
        exit_with_error('For wasm, INITIAL_MEMORY must be a multiple of 64KB, was ' + str(shared.Settings.INITIAL_MEMORY))
      if shared.Settings.INITIAL_MEMORY >= 2 * 1024 * 1024 * 1024:
        exit_with_error('INITIAL_MEMORY must be less than 2GB due to current spec limitations')
    else:
      if shared.Settings.INITIAL_MEMORY < 16 * 1024 * 1024:
        exit_with_error('INITIAL_MEMORY must be at least 16MB, was ' + str(shared.Settings.INITIAL_MEMORY))
      if shared.Settings.INITIAL_MEMORY % (16 * 1024 * 1024) != 0:
        exit_with_error('For asm.js, INITIAL_MEMORY must be a multiple of 16MB, was ' + str(shared.Settings.INITIAL_MEMORY))
    if shared.Settings.INITIAL_MEMORY < shared.Settings.TOTAL_STACK:
      exit_with_error('INITIAL_MEMORY must be larger than TOTAL_STACK, was ' + str(shared.Settings.INITIAL_MEMORY) + ' (TOTAL_STACK=' + str(shared.Settings.TOTAL_STACK) + ')')
    if shared.Settings.MAXIMUM_MEMORY != -1 and shared.Settings.MAXIMUM_MEMORY % 65536 != 0:
      exit_with_error('MAXIMUM_MEMORY must be a multiple of 64KB, was ' + str(shared.Settings.MAXIMUM_MEMORY))
    if shared.Settings.MEMORY_GROWTH_LINEAR_STEP != -1 and shared.Settings.MEMORY_GROWTH_LINEAR_STEP % 65536 != 0:
      exit_with_error('MEMORY_GROWTH_LINEAR_STEP must be a multiple of 64KB, was ' + str(shared.Settings.MEMORY_GROWTH_LINEAR_STEP))
    if shared.Settings.USE_PTHREADS and shared.Settings.WASM and shared.Settings.ALLOW_MEMORY_GROWTH and shared.Settings.MAXIMUM_MEMORY == -1:
      exit_with_error('If pthreads and memory growth are enabled, MAXIMUM_MEMORY must be set')

    if shared.Settings.EXPORT_ES6 and not shared.Settings.MODULARIZE:
      exit_with_error('EXPORT_ES6 requires MODULARIZE to be set')

    if shared.Settings.MODULARIZE and not shared.Settings.DECLARE_ASM_MODULE_EXPORTS:
      # When MODULARIZE option is used, currently requires declaring all module exports
      # individually - TODO: this could be optimized
      exit_with_error('DECLARE_ASM_MODULE_EXPORTS=0 is not compatible with MODULARIZE')

    # When not declaring asm module exports in outer scope one by one, disable minifying
    # asm.js/wasm module export names so that the names can be passed directly to the outer scope.
    # Also, if using library_exports.js API, disable minification so that the feature can work.
    if not shared.Settings.DECLARE_ASM_MODULE_EXPORTS or 'exports.js' in [x for _, x in libs]:
      shared.Settings.MINIFY_ASMJS_EXPORT_NAMES = 0

    # Enable minification of wasm imports and exports when appropriate, if we
    # are emitting an optimized JS+wasm combo (then the JS knows how to load the minified names).
    # Things that process the JS after this operation would be done must disable this.
    # For example, ASYNCIFY_LAZY_LOAD_CODE needs to identify import names, and wasm2js
    # needs to use the getTempRet0 imports (otherwise, it may create new ones to replace
    # the old, which would break).
    if will_metadce(options) and \
        shared.Settings.OPT_LEVEL >= 2 and \
        shared.Settings.DEBUG_LEVEL <= 2 and \
        not shared.Settings.LINKABLE and \
        not shared.Settings.STANDALONE_WASM and \
        not shared.Settings.AUTODEBUG and \
        not shared.Settings.ASSERTIONS and \
        not shared.Settings.RELOCATABLE and \
        not target.endswith(WASM_ENDINGS) and \
        not shared.Settings.ASYNCIFY_LAZY_LOAD_CODE and \
        not shared.Settings.WASM2JS and \
            shared.Settings.MINIFY_ASMJS_EXPORT_NAMES:
      shared.Settings.MINIFY_WASM_IMPORTS_AND_EXPORTS = 1
      # in fastcomp it's inconvenient to minify module names as there is the
      # asm2wasm module etc.
      if shared.Settings.WASM_BACKEND:
        shared.Settings.MINIFY_WASM_IMPORTED_MODULES = 1

    # In MINIMAL_RUNTIME when modularizing, by default output asm.js module under the same name as
    # the JS module. This allows code to share same loading function for both JS and asm.js modules,
    # to save code size. The intent is that loader code captures the function variable from global
    # scope to XHR loader local scope when it finishes loading, to avoid polluting global JS scope
    # with variables. This provides safety via encapsulation. See src/shell_minimal_runtime.html for
    # an example.
    if shared.Settings.MINIMAL_RUNTIME and not shared.Settings.SEPARATE_ASM_MODULE_NAME and not shared.Settings.WASM and shared.Settings.MODULARIZE:
      shared.Settings.SEPARATE_ASM_MODULE_NAME = 'var ' + shared.Settings.EXPORT_NAME

    if shared.Settings.MODULARIZE and shared.Settings.SEPARATE_ASM and not shared.Settings.WASM and not shared.Settings.SEPARATE_ASM_MODULE_NAME:
      exit_with_error('Targeting asm.js with --separate-asm and -s MODULARIZE=1 requires specifying the target variable name to which the asm.js module is loaded into. See https://github.com/emscripten-core/emscripten/pull/7949 for details')
    # Apply default option if no custom name is provided
    if not shared.Settings.SEPARATE_ASM_MODULE_NAME:
      shared.Settings.SEPARATE_ASM_MODULE_NAME = 'Module["asm"]'
    elif shared.Settings.WASM:
      exit_with_error('-s SEPARATE_ASM_MODULE_NAME option only applies to when targeting asm.js, not with WebAssembly!')

    if shared.Settings.MINIMAL_RUNTIME:
      # Minimal runtime uses a different default shell file
      if options.shell_path == shared.path_from_root('src', 'shell.html'):
        options.shell_path = shared.path_from_root('src', 'shell_minimal_runtime.html')

      if shared.Settings.ASSERTIONS and shared.Settings.MINIMAL_RUNTIME:
        # In ASSERTIONS-builds, functions UTF8ArrayToString() and stringToUTF8Array() (which are not JS library functions), both
        # use warnOnce(), which in MINIMAL_RUNTIME is a JS library function, so explicitly have to mark dependency to warnOnce()
        # in that case. If string functions are turned to library functions in the future, then JS dependency tracking can be
        # used and this special directive can be dropped.
        shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['$warnOnce']

      # Require explicit -lfoo.js flags to link with JS libraries.
      shared.Settings.AUTO_JS_LIBRARIES = 0

      # In asm.js always use memory init file to get the best code size, other modes are not currently supported.
      if not shared.Settings.WASM and not shared.Settings.WASM_BACKEND:
        options.memory_init_file = True

    if shared.Settings.MODULARIZE and shared.Settings.EXPORT_NAME == 'Module' and final_suffix == '.html' and \
       (options.shell_path == shared.path_from_root('src', 'shell.html') or options.shell_path == shared.path_from_root('src', 'shell_minimal.html')):
      exit_with_error('Due to collision in variable name "Module", the shell file "' + options.shell_path + '" is not compatible with build options "-s MODULARIZE=1 -s EXPORT_NAME=Module". Either provide your own shell file, change the name of the export to something else to avoid the name collision. (see https://github.com/emscripten-core/emscripten/issues/7950 for details)')

    if final_suffix in WASM_ENDINGS:
      # if the output is just a wasm file, it will normally be a standalone one,
      # as there is no JS. an exception are side modules, as we can't tell at
      # compile time whether JS will be involved or not - the main module may
      # have JS, and the side module is expected to link against that.
      # we also do not support standalone mode in fastcomp.
      if shared.Settings.WASM_BACKEND and not shared.Settings.SIDE_MODULE:
        shared.Settings.STANDALONE_WASM = 1
      js_target = misc_temp_files.get(suffix='.js').name

    if shared.Settings.WASM:
      if shared.Settings.SINGLE_FILE:
        # placeholder strings for JS glue, to be replaced with subresource locations in do_binaryen
        shared.Settings.WASM_TEXT_FILE = shared.FilenameReplacementStrings.WASM_TEXT_FILE
        shared.Settings.WASM_BINARY_FILE = shared.FilenameReplacementStrings.WASM_BINARY_FILE
        shared.Settings.ASMJS_CODE_FILE = shared.FilenameReplacementStrings.ASMJS_CODE_FILE
      else:
        # set file locations, so that JS glue can find what it needs
        shared.Settings.WASM_TEXT_FILE = shared.JS.escape_for_js_string(os.path.basename(wasm_text_target))
        shared.Settings.WASM_BINARY_FILE = shared.JS.escape_for_js_string(os.path.basename(wasm_binary_target))
        shared.Settings.ASMJS_CODE_FILE = shared.JS.escape_for_js_string(os.path.basename(asm_target))
      shared.Settings.ASM_JS = 2 # when targeting wasm, we use a wasm Memory, but that is not compatible with asm.js opts
      if shared.Settings.ELIMINATE_DUPLICATE_FUNCTIONS:
        diagnostics.warning('emcc', 'for wasm there is no need to set ELIMINATE_DUPLICATE_FUNCTIONS, the binaryen optimizer does it automatically')
        shared.Settings.ELIMINATE_DUPLICATE_FUNCTIONS = 0
      # default precise-f32 to on, since it works well in wasm
      shared.Settings.PRECISE_F32 = 1
      if options.js_opts and not options.force_js_opts:
        options.js_opts = None
        logger.debug('asm.js opts not forced by user or an option that depends them, and we do not intend to run the asm.js, so disabling and leaving opts to the binaryen optimizer')
      if options.use_closure_compiler == 2 and not shared.Settings.WASM2JS:
        exit_with_error('closure compiler mode 2 assumes the code is asm.js, so not meaningful for wasm')
      if any(s.startswith('MEM_INIT_METHOD=') for s in settings_changes):
        exit_with_error('MEM_INIT_METHOD is not supported in wasm. Memory will be embedded in the wasm binary if threads are not used, and included in a separate file if threads are used.')
      if shared.Settings.WASM2JS:
        shared.Settings.MAYBE_WASM2JS = 1
        # wasm2js does not support passive segments or atomics
        if shared.Settings.USE_PTHREADS:
          exit_with_error('WASM2JS does not yet support pthreads')
        # in wasm2js, keep the mem init in the wasm itself if we can and if the
        # options wouldn't tell a js build to use a separate mem init file
        shared.Settings.MEM_INIT_IN_WASM = not options.memory_init_file or shared.Settings.SINGLE_FILE
      else:
        # wasm includes the mem init in the wasm binary. The exception is
        # wasm2js, which behaves more like js.
        options.memory_init_file = True
        shared.Settings.MEM_INIT_IN_WASM = True if shared.Settings.WASM_BACKEND else not shared.Settings.USE_PTHREADS

      # wasm side modules have suffix .wasm
      if shared.Settings.SIDE_MODULE and target.endswith('.js'):
        diagnostics.warning('emcc', 'output suffix .js requested, but wasm side modules are just wasm files; emitting only a .wasm, no .js')

      if options.separate_asm:
        exit_with_error('cannot --separate-asm when emitting wasm, since not emitting asm.js')

      sanitize = set()

      for arg in newargs:
        if arg.startswith('-fsanitize='):
          sanitize.update(arg.split('=', 1)[1].split(','))
        elif arg.startswith('-fno-sanitize='):
          sanitize.difference_update(arg.split('=', 1)[1].split(','))

      if sanitize:
        shared.Settings.USE_OFFSET_CONVERTER = 1
        shared.Settings.EXPORTED_FUNCTIONS += ['_memalign', '_emscripten_builtin_memalign',
                                               '_emscripten_builtin_malloc', '_emscripten_builtin_free',
                                               '___data_end', '___heap_base', '___global_base']

        if not shared.Settings.WASM_BACKEND:
          exit_with_error('Sanitizers are not compatible with the fastcomp backend. Please upgrade to the upstream wasm backend by following these instructions: https://v8.dev/blog/emscripten-llvm-wasm#testing')

      if sanitize & UBSAN_SANITIZERS:
        if '-fsanitize-minimal-runtime' in newargs:
          shared.Settings.UBSAN_RUNTIME = 1
        else:
          shared.Settings.UBSAN_RUNTIME = 2

      if 'leak' in sanitize:
        shared.Settings.USE_LSAN = 1
        shared.Settings.EXIT_RUNTIME = 1

        if shared.Settings.LINKABLE:
          exit_with_error('LSan does not support dynamic linking')

      if 'address' in sanitize:
        shared.Settings.USE_ASAN = 1

        shared.Settings.EXPORTED_FUNCTIONS += [
          '_asan_c_load_1', '_asan_c_load_1u',
          '_asan_c_load_2', '_asan_c_load_2u',
          '_asan_c_load_4', '_asan_c_load_4u',
          '_asan_c_load_f', '_asan_c_load_d',
          '_asan_c_store_1', '_asan_c_store_1u',
          '_asan_c_store_2', '_asan_c_store_2u',
          '_asan_c_store_4', '_asan_c_store_4u',
          '_asan_c_store_f', '_asan_c_store_d',
        ]

        shared.Settings.GLOBAL_BASE = shared.Settings.ASAN_SHADOW_SIZE
        shared.Settings.INITIAL_MEMORY += shared.Settings.ASAN_SHADOW_SIZE
        assert shared.Settings.INITIAL_MEMORY < 2**32

        if shared.Settings.SAFE_HEAP:
          # SAFE_HEAP instruments ASan's shadow memory accesses.
          # Since the shadow memory starts at 0, the act of accessing the shadow memory is detected
          # by SAFE_HEAP as a null pointer dereference.
          exit_with_error('ASan does not work with SAFE_HEAP')

        if shared.Settings.LINKABLE:
          exit_with_error('ASan does not support dynamic linking')

      if sanitize and '-g4' in args:
        shared.Settings.LOAD_SOURCE_MAP = 1

      if shared.Settings.WASM_BACKEND:
        options.js_opts = None

        # wasm backend output can benefit from the binaryen optimizer (in asm2wasm,
        # we run the optimizer during asm2wasm itself). use it, if not overridden.

        passes = []
        # safe heap must run before post-emscripten, so post-emscripten can apply the sbrk ptr
        if shared.Settings.SAFE_HEAP:
          passes += ['--safe-heap']
        passes += ['--post-emscripten']
        # always inline __original_main into main, as otherwise it makes debugging confusing,
        # and doing so is never bad for code size
        # FIXME however, don't do it with DWARF for now, as inlining is not
        #       fully handled in DWARF updating yet
        if shared.Settings.DEBUG_LEVEL < 3:
          passes += ['--inline-main']
        if not shared.Settings.EXIT_RUNTIME:
          passes += ['--no-exit-runtime']
        if shared.Settings.OPT_LEVEL > 0 or shared.Settings.SHRINK_LEVEL > 0:
          passes += [shared.Building.opt_level_to_str(shared.Settings.OPT_LEVEL, shared.Settings.SHRINK_LEVEL)]
        elif shared.Settings.STANDALONE_WASM:
          # even if not optimizing, make an effort to remove all unused imports and
          # exports, to make the wasm as standalone as possible
          passes += ['--remove-unused-module-elements']
        if shared.Settings.GLOBAL_BASE >= 1024: # hardcoded value in the binaryen pass
          passes += ['--low-memory-unused']
        if shared.Settings.DEBUG_LEVEL < 3:
          passes += ['--strip-debug']
        if not shared.Settings.EMIT_PRODUCERS_SECTION:
          passes += ['--strip-producers']
        if shared.Settings.AUTODEBUG:
          # adding '--flatten' here may make these even more effective
          passes += ['--instrument-locals']
          passes += ['--log-execution']
          passes += ['--instrument-memory']
          passes += ['--legalize-js-interface']
        if shared.Settings.EMULATE_FUNCTION_POINTER_CASTS:
          # note that this pass must run before asyncify, as if it runs afterwards we only
          # generate the  byn$fpcast_emu  functions after asyncify runs, and so we wouldn't
          # be able to whitelist them etc.
          passes += ['--fpcast-emu']
        if shared.Settings.ASYNCIFY:
          # TODO: allow whitelist as in asyncify
          passes += ['--asyncify']
          if shared.Settings.ASSERTIONS:
            passes += ['--pass-arg=asyncify-asserts']
          if shared.Settings.ASYNCIFY_IGNORE_INDIRECT:
            passes += ['--pass-arg=asyncify-ignore-indirect']
          else:
            # if we are not ignoring indirect calls, then we must treat invoke_* as if
            # they are indirect calls, since that is what they do - we can't see their
            # targets statically.
            shared.Settings.ASYNCIFY_IMPORTS += ['invoke_*']
          # with pthreads we may call main through the __call_main mechanism, which can
          # therefore reach anything in the program, so mark it as possibly causing a
          # sleep (the asyncify analysis doesn't look through JS, just wasm, so it can't
          # see what it itself calls)
          if shared.Settings.USE_PTHREADS:
            shared.Settings.ASYNCIFY_IMPORTS += ['__call_main']
          # add the default imports
          shared.Settings.ASYNCIFY_IMPORTS += DEFAULT_ASYNCIFY_IMPORTS

          # return the full import name, including module. The name may
          # already have a module prefix; if not, we assume it is "env".
          def get_full_import_name(name):
            if '.' in name:
              return name
            return 'env.' + name

          shared.Settings.ASYNCIFY_IMPORTS = [get_full_import_name(i) for i in shared.Settings.ASYNCIFY_IMPORTS]

          passes += ['--pass-arg=asyncify-imports@%s' % ','.join(shared.Settings.ASYNCIFY_IMPORTS)]

          # shell escaping can be confusing; try to emit useful warnings
          def check_human_readable_list(items):
            for item in items:
              if item.count('(') != item.count(')'):
                logger.warning('''emcc: ASYNCIFY list contains an item without balanced parentheses ("(", ")"):''')
                logger.warning('''   ''' + item)
                logger.warning('''This may indicate improper escaping that led to splitting inside your names.''')
                logger.warning('''Try to quote the entire argument, like this: -s 'ASYNCIFY_WHITELIST=["foo(int, char)", "bar"]' ''')
                break

          if shared.Settings.ASYNCIFY_BLACKLIST:
            check_human_readable_list(shared.Settings.ASYNCIFY_BLACKLIST)
            passes += ['--pass-arg=asyncify-blacklist@%s' % ','.join(shared.Settings.ASYNCIFY_BLACKLIST)]
          if shared.Settings.ASYNCIFY_WHITELIST:
            check_human_readable_list(shared.Settings.ASYNCIFY_WHITELIST)
            passes += ['--pass-arg=asyncify-whitelist@%s' % ','.join(shared.Settings.ASYNCIFY_WHITELIST)]
        if shared.Settings.BINARYEN_IGNORE_IMPLICIT_TRAPS:
          passes += ['--ignore-implicit-traps']

        if shared.Settings.BINARYEN_EXTRA_PASSES:
          # BINARYEN_EXTRA_PASSES is comma-separated, and we support both '-'-prefixed and
          # unprefixed pass names
          extras = shared.Settings.BINARYEN_EXTRA_PASSES.split(',')
          passes += [('--' + p) if p[0] != '-' else p for p in extras if p]
        options.binaryen_passes = passes

      # run safe-heap as a binaryen pass in fastcomp wasm, while in the wasm backend we
      # run it in binaryen_passes so that it can be synchronized with the sbrk ptr
      if shared.Settings.SAFE_HEAP and shared.Building.is_wasm_only() and not shared.Settings.WASM_BACKEND:
        options.binaryen_passes += ['--safe-heap']
      if shared.Settings.EMULATE_FUNCTION_POINTER_CASTS and not shared.Settings.WASM_BACKEND:
        # emulated function pointer casts is emulated in fastcomp wasm using a binaryen pass
        options.binaryen_passes += ['--fpcast-emu']
        # we also need emulated function pointers for that, as we need a single flat
        # table, as is standard in wasm, and not asm.js split ones.
        shared.Settings.EMULATED_FUNCTION_POINTERS = 1

    if shared.Settings.WASM2JS:
      if not shared.Settings.WASM_BACKEND:
        exit_with_error('wasm2js is only available in the upstream wasm backend path')
      if use_source_map(options):
        exit_with_error('wasm2js does not support source maps yet (debug in wasm for now)')

    if shared.Settings.EVAL_CTORS and not shared.Settings.WASM:
      # for asm.js: this option is not a js optimizer pass, but does run the js optimizer internally, so
      # we need to generate proper code for that (for wasm, we run a binaryen tool for this)
      shared.Settings.RUNNING_JS_OPTS = 1

    # memory growth does not work in dynamic linking, except for wasm
    if (shared.Settings.MAIN_MODULE or shared.Settings.SIDE_MODULE) and shared.Settings.ALLOW_MEMORY_GROWTH and not shared.Settings.WASM:
      exit_with_error('memory growth is not supported with shared asm.js modules')

    if shared.Settings.MINIMAL_RUNTIME:
      if shared.Settings.EMTERPRETIFY:
        exit_with_error('-s EMTERPRETIFY=1 is not supported with -s MINIMAL_RUNTIME=1')

      if shared.Settings.PRECISE_F32 == 2:
        exit_with_error('-s PRECISE_F32=2 is not supported with -s MINIMAL_RUNTIME=1')

    if shared.Settings.ALLOW_MEMORY_GROWTH and shared.Settings.ASM_JS == 1:
      # this is an issue in asm.js, but not wasm
      if not shared.Settings.WASM:
        # memory growth does not validate as asm.js
        # http://discourse.wicg.io/t/request-for-comments-switching-resizing-heaps-in-asm-js/641/23
        diagnostics.warning('almost-asm', 'not all asm.js optimizations are possible with ALLOW_MEMORY_GROWTH, disabling those.')
        shared.Settings.ASM_JS = 2

    if shared.Settings.NODE_CODE_CACHING:
      if shared.Settings.WASM_ASYNC_COMPILATION:
        exit_with_error('NODE_CODE_CACHING requires sync compilation (WASM_ASYNC_COMPILATION=0)')
      if not shared.Settings.target_environment_may_be('node'):
        exit_with_error('NODE_CODE_CACHING only works in node, but target environments do not include it')
      if shared.Settings.SINGLE_FILE:
        exit_with_error('NODE_CODE_CACHING saves a file on the side and is not compatible with SINGLE_FILE')

    # safe heap in asm.js uses the js optimizer (in wasm-only mode we can use binaryen)
    if shared.Settings.SAFE_HEAP and not shared.Building.is_wasm_only():
      if not options.js_opts:
        logger.debug('enabling js opts for SAFE_HEAP')
        options.js_opts = True
      options.force_js_opts = True

    if options.js_opts:
      shared.Settings.RUNNING_JS_OPTS = 1

    if shared.Settings.CYBERDWARF:
      shared.Settings.DEBUG_LEVEL = max(shared.Settings.DEBUG_LEVEL, 2)
      shared.Settings.BUNDLED_CD_DEBUG_FILE = target + ".cd"
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_cyberdwarf.js')))
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_debugger_toolkit.js')))
      newargs.append('-g')

    if options.tracing:
      cflags.append('-D__EMSCRIPTEN_TRACING__=1')
      if shared.Settings.ALLOW_MEMORY_GROWTH:
        shared.Settings.DEFAULT_LIBRARY_FUNCS_TO_INCLUDE += ['emscripten_trace_report_memory_layout']

    if shared.Settings.STANDALONE_WASM:
      if not shared.Settings.WASM_BACKEND:
        exit_with_error('STANDALONE_WASM is only available in the upstream wasm backend path')
      if shared.Settings.USE_PTHREADS:
        exit_with_error('STANDALONE_WASM does not support pthreads yet')
      # the wasm must be runnable without the JS, so there cannot be anything that
      # requires JS legalization
      shared.Settings.LEGALIZE_JS_FFI = 0

    if shared.Settings.WASM_BIGINT:
      shared.Settings.LEGALIZE_JS_FFI = 0

    if shared.Settings.WASM_BACKEND:
      if shared.Settings.SIMD:
        newargs.append('-msimd128')
      if shared.Settings.USE_PTHREADS:
        newargs.append('-pthread')
    else:
      # We leave the -O option in place so that the clang front-end runs in that
      # optimization mode, but we disable the actual optimization passes, as we'll
      # run them separately.
      if shared.Settings.OPT_LEVEL > 0:
        newargs.append('-mllvm')
        newargs.append('-disable-llvm-optzns')

    if not shared.Settings.LEGALIZE_JS_FFI:
      assert shared.Building.is_wasm_only(), 'LEGALIZE_JS_FFI incompatible with RUNNING_JS_OPTS.'

    # check if we can address the 2GB mark and higher: either if we start at
    # 2GB, or if we allow growth to either any amount or to 2GB or more.
    if shared.Settings.WASM_BACKEND and \
       (shared.Settings.INITIAL_MEMORY > 2 * 1024 * 1024 * 1024 or
        (shared.Settings.ALLOW_MEMORY_GROWTH and
         (shared.Settings.MAXIMUM_MEMORY < 0 or
          shared.Settings.MAXIMUM_MEMORY > 2 * 1024 * 1024 * 1024))):
      shared.Settings.CAN_ADDRESS_2GB = 1
      if shared.Settings.MALLOC == 'emmalloc':
        if shared.Settings.INITIAL_MEMORY >= 2 * 1024 * 1024 * 1024:
          suggestion = 'decrease INITIAL_MEMORY'
        elif shared.Settings.MAXIMUM_MEMORY < 0:
          suggestion = 'set MAXIMUM_MEMORY'
        else:
          suggestion = 'decrease MAXIMUM_MEMORY'
        exit_with_error('emmalloc only works on <2GB of memory. Use the default allocator, or ' + suggestion)

    shared.Settings.EMSCRIPTEN_VERSION = shared.EMSCRIPTEN_VERSION
    shared.Settings.PROFILING_FUNCS = options.profiling_funcs
    shared.Settings.SOURCE_MAP_BASE = options.source_map_base or ''

    ## Compile source code to bitcode

    logger.debug('compiling to bitcode')

    temp_files = []

  # exit block 'parse arguments and setup'
  log_time('parse arguments and setup')

  if DEBUG:
    # we are about to start using temp dirs. serialize access to the temp dir
    # when using EMCC_DEBUG, since we don't want multiple processes would to
    # use it at once, they might collide if they happen to use the same
    # tempfile names
    shared.Cache.acquire_cache_lock()

  try:
    with ToolchainProfiler.profile_block('compile inputs'):
      if use_cxx:
        clang_compiler = CXX
      else:
        clang_compiler = CC

      def is_link_flag(flag):
        if flag.startswith('-nostdlib'):
          return True
        return flag.startswith(('-l', '-L', '-Wl,'))

      compile_args = [a for a in newargs if a and not is_link_flag(a)]

      # For asm.js, the generated JavaScript could preserve LLVM value names, which can be useful for debugging.
      if shared.Settings.DEBUG_LEVEL >= 3 and not shared.Settings.WASM and not shared.Settings.WASM_BACKEND:
        cflags.append('-fno-discard-value-names')

      if not shared.Building.can_inline():
        cflags.append('-fno-inline-functions')

      # For fastcomp backend, no LLVM IR functions should ever be annotated
      # 'optnone', because that would skip running the SimplifyCFG pass on
      # them, which is required to always run to clean up LandingPadInst
      # instructions that are not needed.
      if not shared.Settings.WASM_BACKEND:
        cflags += ['-Xclang', '-disable-O0-optnone']

      # Precompiled headers support
      if has_header_inputs:
        headers = [header for _, header in input_files]
        for header in headers:
          if not header.endswith(HEADER_ENDINGS):
            exit_with_error('cannot mix precompile headers with non-header inputs: ' + str(headers) + ' : ' + header)
        args = cflags + compile_args + headers
        if specified_target:
          args += ['-o', specified_target]
        args = system_libs.process_args(args, shared.Settings)
        logger.debug("running (for precompiled headers): " + clang_compiler + ' ' + ' '.join(args))
        return run_process([clang_compiler] + args, check=False).returncode

      def get_clang_command(input_files):
        args = [clang_compiler] + cflags + compile_args + input_files
        return system_libs.process_args(args, shared.Settings)

      def get_clang_command_asm(input_files):
        asflags = shared.get_asmflags(compile_args)
        return [clang_compiler] + asflags + compile_args + input_files

      # preprocessor-only (-E) support
      if has_dash_E or '-M' in newargs or '-MM' in newargs or '-fsyntax-only' in newargs:
        input_files = [x[1] for x in input_files]
        cmd = get_clang_command(input_files)
        if specified_target:
          cmd += ['-o', specified_target]
        # Do not compile, but just output the result from preprocessing stage or
        # output the dependency rule. Warning: clang and gcc behave differently
        # with -MF! (clang seems to not recognize it)
        logger.debug(('just preprocessor ' if has_dash_E else 'just dependencies: ') + ' '.join(cmd))
        return run_process(cmd, check=False).returncode

      def get_object_filename(input_file):
        if compile_only and len(input_files) == 1:
          # no need for a temp file, just emit to the right place
          if specified_target:
            return specified_target
          else:
            return unsuffixed_basename(input_files[0][1]) + options.default_object_extension
        else:
          return in_temp(unsuffixed(uniquename(input_file)) + options.default_object_extension)

      def compile_source_file(i, input_file):
        logger.debug('compiling source file: ' + input_file)
        output_file = get_object_filename(input_file)
        temp_files.append((i, output_file))
        if get_file_suffix(input_file) in ASSEMBLY_ENDINGS:
          cmd = get_clang_command_asm([input_file])
        else:
          cmd = get_clang_command([input_file])
        cmd += ['-c', '-o', output_file]
        if shared.Settings.WASM_BACKEND and shared.Settings.RELOCATABLE:
          cmd.append('-fPIC')
          cmd.append('-fvisibility=default')
        if shared.Settings.WASM_BACKEND:
          if shared.Settings.LTO:
            cmd.append('-flto=' + shared.Settings.LTO)
          else:
            # With fastcomp (or with LTO mode) these args get passed instead
            # at link time when the backend runs.
            for a in shared.Building.llvm_backend_args():
              cmd += ['-mllvm', a]
        else:
          cmd.append('-emit-llvm')
        shared.print_compiler_stage(cmd)
        shared.check_call(cmd)
        if output_file != '-':
          assert(os.path.exists(output_file))

      # First, generate LLVM bitcode. For each input file, we get base.o with bitcode
      for i, input_file in input_files:
        file_suffix = get_file_suffix(input_file)
        if file_suffix in SOURCE_ENDINGS or (has_dash_c and file_suffix == '.bc'):
          compile_source_file(i, input_file)
        elif file_suffix in OBJECT_FILE_ENDINGS:
          logger.debug('using object file: ' + input_file)
          temp_files.append((i, input_file))
        elif file_suffix in DYNAMICLIB_ENDINGS:
          logger.debug('using shared library: ' + input_file)
          temp_files.append((i, input_file))
        elif shared.Building.is_ar(input_file):
          logger.debug('using static library: ' + input_file)
          ensure_archive_index(input_file)
          temp_files.append((i, input_file))
        elif file_suffix in ASSEMBLY_ENDINGS:
          temp_file = in_temp(unsuffixed(uniquename(input_file)) + '.o')
          if file_suffix == '.ll':
            logger.debug('assembling assembly file: ' + input_file)
            shared.Building.llvm_as(input_file, temp_file)
            temp_files.append((i, temp_file))
          else:
            if not shared.Settings.WASM_BACKEND:
              exit_with_error('assembly files not supported by fastcomp')
            compile_source_file(i, input_file)
        elif has_fixed_language_mode:
          compile_source_file(i, input_file)
        elif input_file == '-':
          exit_with_error('-E or -x required when input is from standard input')
        else:
          exit_with_error(input_file + ': unknown input file suffix')

    # exit block 'compile inputs'
    log_time('compile inputs')

    with ToolchainProfiler.profile_block('process inputs'):
      if not shared.Settings.WASM_BACKEND:
        assert len(temp_files) == len(input_files)

        # Optimize source files
        if optimizing(options.llvm_opts):
          for pos, (_, input_file) in enumerate(input_files):
            file_suffix = get_file_suffix(input_file)
            if file_suffix in SOURCE_ENDINGS:
              temp_file = temp_files[pos][1]
              logger.debug('optimizing %s', input_file)
              new_temp_file = in_temp(unsuffixed(uniquename(temp_file)) + '.o')
              # after optimizing, lower intrinsics to libc calls so that our linking code
              # will find them (otherwise, llvm.cos.f32() will not link in cosf(), and
              # we end up calling out to JS for Math.cos).
              opts = options.llvm_opts + ['-lower-non-em-intrinsics']
              shared.Building.llvm_opt(temp_file, opts, new_temp_file)
              temp_files[pos] = (temp_files[pos][0], new_temp_file)

      # If we were just compiling stop here
      if compile_only:
        if not specified_target:
          assert len(temp_files) == len(input_files)
          for tempf, inputf in zip(temp_files, input_files):
            safe_move(tempf[1], unsuffixed_basename(inputf[1]) + final_suffix)
        else:
          # Specifying -o with multiple input source files is not allowed.
          # We error out much earlier in this case.
          assert len(input_files) == 1
          input_file = input_files[0][1]
          temp_file = temp_files[0][1]
          if specified_target != '-':
            if temp_file != input_file:
              safe_move(temp_file, specified_target)
            else:
              shutil.copyfile(temp_file, specified_target)
          temp_output_base = unsuffixed(temp_file)
          if os.path.exists(temp_output_base + '.d'):
            # There was a .d file generated, from -MD or -MMD and friends, save a copy of it to where the output resides,
            # adjusting the target name away from the temporary file name to the specified target.
            # It will be deleted with the rest of the temporary directory.
            deps = open(temp_output_base + '.d').read()
            deps = deps.replace(temp_output_base + options.default_object_extension, specified_target)
            with open(os.path.join(os.path.dirname(specified_target), os.path.basename(unsuffixed(input_file) + '.d')), "w") as out_dep:
              out_dep.write(deps)

    # exit block 'process inputs'
    log_time('process inputs')

    if compile_only:
      logger.debug('stopping after compile phase')
      for flag in link_flags:
        diagnostics.warning('unused-command-line-argument', "argument unused during compilation: '%s'" % flag[1])
      return 0

    if specified_target and specified_target.startswith('-'):
      exit_with_error('invalid output filename: `%s`' % specified_target)

    using_lld = shared.Settings.WASM_BACKEND and not (link_to_object and shared.Settings.LTO)
    link_flags = filter_link_flags(link_flags, using_lld)

    # Decide what we will link
    consumed = process_libraries(libs, lib_dirs, temp_files)
    # Filter out libraries that are actually JS libs
    link_flags = [l for l in link_flags if l[0] not in consumed]
    temp_files = filter_out_dynamic_libs(temp_files)

    linker_inputs = [val for _, val in sorted(temp_files + link_flags)]

    if link_to_object:
      with ToolchainProfiler.profile_block('linking to object file'):
        # We have a specified target (-o <target>), which is not JavaScript or HTML, and
        # we have multiple files: Link them
        if shared.Settings.SIDE_MODULE:
          exit_with_error('SIDE_MODULE must only be used when compiling to an executable shared library, and not when emitting an object file.  That is, you should be emitting a .wasm file (for wasm) or a .js file (for asm.js). Note that when compiling to a typical native suffix for a shared library (.so, .dylib, .dll; which many build systems do) then Emscripten emits an object file, which you should then compile to .wasm or .js with SIDE_MODULE.')
        if final_suffix.lower() in ('.so', '.dylib', '.dll'):
          diagnostics.warning('emcc', 'When Emscripten compiles to a typical native suffix for shared libraries (.so, .dylib, .dll) then it emits an object file. You should then compile that to an emscripten SIDE_MODULE (using that flag) with suffix .wasm (for wasm) or .js (for asm.js). (You may also want to adapt your build system to emit the more standard suffix for an object file, \'.bc\' or \'.o\', which would avoid this warning.)')
        logger.debug('link_to_object: ' + str(linker_inputs) + ' -> ' + specified_target)
        if len(temp_files) == 1:
          temp_file = temp_files[0][1]
          # skip running the linker and just copy the object file
          shutil.copyfile(temp_file, specified_target)
        else:
          shared.Building.link_to_object(linker_inputs, specified_target)
        logger.debug('stopping after linking to object file')
        return 0

    ## Continue on to create JavaScript

    with ToolchainProfiler.profile_block('calculate system libraries'):
      # link in ports and system libraries, if necessary
      if not shared.Settings.BOOTSTRAPPING_STRUCT_INFO and \
         not shared.Settings.SIDE_MODULE: # shared libraries/side modules link no C libraries, need them in parent
        extra_files_to_link = system_libs.get_ports(shared.Settings)
        if '-nostdlib' not in newargs and '-nodefaultlibs' not in newargs:
          # TODO(sbc): Only set link_as_cxx if use_cxx
          link_as_cxx = '-nostdlib++' not in newargs
          extra_files_to_link += system_libs.calculate([f for _, f in sorted(temp_files)] + extra_files_to_link, in_temp, link_as_cxx, forced=forced_stdlibs)
        linker_inputs += extra_files_to_link

    # exit block 'calculate system libraries'
    log_time('calculate system libraries')

    def dedup_list(lst):
      rtn = []
      for item in lst:
        if item not in rtn:
          rtn.append(item)
      return rtn

    # Make a final pass over shared.Settings.EXPORTED_FUNCTIONS to remove any
    # duplication between functions added by the driver/libraries and function
    # specified by the user
    shared.Settings.EXPORTED_FUNCTIONS = dedup_list(shared.Settings.EXPORTED_FUNCTIONS)

    with ToolchainProfiler.profile_block('link'):
      # final will be an array if linking is deferred, otherwise a normal string.
      if shared.Settings.WASM_BACKEND:
        DEFAULT_FINAL = in_temp(target_basename + '.wasm')
      else:
        DEFAULT_FINAL = in_temp(target_basename + '.bc')

      def get_final():
        global final
        if isinstance(final, list):
          final = DEFAULT_FINAL
        return final

      # First, combine the bitcode files if there are several. We must also link if we have a singleton .a
      perform_link = len(linker_inputs) > 1 or shared.Settings.WASM_BACKEND
      if not perform_link:
        is_bc = suffix(temp_files[0][1]) in OBJECT_FILE_ENDINGS
        is_dylib = suffix(temp_files[0][1]) in DYNAMICLIB_ENDINGS
        is_ar = shared.Building.is_ar(temp_files[0][1])
        perform_link = not (is_bc or is_dylib) and is_ar
      if perform_link:
        logger.debug('linking: ' + str(linker_inputs))
        # force archive contents to all be included, if just archives, or if linking shared modules
        force_archive_contents = all(t.endswith(STATICLIB_ENDINGS) for _, t in temp_files) or shared.Settings.LINKABLE

        # if  EMCC_DEBUG=2  then we must link now, so the temp files are complete.
        # if using the wasm backend, we might be using vanilla LLVM, which does not allow our
        # fastcomp deferred linking opts.
        # TODO: we could check if this is a fastcomp build, and still speed things up here
        just_calculate = DEBUG != 2 and not shared.Settings.WASM_BACKEND
        if shared.Settings.WASM_BACKEND:
          js_funcs = None
          if shared.Settings.LLD_REPORT_UNDEFINED and shared.Settings.ERROR_ON_UNDEFINED_SYMBOLS:
            js_funcs = get_all_js_syms(misc_temp_files)
            log_time('JS symbol generation')
          final = shared.Building.link_lld(linker_inputs, DEFAULT_FINAL, external_symbol_list=js_funcs)
          # Special handling for when the user passed '-Wl,--version'.  In this case the linker
          # does not create the output file, but just prints its version and exits with 0.
          if '--version' in linker_inputs:
            return 0
        else:
          final = shared.Building.link(linker_inputs, DEFAULT_FINAL, force_archive_contents=force_archive_contents, just_calculate=just_calculate)
      else:
        logger.debug('skipping linking: ' + str(linker_inputs))
        temp_file = temp_files[0][1]
        input_file = input_files[0][1]
        final = in_temp(target_basename + '.bc')
        if temp_file != input_file:
          shutil.move(temp_file, final)
        else:
          shutil.copyfile(temp_file, final)

    # exit block 'link'
    log_time('link')

    if not shared.Settings.WASM_BACKEND:
      with ToolchainProfiler.profile_block('post-link'):
        if DEBUG:
          logger.debug('saving intermediate processing steps to %s', shared.get_emscripten_temp_dir())
          save_intermediate('basebc', 'bc')

        # Optimize, if asked to
        # remove LLVM debug if we are not asked for it
        link_opts = []
        if not need_llvm_debug_info(options):
          link_opts += ['-strip-debug']
        if not shared.Settings.ASSERTIONS:
          link_opts += ['-disable-verify']
        else:
          # when verifying, LLVM debug info has some tricky linking aspects, and llvm-link will
          # disable the type map in that case. we added linking to opt, so we need to do
          # something similar, which we can do with a param to opt
          link_opts += ['-disable-debug-info-type-map']

        if options.llvm_lto is not None and options.llvm_lto >= 2 and optimizing(options.llvm_opts):
          logger.debug('running LLVM opts as pre-LTO')
          final = shared.Building.llvm_opt(final, options.llvm_opts, DEFAULT_FINAL)
          save_intermediate('opt', 'bc')

        # If we can LTO, do it before dce, since it opens up dce opportunities
        if (not shared.Settings.LINKABLE) and options.llvm_lto and options.llvm_lto != 2:
          if not shared.Building.can_inline():
            link_opts.append('-disable-inlining')
          # add a manual internalize with the proper things we need to be kept alive during lto
          link_opts += shared.Building.get_safe_internalize() + ['-std-link-opts']
          # execute it now, so it is done entirely before we get to the stage of legalization etc.
          final = shared.Building.llvm_opt(final, link_opts, DEFAULT_FINAL)
          save_intermediate('lto', 'bc')
          link_opts = []
        else:
          # At minimum remove dead functions etc., this potentially saves a
          # lot in the size of the generated code (and the time to compile it)
          link_opts += shared.Building.get_safe_internalize() + ['-globaldce']

        if options.cfi:
          if use_cxx:
             link_opts.append("-wholeprogramdevirt")
          link_opts.append("-lowertypetests")

        if shared.Settings.AUTODEBUG:
          # let llvm opt directly emit ll, to skip writing and reading all the bitcode
          link_opts += ['-S']
          final = shared.Building.llvm_opt(final, link_opts, get_final() + '.link.ll')
          save_intermediate('linktime', 'll')
        else:
          if len(link_opts) > 0:
            final = shared.Building.llvm_opt(final, link_opts, DEFAULT_FINAL)
            save_intermediate('linktime', 'bc')
          if options.save_bc:
            shutil.copyfile(final, options.save_bc)

        # Prepare .ll for Emscripten
        if options.save_bc:
          save_intermediate('ll', 'll')

        if shared.Settings.AUTODEBUG:
          logger.debug('autodebug')
          next = get_final() + '.ad.ll'
          run_process([shared.PYTHON, shared.AUTODEBUGGER, final, next])
          final = next
          save_intermediate('autodebug', 'll')

        assert not isinstance(final, list), 'we must have linked the final files, if linking was deferred, by this point'

      # exit block 'post-link'
      log_time('post-link')

    if target == DEV_NULL:
      # TODO(sbc): In theory we should really run the whole pipeline even if the output is
      # /dev/null, but that will take some refactoring
      return 0

    with ToolchainProfiler.profile_block('emscript'):
      # Emscripten
      logger.debug('LLVM => JS')
      if options.memory_init_file:
        shared.Settings.MEM_INIT_METHOD = 1
      else:
        assert shared.Settings.MEM_INIT_METHOD != 1

      if embed_memfile(options):
        shared.Settings.SUPPORT_BASE64_EMBEDDING = 1

      final = do_emscripten(final, shared.replace_or_append_suffix(target, '.mem'))
      save_intermediate('original')

      if shared.Settings.WASM_BACKEND:
        # we also received wat and wasm at this stage
        temp_basename = unsuffixed(final)
        wasm_temp = temp_basename + '.wasm'
        shutil.move(wasm_temp, wasm_binary_target)
        if use_source_map(options):
          shutil.move(wasm_temp + '.map', wasm_source_map_target)

      if shared.Settings.CYBERDWARF:
        cd_target = final + '.cd'
        shutil.move(cd_target, shared.replace_or_append_suffix(target, '.cd'))

    # exit block 'emscript'
    log_time('emscript (llvm => executable code)')

    with ToolchainProfiler.profile_block('source transforms'):
      # Embed and preload files
      if len(options.preload_files) or len(options.embed_files):

        # Also, MEMFS is not aware of heap resizing feature in wasm, so if MEMFS and memory growth are used together, force
        # no_heap_copy to be enabled.
        if shared.Settings.ALLOW_MEMORY_GROWTH and not options.no_heap_copy:
          logger.info('Enabling --no-heap-copy because -s ALLOW_MEMORY_GROWTH=1 is being used with file_packager.py (pass --no-heap-copy to suppress this notification)')
          options.no_heap_copy = True

        logger.debug('setting up files')
        file_args = ['--from-emcc', '--export-name=' + shared.Settings.EXPORT_NAME]
        if len(options.preload_files):
          file_args.append('--preload')
          file_args += options.preload_files
        if len(options.embed_files):
          file_args.append('--embed')
          file_args += options.embed_files
        if len(options.exclude_files):
          file_args.append('--exclude')
          file_args += options.exclude_files
        if options.use_preload_cache:
          file_args.append('--use-preload-cache')
        if options.no_heap_copy:
          file_args.append('--no-heap-copy')
        if shared.Settings.LZ4:
          file_args.append('--lz4')
        if options.use_preload_plugins:
          file_args.append('--use-preload-plugins')
        file_code = run_process([shared.PYTHON, shared.FILE_PACKAGER, unsuffixed(target) + '.data'] + file_args, stdout=PIPE).stdout
        options.pre_js = file_code + options.pre_js

      # Apply pre and postjs files
      if options.pre_js or options.post_js:
        logger.debug('applying pre/postjses')
        src = open(final).read()
        final += '.pp.js'
        with open(final, 'w') as f:
          # pre-js code goes right after the Module integration code (so it
          # can use Module), we have a marker for it
          f.write(src.replace('// {{PRE_JSES}}', fix_windows_newlines(options.pre_js)))
          f.write(fix_windows_newlines(options.post_js))
        options.pre_js = src = options.post_js = None
        save_intermediate('pre-post')

      # Apply a source code transformation, if requested
      if options.js_transform:
        shutil.copyfile(final, final + '.tr.js')
        final += '.tr.js'
        posix = not shared.WINDOWS
        logger.debug('applying transform: %s', options.js_transform)
        shared.check_call(shared.Building.remove_quotes(shlex.split(options.js_transform, posix=posix) + [os.path.abspath(final)]))
        save_intermediate('transformed')

      js_transform_tempfiles = [final]

    # exit block 'source transforms'
    log_time('source transforms')

    with ToolchainProfiler.profile_block('memory initializer'):
      memfile = None
      if (not shared.Settings.WASM_BACKEND and (shared.Settings.MEM_INIT_METHOD > 0 or embed_memfile(options))) or \
         (shared.Settings.WASM_BACKEND and not shared.Settings.MEM_INIT_IN_WASM):
         memfile = shared.replace_or_append_suffix(target, '.mem')

      if memfile:
        if shared.Settings.WASM_BACKEND:
          # For the wasm backend, we don't have any memory info in JS. All we need to do
          # is set the memory initializer url.
          src = open(final).read()
          src = src.replace('var memoryInitializer = null;', 'var memoryInitializer = "%s";' % os.path.basename(memfile))
          open(final + '.mem.js', 'w').write(src)
          final += '.mem.js'
        else:
          # Non-wasm backend path: Strip the memory initializer out of the asmjs file
          shared.try_delete(memfile)

          def parse_mem_bytes(s):
            membytes = [int(x or '0') for x in s.split(',')]
            while membytes and membytes[-1] == 0:
              membytes.pop()
            return membytes

          def repl(m):
            # handle chunking of the memory initializer
            s = m.group(1)
            if len(s) == 0:
              return '' # don't emit 0-size ones
            membytes = parse_mem_bytes(s)
            if not membytes:
              return ''
            if shared.Settings.MEM_INIT_METHOD == 2:
              # memory initializer in a string literal
              return "memoryInitializer = '%s';" % shared.JS.generate_string_initializer(membytes)
            open(memfile, 'wb').write(bytearray(membytes))
            if DEBUG:
              # Copy into temp dir as well, so can be run there too
              shared.safe_copy(memfile, os.path.join(shared.get_emscripten_temp_dir(), os.path.basename(memfile)))
            if not shared.Settings.WASM or not shared.Settings.MEM_INIT_IN_WASM:
              return 'memoryInitializer = "%s";' % shared.JS.get_subresource_location(memfile, embed_memfile(options))
            else:
              return ''

          if shared.Settings.MINIMAL_RUNTIME and (shared.Settings.MEM_INIT_METHOD == 0 or (shared.Settings.SINGLE_FILE and not shared.Settings.WASM)):
            # In MINIMAL_RUNTIME emit the base64 memory initializer directly into a HEAPU8.set() statement.
            mem_init_data = re.search(shared.JS.memory_initializer_pattern, open(final).read())
            src = open(final).read().replace('{{{ BASE64_MEMORY_INITIALIZER }}}', base64_encode(bytearray(parse_mem_bytes(mem_init_data.group(1)))))
            src = re.sub(shared.JS.memory_initializer_pattern, '', src)
          else:
            src = re.sub(shared.JS.memory_initializer_pattern, repl, open(final).read(), count=1)
          open(final + '.mem.js', 'w').write(src)
          final += '.mem.js'
          src = None
          js_transform_tempfiles[-1] = final # simple text substitution preserves comment line number mappings
          if os.path.exists(memfile):
            save_intermediate('meminit')
            logger.debug('wrote memory initialization to %s', memfile)
          else:
            logger.debug('did not see memory initialization')

      if shared.Settings.USE_PTHREADS:
        target_dir = os.path.dirname(os.path.abspath(target))
        worker_output = os.path.join(target_dir, shared.Settings.PTHREAD_WORKER_FILE)
        with open(worker_output, 'w') as f:
          f.write(shared.read_and_preprocess(shared.path_from_root('src', 'worker.js'), expand_macros=True))

        # Minify the worker.js file in optimized builds
        if (shared.Settings.OPT_LEVEL >= 1 or shared.Settings.SHRINK_LEVEL >= 1) and not shared.Settings.DEBUG_LEVEL:
          minified_worker = shared.Building.acorn_optimizer(worker_output, ['minifyWhitespace'], return_output=True)
          open(worker_output, 'w').write(minified_worker)

      # Generate the fetch.js worker script for multithreaded emscripten_fetch() support if targeting pthreads.
      if shared.Settings.USE_FETCH_WORKER:
        shared.make_fetch_worker(final, shared.Settings.FETCH_WORKER_FILE)

    # exit block 'memory initializer'
    log_time('memory initializer')

    optimizer = JSOptimizer(
      target=target,
      options=options,
      js_transform_tempfiles=js_transform_tempfiles,
      in_temp=in_temp,
    )
    with ToolchainProfiler.profile_block('js opts'):
      # It is useful to run several js optimizer passes together, to save on unneeded unparsing/reparsing
      if shared.Settings.DEAD_FUNCTIONS:
        optimizer.queue += ['eliminateDeadFuncs']
        optimizer.extra_info['dead_functions'] = shared.Settings.DEAD_FUNCTIONS

      if shared.Settings.OPT_LEVEL >= 1 and options.js_opts:
        logger.debug('running js post-opts')

        if DEBUG == 2:
          # Clean up the syntax a bit
          optimizer.queue += ['noop']

        def get_eliminate():
          if shared.Settings.ALLOW_MEMORY_GROWTH:
            return 'eliminateMemSafe'
          else:
            return 'eliminate'

        if shared.Settings.OPT_LEVEL >= 2:
          optimizer.queue += [get_eliminate()]

          if shared.Settings.AGGRESSIVE_VARIABLE_ELIMINATION:
            # note that this happens before registerize/minification, which can obfuscate the name of 'label', which is tricky
            optimizer.queue += ['aggressiveVariableElimination']

          optimizer.queue += ['simplifyExpressions']

          if shared.Settings.EMTERPRETIFY:
            # emterpreter code will not run through a JS optimizing JIT, do more work ourselves
            optimizer.queue += ['localCSE']

      if shared.Settings.EMTERPRETIFY:
        # add explicit label setting, as we will run aggressiveVariableElimination late, *after* 'label' is no longer notable by name
        optimizer.queue += ['safeLabelSetting']

      if shared.Settings.OPT_LEVEL >= 1 and options.js_opts:
        if shared.Settings.OPT_LEVEL >= 2:
          # simplify ifs if it is ok to make the code somewhat unreadable,
          # with commaified code breaks late aggressive variable elimination)
          # do not do this with binaryen, as commaifying confuses binaryen call type detection (FIXME, in theory, but unimportant)
          debugging = shared.Settings.DEBUG_LEVEL == 0 or options.profiling
          if shared.Settings.SIMPLIFY_IFS and debugging and not shared.Settings.WASM:
            optimizer.queue += ['simplifyIfs']

          if shared.Settings.PRECISE_F32:
            optimizer.queue += ['optimizeFrounds']

      if options.js_opts:
        if shared.Settings.SAFE_HEAP and not shared.Building.is_wasm_only():
          optimizer.queue += ['safeHeap']

        if shared.Settings.OPT_LEVEL >= 2 and shared.Settings.DEBUG_LEVEL < 3:
          if shared.Settings.OPT_LEVEL >= 3 or shared.Settings.SHRINK_LEVEL > 0:
            optimizer.queue += ['registerizeHarder']
          else:
            optimizer.queue += ['registerize']

        # NOTE: Important that this comes after registerize/registerizeHarder
        if shared.Settings.ELIMINATE_DUPLICATE_FUNCTIONS and shared.Settings.OPT_LEVEL >= 2:
          optimizer.flush()
          shared.Building.eliminate_duplicate_funcs(final)
          save_intermediate('dfe')

      if shared.Settings.EVAL_CTORS and options.memory_init_file and not use_source_map(options) and not shared.Settings.WASM:
        optimizer.flush()
        shared.Building.eval_ctors(final, memfile)
        save_intermediate('eval-ctors')

      if options.js_opts:
        # some compilation modes require us to minify later or not at all
        if not shared.Settings.EMTERPRETIFY and not shared.Settings.WASM:
          optimizer.do_minify()

        if shared.Settings.OPT_LEVEL >= 2:
          optimizer.queue += ['asmLastOpts']

        if shared.Settings.FINALIZE_ASM_JS:
          optimizer.queue += ['last']

        optimizer.flush()

      if options.use_closure_compiler == 2 and not shared.Settings.WASM_BACKEND:
        optimizer.flush()

        logger.debug('running closure')
        # no need to add this to js_transform_tempfiles, because closure and
        # debug_level > 0 are never simultaneously true
        final = shared.Building.closure_compiler(final, pretty=shared.Settings.DEBUG_LEVEL >= 1,
                                                 extra_closure_args=options.closure_args)
        save_intermediate('closure')

    log_time('js opts')

    with ToolchainProfiler.profile_block('final emitting'):
      if shared.Settings.EMTERPRETIFY:
        emterpretify(js_target, optimizer, options)

      # Remove some trivial whitespace
      # TODO: do not run when compress has already been done on all parts of the code
      # src = open(final).read()
      # src = re.sub(r'\n+[ \n]*\n+', '\n', src)
      # open(final, 'w').write(src)

      # Bundle symbol data in with the cyberdwarf file
      if shared.Settings.CYBERDWARF:
        run_process([shared.PYTHON, shared.path_from_root('tools', 'emdebug_cd_merger.py'), shared.replace_or_append_suffix(target, '.cd'), shared.replace_or_append_suffix(target, '.symbols')])

      if use_source_map(options) and not shared.Settings.WASM:
        emit_js_source_maps(target, optimizer.js_transform_tempfiles)

      # track files that will need native eols
      generated_text_files_with_native_eols = []

      if (options.separate_asm or shared.Settings.WASM) and not shared.Settings.WASM_BACKEND:
        separate_asm_js(final, asm_target)
        if not shared.Settings.SINGLE_FILE:
          generated_text_files_with_native_eols += [asm_target]

      if shared.Settings.WASM:
        do_binaryen(target, asm_target, options, memfile, wasm_binary_target,
                    wasm_text_target, wasm_source_map_target, misc_temp_files,
                    optimizer)

      if shared.Settings.MODULARIZE:
        modularize()

      module_export_name_substitution()

      # Run a final regex pass to clean up items that were not possible to optimize by Closure, or unoptimalities that were left behind
      # by processing steps that occurred after Closure.
      if shared.Settings.MINIMAL_RUNTIME == 2 and shared.Settings.USE_CLOSURE_COMPILER and shared.Settings.DEBUG_LEVEL == 0 and not shared.Settings.SINGLE_FILE:
        # Process .js runtime file
        shared.run_process([shared.PYTHON, shared.path_from_root('tools', 'hacky_postprocess_around_closure_limitations.py'), final])
        # Process .asm.js file
        if not shared.Settings.WASM and shared.Settings.SEPARATE_ASM:
          shared.run_process([shared.PYTHON, shared.path_from_root('tools', 'hacky_postprocess_around_closure_limitations.py'), asm_target])

      # Apply pre and postjs files
      if options.extern_pre_js or options.extern_post_js:
        logger.debug('applying extern pre/postjses')
        src = open(final).read()
        final += '.epp.js'
        with open(final, 'w') as f:
          f.write(fix_windows_newlines(options.extern_pre_js))
          f.write(src)
          f.write(fix_windows_newlines(options.extern_post_js))
        save_intermediate('extern-pre-post')

      # The JS is now final. Move it to its final location
      shutil.move(final, js_target)

      if not shared.Settings.SINGLE_FILE:
        generated_text_files_with_native_eols += [js_target]

      # If we were asked to also generate HTML, do that
      if final_suffix == '.html':
        generate_html(target, options, js_target, target_basename,
                      asm_target, wasm_binary_target,
                      memfile, optimizer)
      else:
        if options.proxy_to_worker:
          generate_worker_js(target, js_target, target_basename)

      if embed_memfile(options) and memfile:
        shared.try_delete(memfile)

      for f in generated_text_files_with_native_eols:
        tools.line_endings.convert_line_endings_in_file(f, os.linesep, options.output_eol)
    log_time('final emitting')
    # exit block 'final emitting'

  finally:
    if DEBUG:
      shared.Cache.release_cache_lock()

  if DEBUG:
    logger.debug('total time: %.2f seconds', (time.time() - start_time))

  return 0


def parse_args(newargs):
  options = EmccOptions()
  settings_changes = []
  should_exit = False
  eh_enabled = False
  wasm_eh_enabled = False

  for i in range(len(newargs)):
    # On Windows Vista (and possibly others), excessive spaces in the command line
    # leak into the items in this array, so trim e.g. 'foo.cpp ' -> 'foo.cpp'
    newargs[i] = newargs[i].strip()
    arg = newargs[i]

    def check_arg(value):
      if arg.startswith(value) and '=' in arg:
        exit_with_error('Invalid parameter (do not use "=" with "--" options)')
      return arg == value

    def consume_arg():
      # Consume that option and its argument
      if len(newargs) <= i + 1:
        exit_with_error("option '%s' requires an argument" % arg)
      ret = newargs[i + 1]
      newargs[i] = ''
      newargs[i + 1] = ''
      return ret

    if newargs[i].startswith('-O'):
      # Let -O default to -O2, which is what gcc does.
      options.requested_level = newargs[i][2:] or '2'
      if options.requested_level == 's':
        options.llvm_opts = ['-Os']
        options.requested_level = 2
        shared.Settings.SHRINK_LEVEL = 1
        settings_changes.append('INLINING_LIMIT=50')
      elif options.requested_level == 'z':
        options.llvm_opts = ['-Oz']
        options.requested_level = 2
        shared.Settings.SHRINK_LEVEL = 2
        settings_changes.append('INLINING_LIMIT=25')
      shared.Settings.OPT_LEVEL = validate_arg_level(options.requested_level, 3, 'Invalid optimization level: ' + newargs[i], clamp=True)
    elif check_arg('--js-opts'):
      options.js_opts = int(consume_arg())
      if options.js_opts:
        options.force_js_opts = True
    elif check_arg('--llvm-opts'):
      options.llvm_opts = parse_value(consume_arg())
    elif newargs[i].startswith('-flto'):
      if '=' in newargs[i]:
        shared.Settings.LTO = newargs[i].split('=')[1]
      else:
        shared.Settings.LTO = "full"
    elif check_arg('--llvm-lto'):
      if shared.Settings.WASM_BACKEND:
        logger.warning('--llvm-lto ignored when using llvm backend')
      options.llvm_lto = int(consume_arg())
    elif check_arg('--closure-args'):
      args = consume_arg()
      options.closure_args += shlex.split(args)
    elif check_arg('--closure'):
      options.use_closure_compiler = int(consume_arg())
    elif check_arg('--js-transform'):
      options.js_transform = consume_arg()
    elif check_arg('--pre-js'):
      options.pre_js += open(consume_arg()).read() + '\n'
    elif check_arg('--post-js'):
      options.post_js += open(consume_arg()).read() + '\n'
    elif check_arg('--extern-pre-js'):
      options.extern_pre_js += open(consume_arg()).read() + '\n'
    elif check_arg('--extern-post-js'):
      options.extern_post_js += open(consume_arg()).read() + '\n'
    elif check_arg('--minify'):
      arg = consume_arg()
      if arg != '0':
        exit_with_error('0 is the only supported option for --minify; 1 has been deprecated')
      shared.Settings.DEBUG_LEVEL = max(1, shared.Settings.DEBUG_LEVEL)
    elif newargs[i].startswith('-g'):
      options.requested_debug = newargs[i]
      requested_level = newargs[i][2:] or '3'
      if is_int(requested_level):
        # the -gX value is the debug level (-g1, -g2, etc.)
        shared.Settings.DEBUG_LEVEL = validate_arg_level(requested_level, 4, 'Invalid debug level: ' + newargs[i])
        # if we don't need to preserve LLVM debug info, do not keep this flag
        # for clang
        if shared.Settings.DEBUG_LEVEL < 3:
          newargs[i] = ''
        else:
          # for 3+, report -g to clang as -g4 is not accepted
          newargs[i] = '-g'
      else:
        if requested_level.startswith('force_dwarf'):
          exit_with_error('gforce_dwarf was a temporary option and is no longer necessary (use -g)')
        elif requested_level.startswith('separate-dwarf'):
          # emit full DWARF but also emit it in a file on the side
          newargs[i] = '-g'
          # if a file is provided, use that; otherwise use the default location
          # (note that we do not know the default location until all args have
          # been parsed, so just note True for now).
          if requested_level != 'separate-dwarf':
            if not requested_level.startswith('separate-dwarf=') or requested_level.count('=') != 1:
              exit_with_error('invalid -gseparate-dwarf=FILENAME notation')
            shared.Settings.SEPARATE_DWARF = requested_level.split('=')[1]
          else:
            shared.Settings.SEPARATE_DWARF = True
        # a non-integer level can be something like -gline-tables-only. keep
        # the flag for the clang frontend to emit the appropriate DWARF info.
        # set the emscripten debug level to 3 so that we do not remove that
        # debug info during link (during compile, this does not make a
        # difference).
        shared.Settings.DEBUG_LEVEL = 3
    elif newargs[i] == '-profiling' or newargs[i] == '--profiling':
      shared.Settings.DEBUG_LEVEL = max(shared.Settings.DEBUG_LEVEL, 2)
      options.profiling = True
      newargs[i] = ''
    elif newargs[i] == '-profiling-funcs' or newargs[i] == '--profiling-funcs':
      options.profiling_funcs = True
      newargs[i] = ''
    elif newargs[i] == '--tracing' or newargs[i] == '--memoryprofiler':
      if newargs[i] == '--memoryprofiler':
        options.memory_profiler = True
      options.tracing = True
      newargs[i] = ''
      settings_changes.append("EMSCRIPTEN_TRACING=1")
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'library_trace.js')))
    elif newargs[i] == '--emit-symbol-map':
      options.emit_symbol_map = True
      shared.Settings.EMIT_SYMBOL_MAP = 1
      newargs[i] = ''
    elif newargs[i] == '--bind':
      shared.Settings.EMBIND = 1
      newargs[i] = ''
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'embind', 'emval.js')))
      shared.Settings.SYSTEM_JS_LIBRARIES.append((0, shared.path_from_root('src', 'embind', 'embind.js')))
    elif check_arg('--embed-file'):
      options.embed_files.append(consume_arg())
    elif check_arg('--preload-file'):
      options.preload_files.append(consume_arg())
    elif check_arg('--exclude-file'):
      options.exclude_files.append(consume_arg())
    elif newargs[i].startswith('--use-preload-cache'):
      options.use_preload_cache = True
      newargs[i] = ''
    elif newargs[i].startswith('--no-heap-copy'):
      options.no_heap_copy = True
      newargs[i] = ''
    elif newargs[i].startswith('--use-preload-plugins'):
      options.use_preload_plugins = True
      newargs[i] = ''
    elif newargs[i] == '--ignore-dynamic-linking':
      options.ignore_dynamic_linking = True
      newargs[i] = ''
    elif newargs[i] == '-v':
      shared.PRINT_STAGES = True
      shared.check_sanity(force=True)
    elif check_arg('--shell-file'):
      options.shell_path = consume_arg()
    elif check_arg('--source-map-base'):
      options.source_map_base = consume_arg()
    elif newargs[i] == '--no-entry':
      options.no_entry = True
      newargs[i] = ''
    elif check_arg('--js-library'):
      shared.Settings.SYSTEM_JS_LIBRARIES.append((i + 1, os.path.abspath(consume_arg())))
    elif newargs[i] == '--remove-duplicates':
      diagnostics.warning('legacy-settings', '--remove-duplicates is deprecated as it is no longer needed. If you cannot link without it, file a bug with a testcase')
      newargs[i] = ''
    elif newargs[i] == '--jcache':
      logger.error('jcache is no longer supported')
      newargs[i] = ''
    elif newargs[i] == '--clear-cache':
      logger.info('clearing cache as requested by --clear-cache')
      shared.Cache.erase()
      shared.check_sanity(force=True) # this is a good time for a sanity check
      should_exit = True
    elif newargs[i] == '--clear-ports':
      logger.info('clearing ports and cache as requested by --clear-ports')
      system_libs.Ports.erase()
      shared.Cache.erase()
      shared.check_sanity(force=True) # this is a good time for a sanity check
      should_exit = True
    elif newargs[i] == '--show-ports':
      system_libs.show_ports()
      should_exit = True
    elif check_arg('--save-bc'):
      options.save_bc = consume_arg()
    elif check_arg('--memory-init-file'):
      options.memory_init_file = int(consume_arg())
    elif newargs[i] == '--proxy-to-worker':
      options.proxy_to_worker = True
      newargs[i] = ''
    elif newargs[i] == '--valid-abspath':
      options.valid_abspaths.append(newargs[i + 1])
      newargs[i] = ''
      newargs[i + 1] = ''
    elif newargs[i] == '--separate-asm':
      options.separate_asm = True
      newargs[i] = ''
    elif newargs[i].startswith(('-I', '-L')):
      options.path_name = newargs[i][2:]
      if os.path.isabs(options.path_name) and not is_valid_abspath(options, options.path_name):
        # Of course an absolute path to a non-system-specific library or header
        # is fine, and you can ignore this warning. The danger are system headers
        # that are e.g. x86 specific and nonportable. The emscripten bundled
        # headers are modified to be portable, local system ones are generally not.
        diagnostics.warning(
            'absolute-paths', '-I or -L of an absolute path "' + newargs[i] +
            '" encountered. If this is to a local system header/library, it may '
            'cause problems (local system files make sense for compiling natively '
            'on your system, but not necessarily to JavaScript).')
    elif newargs[i] == '--emrun':
      options.emrun = True
      newargs[i] = ''
    elif newargs[i] == '--cpuprofiler':
      options.cpu_profiler = True
      newargs[i] = ''
    elif newargs[i] == '--threadprofiler':
      options.thread_profiler = True
      settings_changes.append('PTHREADS_PROFILING=1')
      newargs[i] = ''
    elif newargs[i] == '-fno-exceptions':
      shared.Settings.DISABLE_EXCEPTION_CATCHING = 1
      shared.Settings.DISABLE_EXCEPTION_THROWING = 1
      shared.Settings.EXCEPTION_HANDLING = 0
    elif newargs[i] == '-fexceptions':
      eh_enabled = True
    elif newargs[i] == '-fwasm-exceptions':
      wasm_eh_enabled = True
    elif newargs[i] == '-fignore-exceptions':
      shared.Settings.DISABLE_EXCEPTION_CATCHING = 1
    elif newargs[i] == '--default-obj-ext':
      newargs[i] = ''
      options.default_object_extension = newargs[i + 1]
      if not options.default_object_extension.startswith('.'):
        options.default_object_extension = '.' + options.default_object_extension
      newargs[i + 1] = ''
    elif newargs[i].startswith("-fsanitize=cfi"):
      options.cfi = True
    elif newargs[i] == "--output_eol":
      if newargs[i + 1].lower() == 'windows':
        options.output_eol = '\r\n'
      elif newargs[i + 1].lower() == 'linux':
        options.output_eol = '\n'
      else:
        exit_with_error('Invalid value "' + newargs[i + 1] + '" to --output_eol!')
      newargs[i] = ''
      newargs[i + 1] = ''
    elif newargs[i] == '--generate-config':
      optarg = newargs[i + 1]
      path = os.path.expanduser(optarg)
      if os.path.exists(path):
        exit_with_error('File ' + optarg + ' passed to --generate-config already exists!')
      else:
        shared.generate_config(optarg)
      should_exit = True
    # Record SIMD setting because it controls whether the autovectorizer runs
    elif newargs[i] == '-msimd128':
      settings_changes.append('SIMD=1')
    elif newargs[i] == '-mno-simd128':
      settings_changes.append('SIMD=0')
    # Record USE_PTHREADS setting because it controls whether --shared-memory is passed to lld
    elif newargs[i] == '-pthread':
      settings_changes.append('USE_PTHREADS=1')
    elif newargs[i] in ('-fno-diagnostics-color', '-fdiagnostics-color=never'):
      colored_logger.disable()
      diagnostics.color_enabled = False
    elif newargs[i] == '-no-canonical-prefixes':
      options.expand_symlinks = False
    elif newargs[i] == '-fno-rtti':
      shared.Settings.USE_RTTI = 0

    # TODO Currently -fexceptions only means Emscripten EH. Switch to wasm
    # exception handling by default when -fexceptions is given when wasm
    # exception handling becomes stable.
    if wasm_eh_enabled:
      shared.Settings.EXCEPTION_HANDLING = 1
      shared.Settings.DISABLE_EXCEPTION_THROWING = 1
      shared.Settings.DISABLE_EXCEPTION_CATCHING = 1
    elif eh_enabled:
      shared.Settings.EXCEPTION_HANDLING = 0
      shared.Settings.DISABLE_EXCEPTION_THROWING = 0
      shared.Settings.DISABLE_EXCEPTION_CATCHING = 0

  if should_exit:
    sys.exit(0)

  newargs = [a for a in newargs if a]
  return options, settings_changes, newargs


def emterpretify(js_target, optimizer, options):
  global final
  optimizer.flush('pre-emterpretify')
  logger.debug('emterpretifying')
  blacklist = shared.Settings.EMTERPRETIFY_BLACKLIST
  whitelist = shared.Settings.EMTERPRETIFY_WHITELIST
  synclist = shared.Settings.EMTERPRETIFY_SYNCLIST
  if type(blacklist) == list:
    blacklist = json.dumps(blacklist)
  if type(whitelist) == list:
    whitelist = json.dumps(whitelist)
  if type(synclist) == list:
    synclist = json.dumps(synclist)

  args = [shared.PYTHON,
          shared.path_from_root('tools', 'emterpretify.py'),
          js_target,
          final + '.em.js',
          blacklist,
          whitelist,
          synclist]
  if shared.Settings.EMTERPRETIFY_ASYNC:
    args += ['ASYNC=1']
  if shared.Settings.EMTERPRETIFY_ADVISE:
    args += ['ADVISE=1']
  if options.profiling or options.profiling_funcs:
    args += ['PROFILING=1']
  if shared.Settings.ASSERTIONS:
    args += ['ASSERTIONS=1']
  if shared.Settings.PRECISE_F32:
    args += ['FROUND=1']
  if shared.Settings.ALLOW_MEMORY_GROWTH:
    args += ['MEMORY_SAFE=1']
  if shared.Settings.EMTERPRETIFY_FILE:
    args += ['FILE="' + shared.Settings.EMTERPRETIFY_FILE + '"']

  try:
    # move temp js to final position, alongside its mem init file
    shutil.move(final, js_target)
    shared.check_call(args)
  finally:
    shared.try_delete(js_target)

  final = final + '.em.js'

  if shared.Settings.EMTERPRETIFY_ADVISE:
    logger.warning('halting compilation due to EMTERPRETIFY_ADVISE')
    sys.exit(0)

  # minify (if requested) after emterpreter processing, and finalize output
  logger.debug('finalizing emterpreted code')
  shared.Settings.FINALIZE_ASM_JS = 1
  if not shared.Settings.WASM:
    optimizer.do_minify()
  optimizer.queue += ['last']
  optimizer.flush('finalizing-emterpreted-code')


def emit_js_source_maps(target, js_transform_tempfiles):
  logger.debug('generating source maps')
  jsrun.run_js_tool(shared.path_from_root('tools', 'source-maps', 'sourcemapper.js'),
                    shared.NODE_JS, js_transform_tempfiles +
                    ['--sourceRoot', os.getcwd(),
                     '--mapFileBaseName', target,
                     '--offset', '0'])


def separate_asm_js(final, asm_target):
  """Separate out the asm.js code, if asked. Or, if necessary for another option"""
  logger.debug('separating asm')
  shared.check_call([shared.PYTHON, shared.path_from_root('tools', 'separate_asm.py'), final, asm_target, final, shared.Settings.SEPARATE_ASM_MODULE_NAME])


def do_binaryen(target, asm_target, options, memfile, wasm_binary_target,
                wasm_text_target, wasm_source_map_target, misc_temp_files,
                optimizer):
  global final
  logger.debug('using binaryen')
  if use_source_map(options) and not shared.Settings.SOURCE_MAP_BASE:
    logger.warning("Wasm source map won't be usable in a browser without --source-map-base")
  binaryen_bin = shared.Building.get_binaryen_bin()
  # whether we need to emit -g (function name debug info) in the final wasm
  debug_info = shared.Settings.DEBUG_LEVEL >= 2 or options.profiling_funcs
  # whether we need to emit -g in the intermediate binaryen invocations (but not necessarily at the very end).
  # this is necessary for emitting a symbol map at the end.
  intermediate_debug_info = bool(debug_info or options.emit_symbol_map or shared.Settings.ASYNCIFY_WHITELIST or shared.Settings.ASYNCIFY_BLACKLIST)
  emit_symbol_map = options.emit_symbol_map or shared.Settings.CYBERDWARF
  # finish compiling to WebAssembly, using asm2wasm, if we didn't already emit WebAssembly directly using the wasm backend.
  if not shared.Settings.WASM_BACKEND:
    if DEBUG:
      # save the asm.js input
      shared.Building.save_intermediate(asm_target, 'asmjs.js')
    cmd = [os.path.join(binaryen_bin, 'asm2wasm'), asm_target, '--total-memory=' + str(shared.Settings.INITIAL_MEMORY)]
    if shared.Settings.BINARYEN_TRAP_MODE not in ('js', 'clamp', 'allow'):
      exit_with_error('invalid BINARYEN_TRAP_MODE value: ' + shared.Settings.BINARYEN_TRAP_MODE + ' (should be js/clamp/allow)')
    cmd += ['--trap-mode=' + shared.Settings.BINARYEN_TRAP_MODE]
    if shared.Settings.BINARYEN_IGNORE_IMPLICIT_TRAPS:
      cmd += ['--ignore-implicit-traps']
    # pass optimization level to asm2wasm (if not optimizing, or which passes we should run was overridden, do not optimize)
    if shared.Settings.OPT_LEVEL > 0:
      cmd.append(shared.Building.opt_level_to_str(shared.Settings.OPT_LEVEL, shared.Settings.SHRINK_LEVEL))
    # import mem init file if it exists, and if we will not be using asm.js as a binaryen method (as it needs the mem init file, of course)
    mem_file_exists = options.memory_init_file and os.path.exists(memfile)
    import_mem_init = mem_file_exists and shared.Settings.MEM_INIT_IN_WASM
    if import_mem_init:
      cmd += ['--mem-init=' + memfile]
      if not shared.Settings.RELOCATABLE:
        cmd += ['--mem-base=' + str(shared.Settings.GLOBAL_BASE)]
    # various options imply that the imported table may not be the exact size as
    # the wasm module's own table segments
    if shared.Settings.RELOCATABLE or shared.Settings.RESERVED_FUNCTION_POINTERS > 0 or shared.Settings.EMULATED_FUNCTION_POINTERS:
      cmd += ['--table-max=-1']
    if shared.Settings.SIDE_MODULE:
      cmd += ['--mem-max=-1']
    elif not shared.Settings.ALLOW_MEMORY_GROWTH:
      cmd += ['--mem-max=' + str(shared.Settings.INITIAL_MEMORY)]
    elif shared.Settings.MAXIMUM_MEMORY >= 0:
      cmd += ['--mem-max=' + str(shared.Settings.MAXIMUM_MEMORY)]
    if shared.Settings.LEGALIZE_JS_FFI != 1:
      cmd += ['--no-legalize-javascript-ffi']
    if shared.Building.is_wasm_only():
      cmd += ['--wasm-only'] # this asm.js is code not intended to run as asm.js, it is only ever going to be wasm, an can contain special fastcomp-wasm support
    if shared.Settings.USE_PTHREADS:
      cmd += ['--enable-threads']
    if intermediate_debug_info:
      cmd += ['-g']
    if emit_symbol_map:
      cmd += ['--symbolmap=' + shared.replace_or_append_suffix(target, '.symbols')]
    # we prefer to emit a binary, as it is more efficient. however, when we
    # want full debug info support (not just function names), then we must
    # emit text (at least until wasm gains support for debug info in binaries)
    target_binary = shared.Settings.DEBUG_LEVEL < 3
    if target_binary:
      cmd += ['-o', wasm_binary_target]
    else:
      cmd += ['-o', wasm_text_target, '-S']
    cmd += shared.Building.get_binaryen_feature_flags()
    logger.debug('asm2wasm (asm.js => WebAssembly): ' + ' '.join(cmd))
    TimeLogger.update()
    shared.check_call(cmd)

    if not target_binary:
      cmd = [os.path.join(binaryen_bin, 'wasm-as'), wasm_text_target, '-o', wasm_binary_target, '--all-features', '--disable-bulk-memory']
      if intermediate_debug_info:
        cmd += ['-g']
        if use_source_map(options):
          cmd += ['--source-map=' + wasm_source_map_target]
          cmd += ['--source-map-url=' + options.source_map_base + os.path.basename(wasm_binary_target) + '.map']
      logger.debug('wasm-as (text => binary): ' + ' '.join(cmd))
      shared.check_call(cmd)
    if import_mem_init:
      # remove the mem init file in later processing; it does not need to be prefetched in the html, etc.
      if DEBUG:
        safe_move(memfile, os.path.join(shared.get_emscripten_temp_dir(), os.path.basename(memfile)))
      else:
        os.unlink(memfile)
    log_time('asm2wasm')

  if options.binaryen_passes:
    if '--post-emscripten' in options.binaryen_passes and not shared.Settings.SIDE_MODULE:
      if shared.Settings.WASM_BACKEND and not shared.Settings.RELOCATABLE:
        # With the wasm backend stack point value is baked in at link time.  However, emscripten's
        # JS compiler can allocate more static data which then shifts the stack pointer.
        # See `makeStaticAlloc` in the JS compiler.
        options.binaryen_passes += ['--pass-arg=stack-pointer@%d' % shared.Settings.STACK_BASE]
      # the value of the sbrk pointer has been computed by the JS compiler, and we can apply it in the wasm
      # (we can't add this value when we placed post-emscripten in the proper position in the list of
      # passes because that was before the value was computed)
      # note that we don't pass this for a side module, as the value can't be applied - it must be
      # imported
      options.binaryen_passes += ['--pass-arg=emscripten-sbrk-ptr@%d' % shared.Settings.DYNAMICTOP_PTR]
      if shared.Settings.STANDALONE_WASM:
        options.binaryen_passes += ['--pass-arg=emscripten-sbrk-val@%d' % shared.Settings.DYNAMIC_BASE]
    shared.Building.save_intermediate(wasm_binary_target, 'pre-byn.wasm')
    args = options.binaryen_passes
    shared.Building.run_wasm_opt(wasm_binary_target,
                                 wasm_binary_target,
                                 args=args,
                                 debug=intermediate_debug_info)

  if shared.Settings.BINARYEN_SCRIPTS:
    binaryen_scripts = os.path.join(shared.BINARYEN_ROOT, 'scripts')
    script_env = os.environ.copy()
    root_dir = os.path.abspath(os.path.dirname(__file__))
    if script_env.get('PYTHONPATH'):
      script_env['PYTHONPATH'] += ':' + root_dir
    else:
      script_env['PYTHONPATH'] = root_dir
    for script in shared.Settings.BINARYEN_SCRIPTS.split(','):
      logger.debug('running binaryen script: ' + script)
      shared.check_call([shared.PYTHON, os.path.join(binaryen_scripts, script), final, wasm_text_target], env=script_env)
  if shared.Settings.EVAL_CTORS:
    shared.Building.save_intermediate(wasm_binary_target, 'pre-ctors.wasm')
    shared.Building.eval_ctors(final, wasm_binary_target, binaryen_bin, debug_info=intermediate_debug_info)

  # after generating the wasm, do some final operations
  if shared.Settings.SIDE_MODULE and not shared.Settings.WASM_BACKEND:
    shared.WebAssembly.add_dylink_section(wasm_binary_target, shared.Settings.RUNTIME_LINKED_LIBS)
    if not DEBUG:
      os.unlink(asm_target) # we don't need the asm.js, it can just confuse

  # after generating the wasm, do some final operations
  if shared.Settings.EMIT_EMSCRIPTEN_METADATA:
    shared.WebAssembly.add_emscripten_metadata(final, wasm_binary_target)

  if shared.Settings.SIDE_MODULE:
    sys.exit(0) # and we are done.

  # pthreads memory growth requires some additional JS fixups
  if shared.Settings.USE_PTHREADS and shared.Settings.ALLOW_MEMORY_GROWTH:
    final = shared.Building.apply_wasm_memory_growth(final)

  # >=2GB heap support requires pointers in JS to be unsigned. rather than
  # require all pointers to be unsigned by default, which increases code size
  # a little, keep them signed, and just unsign them here if we need that.
  if shared.Settings.CAN_ADDRESS_2GB:
    final = shared.Building.use_unsigned_pointers_in_js(final)

  if shared.Settings.USE_ASAN:
    final = shared.Building.instrument_js_for_asan(final)

  if shared.Settings.OPT_LEVEL >= 2 and shared.Settings.DEBUG_LEVEL <= 2:
    # minify the JS
    optimizer.do_minify() # calculate how to minify
    save_intermediate_with_wasm('preclean', wasm_binary_target)
    final = shared.Building.minify_wasm_js(js_file=final,
                                           wasm_file=wasm_binary_target,
                                           expensive_optimizations=will_metadce(options),
                                           minify_whitespace=optimizer.minify_whitespace,
                                           debug_info=intermediate_debug_info)
    save_intermediate_with_wasm('postclean', wasm_binary_target)

  if shared.Settings.ASYNCIFY_LAZY_LOAD_CODE:
    if not shared.Settings.ASYNCIFY:
      exit_with_error('ASYNCIFY_LAZY_LOAD_CODE requires ASYNCIFY')
    shared.Building.asyncify_lazy_load_code(wasm_binary_target, debug=intermediate_debug_info)

  def preprocess_wasm2js_script():
    wasm2js = read_and_preprocess(shared.path_from_root('src', 'wasm2js.js'))
    # We do not currently have a setup to preprocess {{{ }}} settings in user scripts, so manually
    # expand the settings that wasm2js.js actually uses.
    wasm2js = wasm2js.replace('{{{ WASM_PAGE_SIZE }}}', '65536')
    for opt in ['RESERVED_FUNCTION_POINTERS']:
      wasm2js = wasm2js.replace('{{{ %s }}}' % opt, str(shared.Settings.get(opt)))
    for opt in ['WASM_TABLE_SIZE']:
      wasm2js = wasm2js.replace("{{{ getQuoted('%s') }}}" % opt, str(shared.Settings.get(opt)))
    return wasm2js

  def run_closure_compiler(final):
    final = shared.Building.closure_compiler(final, pretty=not optimizer.minify_whitespace,
                                             extra_closure_args=options.closure_args)
    save_intermediate_with_wasm('closure', wasm_binary_target)
    return final

  if options.use_closure_compiler:
    final = run_closure_compiler(final)

  symbols_file = shared.replace_or_append_suffix(target, '.symbols') if options.emit_symbol_map else None

  if shared.Settings.WASM2JS:
    if shared.Settings.WASM == 2:
      wasm2js_template = wasm_binary_target + '.js'
      open(wasm2js_template, 'w').write(preprocess_wasm2js_script())
    else:
      wasm2js_template = final

    wasm2js = shared.Building.wasm2js(wasm2js_template,
                                      wasm_binary_target,
                                      opt_level=shared.Settings.OPT_LEVEL,
                                      minify_whitespace=optimizer.minify_whitespace,
                                      use_closure_compiler=options.use_closure_compiler,
                                      debug_info=intermediate_debug_info,
                                      symbols_file=symbols_file)
    if shared.Settings.WASM == 2:
      shutil.copyfile(wasm2js, wasm2js_template)
      shared.try_delete(wasm2js)

    if shared.Settings.WASM != 2:
      final = wasm2js
      # if we only target JS, we don't need the wasm any more
      shared.try_delete(wasm_binary_target)

    save_intermediate('wasm2js')

  # emit the final symbols, either in the binary or in a symbol map.
  # this will also remove debug info if we only kept it around in the intermediate invocations.
  # note that if we aren't emitting a binary (like in wasm2js) then we don't
  # have anything to do here.
  if options.emit_symbol_map and os.path.exists(wasm_binary_target):
    shared.Building.handle_final_wasm_symbols(wasm_file=wasm_binary_target, symbols_file=symbols_file, debug_info=debug_info)
    save_intermediate_with_wasm('symbolmap', wasm_binary_target)

  if shared.Settings.DEBUG_LEVEL >= 3 and shared.Settings.SEPARATE_DWARF and os.path.exists(wasm_binary_target):
    # if the dwarf filename wasn't provided, use the default target + a suffix
    dwarf_target = shared.Settings.SEPARATE_DWARF
    if dwarf_target is True:
      dwarf_target = wasm_binary_target + '.debug.wasm'
    shared.Building.emit_debug_on_side(wasm_binary_target, dwarf_target)

  # replace placeholder strings with correct subresource locations
  if shared.Settings.SINGLE_FILE:
    js = open(final).read()

    if '{{{ WASM_BINARY_DATA }}}' in js:
      js = js.replace('{{{ WASM_BINARY_DATA }}}', base64_encode(open(wasm_binary_target, 'rb').read()))

    for target, replacement_string, should_embed in (
        (wasm_binary_target,
         shared.FilenameReplacementStrings.WASM_BINARY_FILE,
         True),
        (asm_target,
         shared.FilenameReplacementStrings.ASMJS_CODE_FILE,
         False),
      ):
      if should_embed and os.path.isfile(target):
        js = js.replace(replacement_string, shared.JS.get_subresource_location(target))
      else:
        js = js.replace(replacement_string, '')
      shared.try_delete(target)
    with open(final, 'w') as f:
      f.write(js)


def modularize():
  global final
  logger.debug('Modularizing, assigning to var ' + shared.Settings.EXPORT_NAME)
  src = open(final).read()

  return_value = shared.Settings.EXPORT_NAME + '.ready'
  if not shared.Settings.EXPORT_READY_PROMISE:
    return_value = '{}'

  src = '''
function(%(EXPORT_NAME)s) {
  %(EXPORT_NAME)s = %(EXPORT_NAME)s || {};

%(src)s

  return %(return_value)s
}
''' % {
    'EXPORT_NAME': shared.Settings.EXPORT_NAME,
    'src': src,
    'return_value': return_value
  }

  if shared.Settings.MINIMAL_RUNTIME and not shared.Settings.USE_PTHREADS:
    # Single threaded MINIMAL_RUNTIME programs do not need access to
    # document.currentScript, so a simple export declaration is enough.
    src = 'var %s=%s' % (shared.Settings.EXPORT_NAME, src)
  else:
    script_url_node = ""
    # When MODULARIZE this JS may be executed later,
    # after document.currentScript is gone, so we save it.
    # In EXPORT_ES6 + USE_PTHREADS the 'thread' is actually an ES6 module webworker running in strict mode,
    # so doesn't have access to 'document'. In this case use 'import.meta' instead.
    if shared.Settings.EXPORT_ES6 and shared.Settings.USE_ES6_IMPORT_META:
      script_url = "import.meta.url"
    else:
      script_url = "typeof document !== 'undefined' && document.currentScript ? document.currentScript.src : undefined"
      if shared.Settings.target_environment_may_be('node'):
        script_url_node = "if (typeof __filename !== 'undefined') _scriptDir = _scriptDir || __filename;"
    src = '''
var %(EXPORT_NAME)s = (function() {
  var _scriptDir = %(script_url)s;
  %(script_url_node)s
  return (%(src)s);
})();
''' % {
      'EXPORT_NAME': shared.Settings.EXPORT_NAME,
      'script_url': script_url,
      'script_url_node': script_url_node,
      'src': src
    }

  final = final + '.modular.js'
  with open(final, 'w') as f:
    f.write(src)

    # Export using a UMD style export, or ES6 exports if selected

    if shared.Settings.EXPORT_ES6:
      f.write('''export default %s;''' % shared.Settings.EXPORT_NAME)
    elif not shared.Settings.MINIMAL_RUNTIME:
      f.write('''if (typeof exports === 'object' && typeof module === 'object')
      module.exports = %(EXPORT_NAME)s;
    else if (typeof define === 'function' && define['amd'])
      define([], function() { return %(EXPORT_NAME)s; });
    else if (typeof exports === 'object')
      exports["%(EXPORT_NAME)s"] = %(EXPORT_NAME)s;
    ''' % {
        'EXPORT_NAME': shared.Settings.EXPORT_NAME
      })

  save_intermediate('modularized')


def module_export_name_substitution():
  global final
  logger.debug('Private module export name substitution with ' + shared.Settings.EXPORT_NAME)
  src = open(final).read()
  final = final + '.module_export_name_substitution.js'
  if shared.Settings.MINIMAL_RUNTIME:
    # In MINIMAL_RUNTIME the Module object is always present to provide the .asm.js/.wasm content
    replacement = shared.Settings.EXPORT_NAME
  else:
    replacement = "typeof %(EXPORT_NAME)s !== 'undefined' ? %(EXPORT_NAME)s : {}" % {"EXPORT_NAME": shared.Settings.EXPORT_NAME}
  with open(final, 'w') as f:
    src = re.sub(r'{[\'"]?__EMSCRIPTEN_PRIVATE_MODULE_EXPORT_NAME_SUBSTITUTION__[\'"]?:1}', replacement, src)
    # For Node.js and other shell environments, create an unminified Module object so that
    # loading external .asm.js file that assigns to Module['asm'] works even when Closure is used.
    if shared.Settings.MINIMAL_RUNTIME and (shared.Settings.target_environment_may_be('node') or shared.Settings.target_environment_may_be('shell')):
      src = 'if(typeof Module==="undefined"){var Module={};}' + src
    f.write(src)
  save_intermediate('module_export_name_substitution')


def generate_traditional_runtime_html(target, options, js_target, target_basename,
                                      asm_target, wasm_binary_target,
                                      memfile, optimizer):
  script = ScriptSource()

  shell = read_and_preprocess(options.shell_path)
  assert '{{{ SCRIPT }}}' in shell, 'HTML shell must contain  {{{ SCRIPT }}}  , see src/shell.html for an example'
  base_js_target = os.path.basename(js_target)

  asm_mods = []

  if options.proxy_to_worker:
    proxy_worker_filename = (shared.Settings.PROXY_TO_WORKER_FILENAME or target_basename) + '.js'
    worker_js = worker_js_script(proxy_worker_filename)
    script.inline = ('''
  var filename = '%s';
  if ((',' + window.location.search.substr(1) + ',').indexOf(',noProxy,') < 0) {
    console.log('running code in a web worker');
''' % shared.JS.get_subresource_location(proxy_worker_filename)) + worker_js + '''
  } else {
    // note: no support for code mods (PRECISE_F32==2)
    console.log('running code on the main thread');
    var fileBytes = tryParseAsDataURI(filename);
    var script = document.createElement('script');
    if (fileBytes) {
      script.innerHTML = intArrayToString(fileBytes);
    } else {
      script.src = filename;
    }
    document.body.appendChild(script);
  }
'''
  else:
    # Normal code generation path
    script.src = base_js_target

    asm_mods = client_mods.get_mods(shared.Settings,
                                    minified='minifyNames' in optimizer.queue_history,
                                    separate_asm=options.separate_asm)

  if not shared.Settings.SINGLE_FILE:
    if shared.Settings.EMTERPRETIFY_FILE:
      # We need to load the emterpreter file before anything else, it has to be synchronously ready
      script.un_src()
      script.inline = '''
          var emterpretURL = '%s';
          var emterpretXHR = new XMLHttpRequest();
          emterpretXHR.open('GET', emterpretURL, true);
          emterpretXHR.responseType = 'arraybuffer';
          emterpretXHR.onload = function() {
            if (emterpretXHR.status === 200 || emterpretXHR.status === 0) {
              Module.emterpreterFile = emterpretXHR.response;
            } else {
              var emterpretURLBytes = tryParseAsDataURI(emterpretURL);
              if (emterpretURLBytes) {
                Module.emterpreterFile = emterpretURLBytes.buffer;
              }
            }
%s
          };
          emterpretXHR.send(null);
''' % (shared.JS.get_subresource_location(shared.Settings.EMTERPRETIFY_FILE), script.inline)

    if options.memory_init_file and not shared.Settings.MEM_INIT_IN_WASM:
      # start to load the memory init file in the HTML, in parallel with the JS
      script.un_src()
      script.inline = ('''
          var memoryInitializer = '%s';
          memoryInitializer = Module['locateFile'] ? Module['locateFile'](memoryInitializer, '') : memoryInitializer;
          Module['memoryInitializerRequestURL'] = memoryInitializer;
          var meminitXHR = Module['memoryInitializerRequest'] = new XMLHttpRequest();
          meminitXHR.open('GET', memoryInitializer, true);
          meminitXHR.responseType = 'arraybuffer';
          meminitXHR.send(null);
''' % shared.JS.get_subresource_location(memfile)) + script.inline

    # Download .asm.js if --separate-asm was passed in an asm.js build, or if 'asmjs' is one
    # of the wasm run methods.
    if not options.separate_asm or shared.Settings.WASM:
      if len(asm_mods):
         exit_with_error('no --separate-asm means no client code mods are possible')
    else:
      script.un_src()
      if len(asm_mods) == 0:
        # just load the asm, then load the rest
        script.inline = '''
    var filename = '%s';
    var fileBytes = tryParseAsDataURI(filename);
    var script = document.createElement('script');
    if (fileBytes) {
      script.innerHTML = intArrayToString(fileBytes);
    } else {
      script.src = filename;
    }
    script.onload = function() {
      setTimeout(function() {
        %s
      }, 1); // delaying even 1ms is enough to allow compilation memory to be reclaimed
    };
    document.body.appendChild(script);
''' % (shared.JS.get_subresource_location(asm_target), script.inline)
      else:
        # may need to modify the asm code, load it as text, modify, and load asynchronously
        script.inline = '''
    var codeURL = '%s';
    var codeXHR = new XMLHttpRequest();
    codeXHR.open('GET', codeURL, true);
    codeXHR.onload = function() {
      var code;
      if (codeXHR.status === 200 || codeXHR.status === 0) {
        code = codeXHR.responseText;
      } else {
        var codeURLBytes = tryParseAsDataURI(codeURL);
        if (codeURLBytes) {
          code = intArrayToString(codeURLBytes);
        }
      }
      %s
      var blob = new Blob([code], { type: 'text/javascript' });
      codeXHR = null;
      var src = URL.createObjectURL(blob);
      var script = document.createElement('script');
      script.src = src;
      script.onload = function() {
        setTimeout(function() {
          %s
        }, 1); // delaying even 1ms is enough to allow compilation memory to be reclaimed
        URL.revokeObjectURL(script.src);
      };
      document.body.appendChild(script);
    };
    codeXHR.send(null);
''' % (shared.JS.get_subresource_location(asm_target), '\n'.join(asm_mods), script.inline)

    if shared.Settings.WASM and not shared.Settings.WASM_ASYNC_COMPILATION:
      # We need to load the wasm file before anything else, it has to be synchronously ready TODO: optimize
      script.un_src()
      script.inline = '''
          var wasmURL = '%s';
          var wasmXHR = new XMLHttpRequest();
          wasmXHR.open('GET', wasmURL, true);
          wasmXHR.responseType = 'arraybuffer';
          wasmXHR.onload = function() {
            if (wasmXHR.status === 200 || wasmXHR.status === 0) {
              Module.wasmBinary = wasmXHR.response;
            } else {
              var wasmURLBytes = tryParseAsDataURI(wasmURL);
              if (wasmURLBytes) {
                Module.wasmBinary = wasmURLBytes.buffer;
              }
            }
%s
          };
          wasmXHR.send(null);
''' % (shared.JS.get_subresource_location(wasm_binary_target), script.inline)

    if shared.Settings.WASM == 2:
      # If target browser does not support WebAssembly, we need to load the .wasm.js file before the main .js file.
      script.un_src()
      script.inline = '''
          function loadMainJs() {
%s
          }
          if (!window.WebAssembly) {
            // Current browser does not support WebAssembly, load the .wasm.js JavaScript fallback
            // before the main JS runtime.
            var wasm2js = document.createElement('script');
            wasm2js.src = '%s';
            wasm2js.onload = loadMainJs;
            document.body.appendChild(wasm2js);
          } else {
            // Current browser supports Wasm, proceed with loading the main JS runtime.
            loadMainJs();
          }
''' % (script.inline, shared.JS.get_subresource_location(wasm_binary_target) + '.js')

  # when script.inline isn't empty, add required helper functions such as tryParseAsDataURI
  if script.inline:
    for filename in ('arrayUtils.js', 'base64Utils.js', 'URIUtils.js'):
      content = read_and_preprocess(shared.path_from_root('src', filename))
      script.inline = content + script.inline

    script.inline = 'var ASSERTIONS = %s;\n%s' % (shared.Settings.ASSERTIONS, script.inline)

  # inline script for SINGLE_FILE output
  if shared.Settings.SINGLE_FILE:
    js_contents = script.inline or ''
    if script.src:
      js_contents += open(js_target).read()
    shared.try_delete(js_target)
    script.src = None
    script.inline = js_contents

  html_contents = shell.replace('{{{ SCRIPT }}}', script.replacement())
  html_contents = tools.line_endings.convert_line_endings(html_contents, '\n', options.output_eol)
  with open(target, 'wb') as f:
    f.write(asbytes(html_contents))


def minify_html(filename, options):
  opts = []
  # -g1 and greater retain whitespace and comments in source
  if shared.Settings.DEBUG_LEVEL == 0:
    opts += ['--collapse-whitespace',
             '--collapse-inline-tag-whitespace',
             '--remove-comments',
             '--remove-tag-whitespace',
             '--sort-attributes',
             '--sort-class-name']
  # -g2 and greater do not minify HTML at all
  if shared.Settings.DEBUG_LEVEL <= 1:
    opts += ['--decode-entities',
             '--collapse-boolean-attributes',
             '--remove-attribute-quotes',
             '--remove-redundant-attributes',
             '--remove-script-type-attributes',
             '--remove-style-link-type-attributes',
             '--use-short-doctype',
             '--minify-css', 'true',
             '--minify-js', 'true']

  # html-minifier also has the following options, but they look unsafe for use:
  # '--remove-optional-tags': removes e.g. <head></head> and <body></body> tags from the page.
  #                           (Breaks at least browser.test_sdl2glshader)
  # '--remove-empty-attributes': removes all attributes with whitespace-only values.
  #                              (Breaks at least browser.test_asmfs_hello_file)
  # '--remove-empty-elements': removes all elements with empty contents.
  #                            (Breaks at least browser.test_asm_swapping)

  if shared.Settings.DEBUG_LEVEL >= 2:
    return

  logger.debug('minifying HTML file ' + filename)
  size_before = os.path.getsize(filename)
  start_time = time.time()
  try:
    run_process([shared.path_from_root('node_modules', '.bin', 'html-minifier-terser' + ('.cmd' if WINDOWS else '')), filename, '-o', filename] + opts, env=shared.env_with_node_in_path())
  except OSError:
    exit_with_error('html-minifier-terser was not found! Please run "npm install" in Emscripten root directory to set up npm dependencies!')

  elapsed_time = time.time() - start_time
  size_after = os.path.getsize(filename)
  delta = size_after - size_before
  logger.debug('HTML minification took {:.2f}'.format(elapsed_time) + ' seconds, and shrunk size of ' + filename + ' from ' + str(size_before) + ' to ' + str(size_after) + ' bytes, delta=' + str(delta) + ' ({:+.2f}%)'.format(delta * 100.0 / size_before))


def generate_html(target, options, js_target, target_basename,
                  asm_target, wasm_binary_target,
                  memfile, optimizer):
  logger.debug('generating HTML')

  if shared.Settings.EXPORT_NAME != 'Module' and \
     not shared.Settings.MINIMAL_RUNTIME and \
     options.shell_path == shared.path_from_root('src', 'shell.html'):
    # the minimal runtime shell HTML is designed to support changing the export
    # name, but the normal one does not support that currently
    exit_with_error('Customizing EXPORT_NAME requires that the HTML be customized to use that name (see https://github.com/emscripten-core/emscripten/issues/10086)')

  if shared.Settings.MINIMAL_RUNTIME:
    generate_minimal_runtime_html(target, options, js_target, target_basename, asm_target,
                                  wasm_binary_target, memfile, optimizer)
  else:
    generate_traditional_runtime_html(target, options, js_target, target_basename, asm_target,
                                      wasm_binary_target, memfile, optimizer)

  if shared.Settings.MINIFY_HTML and (shared.Settings.OPT_LEVEL >= 1 or shared.Settings.SHRINK_LEVEL >= 1):
    minify_html(target, options)


def generate_worker_js(target, js_target, target_basename):
  # compiler output is embedded as base64
  if shared.Settings.SINGLE_FILE:
    proxy_worker_filename = shared.JS.get_subresource_location(js_target)

  # compiler output goes in .worker.js file
  else:
    shutil.move(js_target, unsuffixed(js_target) + '.worker.js')
    worker_target_basename = target_basename + '.worker'
    proxy_worker_filename = (shared.Settings.PROXY_TO_WORKER_FILENAME or worker_target_basename) + '.js'

  target_contents = worker_js_script(proxy_worker_filename)
  open(target, 'w').write(target_contents)


def worker_js_script(proxy_worker_filename):
  web_gl_client_src = open(shared.path_from_root('src', 'webGLClient.js')).read()
  idb_store_src = open(shared.path_from_root('src', 'IDBStore.js')).read()
  proxy_client_src = (
    open(shared.path_from_root('src', 'proxyClient.js')).read()
    .replace('{{{ filename }}}', proxy_worker_filename)
    .replace('{{{ IDBStore.js }}}', idb_store_src)
  )

  return web_gl_client_src + '\n' + proxy_client_src


def process_libraries(libs, lib_dirs, temp_files):
  libraries = []
  consumed = []

  # Find library files
  for i, lib in libs:
    logger.debug('looking for library "%s"', lib)
    suffixes = list(STATICLIB_ENDINGS + DYNAMICLIB_ENDINGS)
    if not shared.Settings.WASM_BACKEND:
      suffixes.append('.bc')

    found = False
    for prefix in LIB_PREFIXES:
      for suff in suffixes:
        name = prefix + lib + suff
        for lib_dir in lib_dirs:
          path = os.path.join(lib_dir, name)
          if os.path.exists(path):
            logger.debug('found library "%s" at %s', lib, path)
            temp_files.append((i, path))
            consumed.append(i)
            found = True
            break
        if found:
          break
      if found:
        break
    if not found:
      jslibs = shared.Building.path_to_system_js_libraries(lib)
      if jslibs:
        libraries += [(i, jslib) for jslib in jslibs]
        consumed.append(i)

  shared.Settings.SYSTEM_JS_LIBRARIES += libraries

  # At this point processing SYSTEM_JS_LIBRARIES is finished, no more items will be added to it.
  # Sort the input list from (order, lib_name) pairs to a flat array in the right order.
  shared.Settings.SYSTEM_JS_LIBRARIES.sort(key=lambda lib: lib[0])
  shared.Settings.SYSTEM_JS_LIBRARIES = [lib[1] for lib in shared.Settings.SYSTEM_JS_LIBRARIES]
  return consumed


class ScriptSource(object):
  def __init__(self):
    self.src = None # if set, we have a script to load with a src attribute
    self.inline = None # if set, we have the contents of a script to write inline in a script

  def un_src(self):
    """Use this if you want to modify the script and need it to be inline."""
    if self.src is None:
      return
    self.inline = '''
          var script = document.createElement('script');
          script.src = "%s";
          document.body.appendChild(script);
''' % self.src
    self.src = None

  def replacement(self):
    """Returns the script tag to replace the {{{ SCRIPT }}} tag in the target"""
    assert (self.src or self.inline) and not (self.src and self.inline)
    if self.src:
      return '<script async type="text/javascript" src="%s"></script>' % quote(self.src)
    else:
      return '<script>\n%s\n</script>' % self.inline


def is_valid_abspath(options, path_name):
  # Any path that is underneath the emscripten repository root must be ok.
  if shared.path_from_root().replace('\\', '/') in path_name.replace('\\', '/'):
    return True

  def in_directory(root, child):
    # make both path absolute
    root = os.path.realpath(root)
    child = os.path.realpath(child)

    # return true, if the common prefix of both is equal to directory
    # e.g. /a/b/c/d.rst and directory is /a/b, the common prefix is /a/b
    return os.path.commonprefix([root, child]) == root

  for valid_abspath in options.valid_abspaths:
    if in_directory(valid_abspath, path_name):
      return True
  return False


def parse_value(text):
  # Note that using response files can introduce whitespace, if the file
  # has a newline at the end. For that reason, we rstrip() in relevant
  # places here.
  def parse_string_value(text):
    first = text[0]
    if first == "'" or first == '"':
      text = text.rstrip()
      assert text[-1] == text[0] and len(text) > 1, 'unclosed opened quoted string. expected final character to be "%s" and length to be greater than 1 in "%s"' % (text[0], text)
      return text[1:-1]
    return text

  def parse_string_list_members(text):
    sep = ','
    values = text.split(sep)
    result = []
    index = 0
    while True:
      current = values[index].lstrip() # Cannot safely rstrip for cases like: "HERE-> ,"
      if not len(current):
        exit_with_error('string array should not contain an empty value')
      first = current[0]
      if not(first == "'" or first == '"'):
        result.append(current.rstrip())
      else:
        start = index
        while True: # Continue until closing quote found
          if index >= len(values):
            exit_with_error("unclosed quoted string. expected final character to be '%s' in '%s'" % (first, values[start]))
          new = values[index].rstrip()
          if new and new[-1] == first:
            if start == index:
              result.append(current.rstrip()[1:-1])
            else:
              result.append((current + sep + new)[1:-1])
            break
          else:
            current += sep + values[index]
            index += 1

      index += 1
      if index >= len(values):
        break
    return result

  def parse_string_list(text):
    text = text.rstrip()
    if text[-1] != ']':
      exit_with_error('unclosed opened string list. expected final character to be "]" in "%s"' % (text))
    inner = text[1:-1]
    if inner.strip() == "":
      return []
    return parse_string_list_members(inner)

  if text[0] == '[':
    # if json parsing fails, we fall back to our own parser, which can handle a few
    # simpler syntaxes
    try:
      return json.loads(text)
    except ValueError:
      return parse_string_list(text)

  try:
    return int(text)
  except ValueError:
    return parse_string_value(text)


def validate_arg_level(level_string, max_level, err_msg, clamp=False):
  try:
    level = int(level_string)
  except ValueError:
    raise Exception(err_msg)
  if clamp:
    if level > max_level:
      logger.warning("optimization level '-O" + level_string + "' is not supported; using '-O" + str(max_level) + "' instead")
      level = max_level
  if not 0 <= level <= max_level:
    raise Exception(err_msg)
  return level


def is_int(s):
  try:
    int(s)
    return True
  except ValueError:
    return False


if __name__ == '__main__':
  try:
    sys.exit(run(sys.argv))
  except KeyboardInterrupt:
    logger.warning('KeyboardInterrupt')
    sys.exit(1)
