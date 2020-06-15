# Copyright 2011 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

from __future__ import print_function

from subprocess import PIPE, STDOUT
import atexit
import base64
import difflib
import json
import logging
import math
import multiprocessing
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

if sys.version_info < (2, 7, 0):
  print('emscripten requires python 2.7.0 or above (python 2.7.12 or newer is recommended, older python versions are known to run into SSL related issues, https://github.com/emscripten-core/emscripten/issues/6275)', file=sys.stderr)
  sys.exit(1)

if sys.version_info[0] == 3 and sys.version_info < (3, 5):
  print('emscripten requires at least python 3.5 (or python 2.7.12 or above)', file=sys.stderr)
  sys.exit(1)

from .toolchain_profiler import ToolchainProfiler
from .tempfiles import try_delete
from . import jsrun, cache, tempfiles, colored_logger
from . import response_file
from . import diagnostics


__rootpath__ = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
WINDOWS = sys.platform.startswith('win')
MACOS = sys.platform == 'darwin'
LINUX = sys.platform.startswith('linux')
DEBUG = int(os.environ.get('EMCC_DEBUG', '0'))
EXPECTED_NODE_VERSION = (4, 1, 1)
EXPECTED_BINARYEN_VERSION = 93


# can add  %(asctime)s  to see timestamps
logging.basicConfig(format='%(name)s:%(levelname)s: %(message)s',
                    level=logging.DEBUG if DEBUG else logging.INFO)
colored_logger.enable()
logger = logging.getLogger('shared')

if sys.version_info < (2, 7, 12):
  logger.debug('python versions older than 2.7.12 are known to run into outdated SSL certificate related issues, https://github.com/emscripten-core/emscripten/issues/6275')

# warning about absolute-paths is disabled by default, and not enabled by -Wall
diagnostics.add_warning('absolute-paths', enabled=False, part_of_all=False)
diagnostics.add_warning('separate-asm')
diagnostics.add_warning('almost-asm')
diagnostics.add_warning('invalid-input')
# Don't show legacy settings warnings by default
diagnostics.add_warning('legacy-settings', enabled=False, part_of_all=False)
# Catch-all for other emcc warnings
diagnostics.add_warning('linkflags')
diagnostics.add_warning('emcc')
diagnostics.add_warning('undefined')
diagnostics.add_warning('version-check')
diagnostics.add_warning('emterpreter')
diagnostics.add_warning('unused-command-line-argument', shared=True)


def exit_with_error(msg, *args):
  diagnostics.error(msg, *args)


# On Windows python suffers from a particularly nasty bug if python is spawning
# new processes while python itself is spawned from some other non-console
# process.
# Use a custom replacement for Popen on Windows to avoid the "WindowsError:
# [Error 6] The handle is invalid" errors when emcc is driven through cmake or
# mingw32-make.
# See http://bugs.python.org/issue3905
class WindowsPopen(object):
  def __init__(self, args, bufsize=0, executable=None, stdin=None, stdout=None, stderr=None, preexec_fn=None, close_fds=False,
               shell=False, cwd=None, env=None, universal_newlines=False, startupinfo=None, creationflags=0):
    self.stdin = stdin
    self.stdout = stdout
    self.stderr = stderr

    # (stdin, stdout, stderr) store what the caller originally wanted to be done with the streams.
    # (stdin_, stdout_, stderr_) will store the fixed set of streams that workaround the bug.
    self.stdin_ = stdin
    self.stdout_ = stdout
    self.stderr_ = stderr

    # If the caller wants one of these PIPEd, we must PIPE them all to avoid the 'handle is invalid' bug.
    if self.stdin_ == PIPE or self.stdout_ == PIPE or self.stderr_ == PIPE:
      if self.stdin_ is None:
        self.stdin_ = PIPE
      if self.stdout_ is None:
        self.stdout_ = PIPE
      if self.stderr_ is None:
        self.stderr_ = PIPE

    try:
      # Call the process with fixed streams.
      self.process = subprocess.Popen(args, bufsize, executable, self.stdin_, self.stdout_, self.stderr_, preexec_fn, close_fds, shell, cwd, env, universal_newlines, startupinfo, creationflags)
      self.pid = self.process.pid
    except Exception as e:
      logger.error('\nsubprocess.Popen(args=%s) failed! Exception %s\n' % (' '.join(args), str(e)))
      raise

  def communicate(self, input=None):
    output = self.process.communicate(input)
    self.returncode = self.process.returncode

    # If caller never wanted to PIPE stdout or stderr, route the output back to screen to avoid swallowing output.
    if self.stdout is None and self.stdout_ == PIPE and len(output[0].strip()):
      print(output[0], file=sys.stdout)
    if self.stderr is None and self.stderr_ == PIPE and len(output[1].strip()):
      print(output[1], file=sys.stderr)

    # Return a mock object to the caller. This works as long as all emscripten code immediately .communicate()s the result, and doesn't
    # leave the process object around for longer/more exotic uses.
    if self.stdout is None and self.stderr is None:
      return (None, None)
    if self.stdout is None:
      return (None, output[1])
    if self.stderr is None:
      return (output[0], None)
    return (output[0], output[1])

  def poll(self):
    return self.process.poll()

  def kill(self):
    return self.process.kill()


def path_from_root(*pathelems):
  return os.path.join(__rootpath__, *pathelems)


# This is a workaround for https://bugs.python.org/issue9400
class Py2CalledProcessError(subprocess.CalledProcessError):
  def __init__(self, returncode, cmd, output=None, stderr=None):
    super(Exception, self).__init__(returncode, cmd, output, stderr)
    self.returncode = returncode
    self.cmd = cmd
    self.output = output
    self.stderr = stderr


# https://docs.python.org/3/library/subprocess.html#subprocess.CompletedProcess
class Py2CompletedProcess:
  def __init__(self, args, returncode, stdout, stderr):
    self.args = args
    self.returncode = returncode
    self.stdout = stdout
    self.stderr = stderr

  def __repr__(self):
    _repr = ['args=%s' % repr(self.args), 'returncode=%s' % self.returncode]
    if self.stdout is not None:
      _repr.append('stdout=' + repr(self.stdout))
    if self.stderr is not None:
      _repr.append('stderr=' + repr(self.stderr))
    return 'CompletedProcess(%s)' % ', '.join(_repr)

  def check_returncode(self):
    if self.returncode != 0:
      raise Py2CalledProcessError(returncode=self.returncode, cmd=self.args, output=self.stdout, stderr=self.stderr)


def run_process(cmd, check=True, input=None, *args, **kw):
  """Runs a subpocess returning the exit code.

  By default this function will raise an exception on failure.  Therefor this should only be
  used if you want to handle such failures.  For most subprocesses, failures are not recoverable
  and should be fatal.  In those cases the `check_call` wrapper should be preferred.
  """

  kw.setdefault('universal_newlines', True)

  debug_text = '%sexecuted %s' % ('successfully ' if check else '', ' '.join(cmd))

  if hasattr(subprocess, "run"):
    ret = subprocess.run(cmd, check=check, input=input, *args, **kw)
    logger.debug(debug_text)
    return ret

  # Python 2 compatibility: Introduce Python 3 subprocess.run-like behavior
  if input is not None:
    kw['stdin'] = subprocess.PIPE
  proc = Popen(cmd, *args, **kw)
  stdout, stderr = proc.communicate(input)
  result = Py2CompletedProcess(cmd, proc.returncode, stdout, stderr)
  if check:
    result.check_returncode()
  logger.debug(debug_text)
  return result


def check_call(cmd, *args, **kw):
  """Like `run_process` above but treat failures as fatal and exit_with_error."""
  try:
    return run_process(cmd, *args, **kw)
  except subprocess.CalledProcessError as e:
    exit_with_error("'%s' failed (%d)", ' '.join(cmd), e.returncode)
  except OSError as e:
    exit_with_error("'%s' failed: %s", ' '.join(cmd), str(e))


def generate_config(path, first_time=False):
  # Note: repr is used to ensure the paths are escaped correctly on Windows.
  # The full string is replaced so that the template stays valid Python.
  config_file = open(path_from_root('tools', 'settings_template.py')).read().splitlines()
  config_file = config_file[3:] # remove the initial comment
  config_file = '\n'.join(config_file)
  # autodetect some default paths
  config_file = config_file.replace('\'{{{ EMSCRIPTEN_ROOT }}}\'', repr(EMSCRIPTEN_ROOT))
  llvm_root = os.path.dirname(Building.which('llvm-dis') or '/usr/bin/llvm-dis')
  config_file = config_file.replace('\'{{{ LLVM_ROOT }}}\'', repr(llvm_root))

  node = Building.which('nodejs') or Building.which('node') or 'node'
  config_file = config_file.replace('\'{{{ NODE }}}\'', repr(node))

  abspath = os.path.abspath(os.path.expanduser(path))
  # write
  with open(abspath, 'w') as f:
    f.write(config_file)

  if first_time:
    print('''
==============================================================================
Welcome to Emscripten!

This is the first time any of the Emscripten tools has been run.

A settings file has been copied to %s, at absolute path: %s

It contains our best guesses for the important paths, which are:

  LLVM_ROOT       = %s
  NODE_JS         = %s
  EMSCRIPTEN_ROOT = %s

Please edit the file if any of those are incorrect.

This command will now exit. When you are done editing those paths, re-run it.
==============================================================================
''' % (path, abspath, llvm_root, node, EMSCRIPTEN_ROOT), file=sys.stderr)


def parse_config_file():
  """Parse the emscripten config file using python's exec.

  Also also EM_<KEY> environment variables to override specific config keys.
  """
  global JS_ENGINES, JAVA, CLOSURE_COMPILER

  config = {}
  config_text = open(CONFIG_FILE, 'r').read() if CONFIG_FILE else EM_CONFIG
  try:
    exec(config_text, config)
  except Exception as e:
    exit_with_error('Error in evaluating %s (at %s): %s, text: %s', EM_CONFIG, CONFIG_FILE, str(e), config_text)

  CONFIG_KEYS = (
    'NODE_JS',
    'BINARYEN_ROOT',
    'POPEN_WORKAROUND',
    'SPIDERMONKEY_ENGINE',
    'EMSCRIPTEN_NATIVE_OPTIMIZER',
    'V8_ENGINE',
    'LLVM_ROOT',
    'LLVM_ADD_VERSION',
    'CLANG_ADD_VERSION',
    'CLOSURE_COMPILER',
    'JAVA',
    'JS_ENGINES',
    'WASMER',
    'WASMTIME',
    'WASM_ENGINES',
    'FROZEN_CACHE',
    'CACHE',
  )

  # Only propagate certain settings from the config file.
  for key in CONFIG_KEYS:
    env_var = 'EM_' + key
    env_value = os.environ.get(env_var)
    if env_value is not None:
      globals()[key] = env_value
    elif key in config:
      globals()[key] = config[key]

  # Certain keys are mandatory
  for key in ('LLVM_ROOT', 'NODE_JS', 'BINARYEN_ROOT'):
    if key not in config:
      exit_with_error('%s is not defined in %s', key, hint_config_file_location())
    if not globals()[key]:
      exit_with_error('%s is set to empty value in %s', key, hint_config_file_location())

  if not NODE_JS:
    exit_with_error('NODE_JS is not defined in %s', hint_config_file_location())

  # EM_CONFIG stuff
  if not JS_ENGINES:
    JS_ENGINES = [NODE_JS]

  if CLOSURE_COMPILER is None:
    if WINDOWS:
      CLOSURE_COMPILER = [path_from_root('node_modules', '.bin', 'google-closure-compiler.cmd')]
    else:
      # Work around an issue that Closure compiler can take up a lot of memory and crash in an error
      # "FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory"
      CLOSURE_COMPILER = [NODE_JS, '--max_old_space_size=8192', path_from_root('node_modules', '.bin', 'google-closure-compiler')]

  if JAVA is None:
    logger.debug('JAVA not defined in ' + hint_config_file_location() + ', using "java"')
    JAVA = 'java'


# Returns a suggestion where current .emscripten config file might be located
# (if EM_CONFIG env. var is used without a file, this hints to "default"
# location at ~/.emscripten)
def hint_config_file_location():
  if CONFIG_FILE:
    return CONFIG_FILE
  else:
    return '~/.emscripten'


def listify(x):
  if type(x) is not list:
    return [x]
  return x


def fix_js_engine(old, new):
  if old is None:
    return
  global JS_ENGINES
  JS_ENGINES = [new if x == old else x for x in JS_ENGINES]
  return new


def expected_llvm_version():
  if get_llvm_target() == WASM_TARGET:
    return "11.0"
  else:
    return "6.0"


def get_clang_version():
  if not hasattr(get_clang_version, 'found_version'):
    if not os.path.exists(CLANG_CC):
      exit_with_error('clang executable not found at `%s`' % CLANG_CC)
    proc = check_call([CLANG_CC, '--version'], stdout=PIPE)
    m = re.search(r'[Vv]ersion\s+(\d+\.\d+)', proc.stdout)
    get_clang_version.found_version = m and m.group(1)
  return get_clang_version.found_version


def check_llvm_version():
  expected = expected_llvm_version()
  actual = get_clang_version()
  if expected in actual:
    return True
  diagnostics.warning('version-check', 'LLVM version appears incorrect (seeing "%s", expected "%s")', actual, expected)
  return False


def get_llc_targets():
  if not os.path.exists(LLVM_COMPILER):
    exit_with_error('llc executable not found at `%s`' % LLVM_COMPILER)
  try:
    llc_version_info = run_process([LLVM_COMPILER, '--version'], stdout=PIPE).stdout
  except subprocess.CalledProcessError:
    exit_with_error('error running `llc --version`.  Check your llvm installation (%s)' % LLVM_COMPILER)
  if 'Registered Targets:' not in llc_version_info:
    exit_with_error('error parsing output of `llc --version`.  Check your llvm installation (%s)' % LLVM_COMPILER)
  pre, targets = llc_version_info.split('Registered Targets:')
  return targets


def has_asm_js_target(targets):
  return 'js' in targets and 'JavaScript (asm.js, emscripten) backend' in targets


def has_wasm_target(targets):
  return 'wasm32' in targets and 'WebAssembly 32-bit' in targets


def check_llvm():
  targets = get_llc_targets()
  if not Settings.WASM_BACKEND:
    if not has_asm_js_target(targets):
      logger.critical('fastcomp in use, but LLVM has not been built with the JavaScript backend as a target, llc reports:')
      print('===========================================================================', file=sys.stderr)
      print(targets, file=sys.stderr)
      print('===========================================================================', file=sys.stderr)
      return False
  else:
    if not has_wasm_target(targets):
      logger.critical('WebAssembly set as target, but LLVM has not been built with the WebAssembly backend, llc reports:')
      print('===========================================================================', file=sys.stderr)
      print(targets, file=sys.stderr)
      print('===========================================================================', file=sys.stderr)
      return False

  return True


def get_node_directory():
  return os.path.dirname(NODE_JS[0] if type(NODE_JS) is list else NODE_JS)


# When we run some tools from npm (closure, html-minifier-terser), those
# expect that the tools have node.js accessible in PATH. Place our node
# there when invoking those tools.
def env_with_node_in_path():
  env = os.environ.copy()
  env['PATH'] = get_node_directory() + os.pathsep + env['PATH']
  return env


def check_node_version():
  jsrun.check_engine(NODE_JS)
  try:
    actual = run_process(NODE_JS + ['--version'], stdout=PIPE).stdout.strip()
    version = tuple(map(int, actual.replace('v', '').replace('-pre', '').split('.')))
    if version >= EXPECTED_NODE_VERSION:
      return True
    diagnostics.warning('version-check', 'node version appears too old (seeing "%s", expected "%s")', actual, 'v' + ('.'.join(map(str, EXPECTED_NODE_VERSION))))
    return False
  except Exception as e:
    diagnostics.warning('version-check', 'cannot check node version: %s', e)
    return False


def set_version_globals():
  global EMSCRIPTEN_VERSION, EMSCRIPTEN_VERSION_MAJOR, EMSCRIPTEN_VERSION_MINOR, EMSCRIPTEN_VERSION_TINY
  filename = path_from_root('emscripten-version.txt')
  with open(filename) as f:
    EMSCRIPTEN_VERSION = f.read().strip().replace('"', '')
  parts = [int(x) for x in EMSCRIPTEN_VERSION.split('.')]
  EMSCRIPTEN_VERSION_MAJOR, EMSCRIPTEN_VERSION_MINOR, EMSCRIPTEN_VERSION_TINY = parts


def generate_sanity():
  return EMSCRIPTEN_VERSION + '|' + LLVM_ROOT + '|' + get_clang_version() + ('_wasm' if Settings.WASM_BACKEND else '')


def perform_sanify_checks():
  logger.info('(Emscripten: Running sanity checks)')

  with ToolchainProfiler.profile_block('sanity compiler_engine'):
    if not jsrun.check_engine(NODE_JS):
      exit_with_error('The configured node executable (%s) does not seem to work, check the paths in %s', NODE_JS, EM_CONFIG)

  with ToolchainProfiler.profile_block('sanity LLVM'):
    for cmd in [CLANG_CC, LLVM_AR, LLVM_AS, LLVM_NM]:
      if not os.path.exists(cmd) and not os.path.exists(cmd + '.exe'):  # .exe extension required for Windows
        exit_with_error('Cannot find %s, check the paths in %s', cmd, EM_CONFIG)


def check_sanity(force=False):
  """Check that basic stuff we need (a JS engine to compile, Node.js, and Clang
  and LLVM) exists.

  The test runner always does this check (through |force|). emcc does this less
  frequently, only when ${EM_CONFIG}_sanity does not exist or is older than
  EM_CONFIG (so, we re-check sanity when the settings are changed).  We also
  re-check sanity and clear the cache when the version changes.
  """
  if not force and os.environ.get('EMCC_SKIP_SANITY_CHECK') == '1':
    return
  # We set EMCC_SKIP_SANITY_CHECK so that any subprocesses that we launch will
  # not re-run the tests.
  os.environ['EMCC_SKIP_SANITY_CHECK'] = '1'
  with ToolchainProfiler.profile_block('sanity'):
    check_llvm_version()
    expected = generate_sanity()
    reason = None
    if not CONFIG_FILE:
      return # config stored directly in EM_CONFIG => skip sanity checks

    settings_mtime = os.path.getmtime(CONFIG_FILE)
    sanity_file = CONFIG_FILE + '_sanity'
    if Settings.WASM_BACKEND:
      sanity_file += '_wasm'
    if os.path.exists(sanity_file):
      sanity_mtime = os.path.getmtime(sanity_file)
      if sanity_mtime < settings_mtime:
        reason = 'settings file has changed'
      else:
        sanity_data = open(sanity_file).read().rstrip()
        if sanity_data != expected:
          reason = 'system change: %s vs %s' % (expected, sanity_data)
        elif not force:
          return # all is well

    if reason:
      if FROZEN_CACHE:
        logger.info('(Emscripten: %s, cache may need to be cleared, but FROZEN_CACHE is set)' % reason)
      else:
        logger.info('(Emscripten: %s, clearing cache)' % reason)
        Cache.erase()
        # the check actually failed, so definitely write out the sanity file, to
        # avoid others later seeing failures too
        force = False

    # some warning, mostly not fatal checks - do them even if EM_IGNORE_SANITY is on
    check_node_version()

    llvm_ok = check_llvm()

    if os.environ.get('EM_IGNORE_SANITY'):
      logger.info('EM_IGNORE_SANITY set, ignoring sanity checks')
      return

    if not llvm_ok:
      exit_with_error('failing sanity checks due to previous llvm failure')

    perform_sanify_checks()

    if not force:
      # Only create/update this file if the sanity check succeeded, i.e., we got here
      with open(sanity_file, 'w') as f:
        f.write(expected)


# Some distributions ship with multiple llvm versions so they add
# the version to the binaries, cope with that
def build_llvm_tool_path(tool):
  if LLVM_ADD_VERSION:
    return os.path.join(LLVM_ROOT, tool + "-" + LLVM_ADD_VERSION)
  else:
    return os.path.join(LLVM_ROOT, tool)


# Some distributions ship with multiple clang versions so they add
# the version to the binaries, cope with that
def build_clang_tool_path(tool):
  if CLANG_ADD_VERSION:
    return os.path.join(LLVM_ROOT, tool + "-" + CLANG_ADD_VERSION)
  else:
    return os.path.join(LLVM_ROOT, tool)


def exe_suffix(cmd):
  return cmd + '.exe' if WINDOWS else cmd


def replace_suffix(filename, new_suffix):
  assert new_suffix[0] == '.'
  return os.path.splitext(filename)[0] + new_suffix


# In MINIMAL_RUNTIME mode, keep suffixes of generated files simple ('.mem' instead of '.js.mem'; .'symbols' instead of '.js.symbols' etc)
# Retain the original naming scheme in traditional runtime.
def replace_or_append_suffix(filename, new_suffix):
  assert new_suffix[0] == '.'
  return replace_suffix(filename, new_suffix) if Settings.MINIMAL_RUNTIME else filename + new_suffix


# Temp dir. Create a random one, unless EMCC_DEBUG is set, in which case use TEMP_DIR/emscripten_temp
def safe_ensure_dirs(dirname):
  try:
    os.makedirs(dirname)
  except OSError:
    # Python 2 compatibility: makedirs does not support exist_ok parameter
    # Ignore error for already existing dirname as exist_ok does
    if not os.path.isdir(dirname):
      raise


# Returns a path to EMSCRIPTEN_TEMP_DIR, creating one if it didn't exist.
def get_emscripten_temp_dir():
  global configuration, EMSCRIPTEN_TEMP_DIR
  if not EMSCRIPTEN_TEMP_DIR:
    EMSCRIPTEN_TEMP_DIR = tempfile.mkdtemp(prefix='emscripten_temp_', dir=configuration.TEMP_DIR)

    def prepare_to_clean_temp(d):
      def clean_temp():
        try_delete(d)

      atexit.register(clean_temp)
    prepare_to_clean_temp(EMSCRIPTEN_TEMP_DIR) # this global var might change later
  return EMSCRIPTEN_TEMP_DIR


def get_canonical_temp_dir(temp_dir):
  return os.path.join(temp_dir, 'emscripten_temp')


class Configuration(object):
  def __init__(self, environ=os.environ):
    self.EMSCRIPTEN_TEMP_DIR = None

    self.TEMP_DIR = environ.get("EMCC_TEMP_DIR", tempfile.gettempdir())
    if not os.path.isdir(self.TEMP_DIR):
      exit_with_error("The temporary directory `" + self.TEMP_DIR + "` does not exist! Please make sure that the path is correct.")

    self.CANONICAL_TEMP_DIR = get_canonical_temp_dir(self.TEMP_DIR)

    if DEBUG:
      try:
        self.EMSCRIPTEN_TEMP_DIR = self.CANONICAL_TEMP_DIR
        safe_ensure_dirs(self.EMSCRIPTEN_TEMP_DIR)
      except Exception as e:
        exit_with_error(str(e) + 'Could not create canonical temp dir. Check definition of TEMP_DIR in ' + hint_config_file_location())

  def get_temp_files(self):
    return tempfiles.TempFiles(
      tmp=self.TEMP_DIR if not DEBUG else get_emscripten_temp_dir(),
      save_debug_files=os.environ.get('EMCC_DEBUG_SAVE'))


def apply_configuration():
  global configuration, EMSCRIPTEN_TEMP_DIR, CANONICAL_TEMP_DIR, TEMP_DIR
  configuration = Configuration()
  EMSCRIPTEN_TEMP_DIR = configuration.EMSCRIPTEN_TEMP_DIR
  CANONICAL_TEMP_DIR = configuration.CANONICAL_TEMP_DIR
  TEMP_DIR = configuration.TEMP_DIR


def check_vanilla():
  global LLVM_TARGET
  # if the env var tells us what to do, do that
  if 'EMCC_WASM_BACKEND' in os.environ:
    if os.environ['EMCC_WASM_BACKEND'] != '0':
      logger.debug('EMCC_WASM_BACKEND tells us to use wasm backend')
      LLVM_TARGET = WASM_TARGET
    else:
      logger.debug('EMCC_WASM_BACKEND tells us to use asm.js backend')
      LLVM_TARGET = ASM_JS_TARGET
  else:
    # if we are using vanilla LLVM, i.e. we don't have our asm.js backend, then we
    # must use wasm (or at least try to). to know that, we have to run llc to
    # see which backends it has. we cache this result.
    temp_cache = cache.Cache(CACHE, use_subdir=False)

    def has_vanilla_targets():
      logger.debug('testing for asm.js target, because if not present (i.e. this is plain vanilla llvm, not emscripten fastcomp), we will use the wasm target instead (set EMCC_WASM_BACKEND to skip this check)')
      targets = get_llc_targets()
      return has_wasm_target(targets) and not has_asm_js_target(targets)

    def get_vanilla_file():
      logger.debug('regenerating vanilla file: %s' % LLVM_ROOT)
      saved_file = os.path.join(temp_cache.dirname, 'is_vanilla.txt')
      if os.path.exists(saved_file):
        logger.debug('old: %s\n' % open(saved_file).read())
      open(saved_file, 'w').write(('1' if has_vanilla_targets() else '0') + ':' + LLVM_ROOT + '\n')
      return saved_file

    is_vanilla_file = temp_cache.get('is_vanilla.txt', get_vanilla_file)
    if CONFIG_FILE and os.path.getmtime(CONFIG_FILE) > os.path.getmtime(is_vanilla_file):
      logger.debug('config file changed since we checked vanilla; re-checking')
      is_vanilla_file = temp_cache.get('is_vanilla.txt', get_vanilla_file, force=True)
    try:
      contents = open(is_vanilla_file).read().strip()
      middle = contents.index(':')
      is_vanilla = int(contents[:middle])
      llvm_used = contents[middle + 1:]
      if llvm_used != LLVM_ROOT:
        logger.debug('regenerating vanilla check since other llvm (%s vs %s)`', llvm_used, LLVM_ROOT)
        temp_cache.get('is_vanilla.txt', get_vanilla_file, force=True)
        is_vanilla = has_vanilla_targets()
    except Exception as e:
      logger.debug('failed to use vanilla file, will re-check: ' + str(e))
      is_vanilla = has_vanilla_targets()
    temp_cache = None
    if is_vanilla:
      logger.debug('check tells us to use wasm backend')
      LLVM_TARGET = WASM_TARGET
    else:
      logger.debug('check tells us to use asm.js backend')
      LLVM_TARGET = ASM_JS_TARGET


def get_llvm_target():
  assert LLVM_TARGET is not None
  return LLVM_TARGET


def emsdk_ldflags(user_args):
  if os.environ.get('EMMAKEN_NO_SDK'):
    return []

  library_paths = [
      path_from_root('system', 'local', 'lib'),
      path_from_root('system', 'lib'),
      Cache.dirname
  ]
  ldflags = ['-L' + l for l in library_paths]

  if '-nostdlib' in user_args:
    return ldflags

  # TODO(sbc): Add system libraries here rather than conditionally including
  # them via .symbols files.
  libraries = []
  ldflags += ['-l' + l for l in libraries]

  return ldflags


def emsdk_cflags():
  # Disable system C and C++ include directories, and add our own (using
  # -isystem so they are last, like system dirs, which allows projects to
  # override them)

  c_opts = ['-Xclang', '-nostdsysteminc']

  c_include_paths = [
    path_from_root('system', 'include', 'compat'),
    path_from_root('system', 'include'),
    path_from_root('system', 'include', 'libc'),
    path_from_root('system', 'lib', 'libc', 'musl', 'arch', 'emscripten'),
    path_from_root('system', 'local', 'include'),
    Cache.get_path('include')
  ]

  cxx_include_paths = [
    path_from_root('system', 'include', 'libcxx'),
    path_from_root('system', 'lib', 'libcxxabi', 'include'),
    path_from_root('system', 'lib', 'libunwind', 'include')
  ]

  def include_directive(paths):
    result = []
    for path in paths:
      result += ['-Xclang', '-isystem' + path]
    return result

  # libcxx include paths must be defined before libc's include paths otherwise libcxx will not build
  if Settings.USE_CXX:
    c_opts += include_directive(cxx_include_paths)
  return c_opts + include_directive(c_include_paths)


def get_asmflags(user_args):
  return ['-target', get_llvm_target()]


def get_cflags(user_args):
  # Set the LIBCPP ABI version to at least 2 so that we get nicely aligned string
  # data and other nice fixes.
  c_opts = [# '-fno-threadsafe-statics', # disabled due to issue 1289
            '-target', get_llvm_target(),
            '-D__EMSCRIPTEN_major__=' + str(EMSCRIPTEN_VERSION_MAJOR),
            '-D__EMSCRIPTEN_minor__=' + str(EMSCRIPTEN_VERSION_MINOR),
            '-D__EMSCRIPTEN_tiny__=' + str(EMSCRIPTEN_VERSION_TINY),
            '-D_LIBCPP_ABI_VERSION=2']

  if get_llvm_target() == WASM_TARGET:
    # wasm target does not automatically define emscripten stuff, so do it here.
    c_opts += ['-Dunix',
               '-D__unix',
               '-D__unix__']

  # Changes to default clang behavior

  # Implicit functions can cause horribly confusing function pointer type errors, see #2175
  # If your codebase really needs them - very unrecommended! - you can disable the error with
  #   -Wno-error=implicit-function-declaration
  # or disable even a warning about it with
  #   -Wno-implicit-function-declaration
  c_opts += ['-Werror=implicit-function-declaration']

  if os.environ.get('EMMAKEN_NO_SDK') or '-nostdinc' in user_args:
    return c_opts

  return c_opts + emsdk_cflags()


# Utilities
def run_js(filename, engine=None, *args, **kw):
  if engine is None:
    engine = JS_ENGINES[0]
  return jsrun.run_js(filename, engine, *args, **kw)


def unique_ordered(values):
  """return a list of unique values in an input list, without changing order
  (list(set(.)) would change order randomly).
  """
  seen = set()

  def check(value):
    if value in seen:
      return False
    seen.add(value)
    return True

  return list(filter(check, values))


def expand_byte_size_suffixes(value):
  """Given a string with KB/MB size suffixes, such as "32MB", computes how
  many bytes that is and returns it as an integer.
  """
  SIZE_SUFFIXES = {suffix: 1024 ** i for i, suffix in enumerate(['b', 'kb', 'mb', 'gb', 'tb'])}
  match = re.match(r'\s*(\d+)\s*([kmgt]?b)$', value, re.I)
  if not match:
    try:
      return int(value)
    except ValueError:
      raise Exception("Invalid byte size, valid suffixes: KB, MB, GB, TB")
  value, suffix = match.groups()
  return int(value) * SIZE_SUFFIXES[suffix.lower()]


# Settings. A global singleton. Not pretty, but nicer than passing |, settings| everywhere
class SettingsManager(object):

  class __impl(object):
    attrs = {}
    internal_settings = set()

    def __init__(self):
      self.reset()

    @classmethod
    def reset(cls):
      cls.attrs = {}

      # Load the JS defaults into python.
      settings = open(path_from_root('src', 'settings.js')).read().replace('//', '#')
      settings = re.sub(r'var ([\w\d]+)', r'attrs["\1"]', settings)
      # Variable TARGET_NOT_SUPPORTED is referenced by value settings.js (also beyond declaring it),
      # so must pass it there explicitly.
      exec(settings, {'attrs': cls.attrs})

      settings = open(path_from_root('src', 'settings_internal.js')).read().replace('//', '#')
      settings = re.sub(r'var ([\w\d]+)', r'attrs["\1"]', settings)
      internal_attrs = {}
      exec(settings, {'attrs': internal_attrs})
      cls.attrs.update(internal_attrs)

      if 'EMCC_STRICT' in os.environ:
        cls.attrs['STRICT'] = int(os.environ.get('EMCC_STRICT'))

      # Special handling for LEGACY_SETTINGS.  See src/setting.js for more
      # details
      cls.legacy_settings = {}
      cls.alt_names = {}
      for legacy in cls.attrs['LEGACY_SETTINGS']:
        if len(legacy) == 2:
          name, new_name = legacy
          cls.legacy_settings[name] = (None, 'setting renamed to ' + new_name)
          cls.alt_names[name] = new_name
          cls.alt_names[new_name] = name
          default_value = cls.attrs[new_name]
        else:
          name, fixed_values, err = legacy
          cls.legacy_settings[name] = (fixed_values, err)
          default_value = fixed_values[0]
        assert name not in cls.attrs, 'legacy setting (%s) cannot also be a regular setting' % name
        if not cls.attrs['STRICT']:
          cls.attrs[name] = default_value

      if get_llvm_target() == WASM_TARGET:
        cls.attrs['WASM_BACKEND'] = 1

      cls.internal_settings = set(internal_attrs.keys())

    # Transforms the Settings information into emcc-compatible args (-s X=Y, etc.). Basically
    # the reverse of load_settings, except for -Ox which is relevant there but not here
    @classmethod
    def serialize(cls):
      ret = []
      for key, value in cls.attrs.items():
        if key == key.upper():  # this is a hack. all of our settings are ALL_CAPS, python internals are not
          jsoned = json.dumps(value, sort_keys=True)
          ret += ['-s', key + '=' + jsoned]
      return ret

    @classmethod
    def to_dict(cls):
      return cls.attrs.copy()

    @classmethod
    def copy(cls, values):
      cls.attrs = values

    @classmethod
    def apply_opt_level(cls, opt_level, shrink_level=0, noisy=False):
      if opt_level >= 1:
        cls.attrs['ASM_JS'] = 1
        cls.attrs['ASSERTIONS'] = 0
        cls.attrs['ALIASING_FUNCTION_POINTERS'] = 1
      if shrink_level >= 2:
        cls.attrs['EVAL_CTORS'] = 1

    def keys(self):
      return self.attrs.keys()

    def __getattr__(self, attr):
      if attr in self.attrs:
        return self.attrs[attr]
      else:
        raise AttributeError("Settings object has no attribute '%s'" % attr)

    def __setattr__(self, attr, value):
      if attr == 'STRICT' and value:
        for a in self.legacy_settings:
          self.attrs.pop(a, None)

      if attr in self.legacy_settings:
        # TODO(sbc): Rather then special case this we should have STRICT turn on the
        # legacy-settings warning below
        if self.attrs['STRICT']:
          exit_with_error('legacy setting used in strict mode: %s', attr)
        fixed_values, error_message = self.legacy_settings[attr]
        if fixed_values and value not in fixed_values:
          exit_with_error('Invalid command line option -s ' + attr + '=' + str(value) + ': ' + error_message)
        diagnostics.warning('legacy-settings', 'use of legacy setting: %s (%s)', attr, error_message)

      if attr in self.alt_names:
        alt_name = self.alt_names[attr]
        self.attrs[alt_name] = value

      if attr not in self.attrs:
        msg = "Attempt to set a non-existent setting: '%s'\n" % attr
        suggestions = ', '.join(difflib.get_close_matches(attr, list(self.attrs.keys())))
        if suggestions:
          msg += ' - did you mean one of %s?\n' % suggestions
        msg += " - perhaps a typo in emcc's  -s X=Y  notation?\n"
        msg += ' - (see src/settings.js for valid values)'
        exit_with_error(msg)

      self.attrs[attr] = value

    @classmethod
    def get(cls, key):
      return cls.attrs.get(key)

    @classmethod
    def __getitem__(cls, key):
      return cls.attrs[key]

    @classmethod
    def target_environment_may_be(self, environment):
      return self.attrs['ENVIRONMENT'] == '' or environment in self.attrs['ENVIRONMENT'].split(',')

  __instance = None

  @staticmethod
  def instance():
    if SettingsManager.__instance is None:
      SettingsManager.__instance = SettingsManager.__impl()
    return SettingsManager.__instance

  def __getattr__(self, attr):
    return getattr(self.instance(), attr)

  def __setattr__(self, attr, value):
    return setattr(self.instance(), attr, value)

  def get(self, key):
    return self.instance().get(key)

  def __getitem__(self, key):
    return self.instance()[key]


def verify_settings():
  if Settings.ASM_JS not in [1, 2]:
    exit_with_error('emcc: ASM_JS can only be set to either 1 or 2')

  if Settings.SAFE_HEAP not in [0, 1]:
    exit_with_error('emcc: SAFE_HEAP must be 0 or 1 in fastcomp')

  if Settings.WASM and Settings.EXPORT_FUNCTION_TABLES:
      exit_with_error('emcc: EXPORT_FUNCTION_TABLES incompatible with WASM')

  if Settings.WASM_BACKEND:
    if not Settings.WASM:
      # When the user requests non-wasm output, we enable wasm2js. that is,
      # we still compile to wasm normally, but we compile the final output
      # to js.
      Settings.WASM = 1
      Settings.WASM2JS = 1
    if Settings.WASM == 2:
      # Requesting both Wasm and Wasm2JS support
      Settings.WASM2JS = 1

    if Settings.CYBERDWARF:
      exit_with_error('emcc: CYBERDWARF is not supported by the LLVM wasm backend')

    if Settings.EMTERPRETIFY:
      exit_with_error('emcc: EMTERPRETIFY is not supported by the LLVM wasm backend')

    if Settings.EMULATED_FUNCTION_POINTERS:
      exit_with_error('emcc: EMULATED_FUNCTION_POINTERS is not meaningful with the wasm backend.')


# llvm-ar appears to just use basenames inside archives. as a result, files with the same basename
# will trample each other when we extract them. to help warn of such situations, we warn if there
# are duplicate entries in the archive
def warn_if_duplicate_entries(archive_contents, archive_filename):
  if len(archive_contents) != len(set(archive_contents)):
    msg = '%s: archive file contains duplicate entries. This is not supported by emscripten. Only the last member with a given name will be linked in which can result in undefined symbols. You should either rename your source files, or use `emar` to create you archives which works around this issue.' % archive_filename
    warned = set()
    for i in range(len(archive_contents)):
      curr = archive_contents[i]
      if curr not in warned and curr in archive_contents[i + 1:]:
        msg += '\n   duplicate: %s' % curr
        warned.add(curr)
    diagnostics.warning('emcc', msg)


# This function creates a temporary directory specified by the 'dir' field in
# the returned dictionary. Caller is responsible for cleaning up those files
# after done.
def extract_archive_contents(archive_file):
  lines = run_process([LLVM_AR, 't', archive_file], stdout=PIPE).stdout.splitlines()
  # ignore empty lines
  contents = [l for l in lines if len(l)]
  if len(contents) == 0:
    logger.debug('Archive %s appears to be empty (recommendation: link an .so instead of .a)' % archive_file)
    return {
      'returncode': 0,
      'dir': None,
      'files': []
    }

  # `ar` files can only contains filenames. Just to be sure,  verify that each
  # file has only as filename component and is not absolute
  for f in contents:
    assert not os.path.dirname(f)
    assert not os.path.isabs(f)

  warn_if_duplicate_entries(contents, archive_file)

  # create temp dir
  temp_dir = tempfile.mkdtemp('_archive_contents', 'emscripten_temp_')

  # extract file in temp dir
  proc = run_process([LLVM_AR, 'xo', archive_file], stdout=PIPE, stderr=STDOUT, cwd=temp_dir)
  abs_contents = [os.path.join(temp_dir, c) for c in contents]

  # check that all files were created
  missing_contents = [x for x in abs_contents if not os.path.exists(x)]
  if missing_contents:
    exit_with_error('llvm-ar failed to extract file(s) ' + str(missing_contents) + ' from archive file ' + f + '! Error:' + str(proc.stdout))

  return {
    'returncode': proc.returncode,
    'dir': temp_dir,
    'files': abs_contents
  }


class ObjectFileInfo(object):
  def __init__(self, returncode, output, defs=set(), undefs=set(), commons=set()):
    self.returncode = returncode
    self.output = output
    self.defs = defs
    self.undefs = undefs
    self.commons = commons

  def is_valid_for_nm(self):
    return self.returncode == 0


# Due to a python pickling issue, the following two functions must be at top
# level, or multiprocessing pool spawn won't find them.
def g_llvm_nm_uncached(filename):
  return Building.llvm_nm_uncached(filename)


def g_multiprocessing_initializer(*args):
  for item in args:
    (key, value) = item.split('=', 1)
    if key == 'EMCC_POOL_CWD':
      os.chdir(value)
    else:
      os.environ[key] = value


def print_compiler_stage(cmd):
  """Emulate the '-v' of clang/gcc by printing the name of the sub-command
  before executing it."""
  if PRINT_STAGES:
    print(' "%s" %s' % (cmd[0], ' '.join(cmd[1:])), file=sys.stderr)
    sys.stderr.flush()


def mangle_c_symbol_name(name):
  return '_' + name if not name.startswith('$') else name[1:]


def demangle_c_symbol_name(name):
  return name[1:] if name.startswith('_') else '$' + name


def is_c_symbol(name):
  return name.startswith('_')


def treat_as_user_function(name):
  if name.startswith('dynCall_'):
    return False
  if name in Settings.WASM_FUNCTIONS_THAT_ARE_NOT_NAME_MANGLED:
    return False
  return True


def asmjs_mangle(name):
  """Mangle a name the way asm.js/JSBackend globals are mangled.

  Prepends '_' and replaces non-alphanumerics with '_'.
  Used by wasm backend for JS library consistency with asm.js.
  """
  if treat_as_user_function(name):
    return '_' + name
  else:
    return name


#  Building
class Building(object):
  multiprocessing_pool = None
  binaryen_checked = False

  # internal caches
  internal_nm_cache = {} # cache results of nm - it can be slow to run
  uninternal_nm_cache = {}
  ar_contents = {} # Stores the object files contained in different archive files passed as input
  _is_ar_cache = {}

  # clear internal caches. this is not normally needed, except if the clang/LLVM
  # used changes inside this invocation of Building, which can happen in the benchmarker
  # when it compares different builds.
  @staticmethod
  def clear():
    Building.internal_nm_cache = {}
    Building.uninternal_nm_cache = {}
    Building.ar_contents = {}
    Building._is_ar_cache = {}

  @staticmethod
  def get_num_cores():
    return int(os.environ.get('EMCC_CORES', multiprocessing.cpu_count()))

  # Multiprocessing pools are very slow to build up and tear down, and having several pools throughout
  # the application has a problem of overallocating child processes. Therefore maintain a single
  # centralized pool that is shared between all pooled task invocations.
  @staticmethod
  def get_multiprocessing_pool():
    if not Building.multiprocessing_pool:
      cores = Building.get_num_cores()
      if DEBUG:
        # When in EMCC_DEBUG mode, only use a single core in the pool, so that
        # logging is not all jumbled up.
        cores = 1

      # If running with one core only, create a mock instance of a pool that does not
      # actually spawn any new subprocesses. Very useful for internal debugging.
      if cores == 1:
        class FakeMultiprocessor(object):
          def map(self, func, tasks, *args, **kwargs):
            results = []
            for t in tasks:
              results += [func(t)]
            return results

          def map_async(self, func, tasks, *args, **kwargs):
            class Result:
              def __init__(self, func, tasks):
                self.func = func
                self.tasks = tasks

              def get(self, timeout):
                results = []
                for t in tasks:
                  results += [func(t)]
                return results

            return Result(func, tasks)

        Building.multiprocessing_pool = FakeMultiprocessor()
      else:
        child_env = [
          # Multiprocessing pool children must have their current working
          # directory set to a safe path that is guaranteed not to die in
          # between of executing commands, or otherwise the pool children will
          # have trouble spawning subprocesses of their own.
          'EMCC_POOL_CWD=' + path_from_root(),
          # Multiprocessing pool children need to avoid all calling
          # check_vanilla() again and again, otherwise the compiler can deadlock
          # when building system libs, because the multiprocess parent can have
          # the Emscripten cache directory locked for write access, and the
          # EMCC_WASM_BACKEND check also requires locked access to the cache,
          # which the multiprocess children would not get.
          'EMCC_WASM_BACKEND=%s' % Settings.WASM_BACKEND,
          # Multiprocessing pool children can't spawn their own linear number of
          # children, that could cause a quadratic amount of spawned processes.
          'EMCC_CORES=1'
        ]
        Building.multiprocessing_pool = multiprocessing.Pool(processes=cores, initializer=g_multiprocessing_initializer, initargs=child_env)

        def close_multiprocessing_pool():
          try:
            # Shut down the pool explicitly, because leaving that for Python to do at process shutdown is buggy and can generate
            # noisy "WindowsError: [Error 5] Access is denied" spam which is not fatal.
            Building.multiprocessing_pool.terminate()
            Building.multiprocessing_pool.join()
            Building.multiprocessing_pool = None
          except OSError as e:
            # Mute the "WindowsError: [Error 5] Access is denied" errors, raise all others through
            if not (sys.platform.startswith('win') and isinstance(e, WindowsError) and e.winerror == 5):
              raise
        atexit.register(close_multiprocessing_pool)

    return Building.multiprocessing_pool

  # When creating environment variables for Makefiles to execute, we need to doublequote the commands if they have spaces in them..
  @staticmethod
  def doublequote_spaces(arg):
    arg = arg[:] # Operate on a copy of the input string/list
    if isinstance(arg, list):
      for i in range(len(arg)):
        arg[i] = Building.doublequote_spaces(arg[i])
      return arg

    if ' ' in arg and (not (arg.startswith('"') and arg.endswith('"'))) and (not (arg.startswith("'") and arg.endswith("'"))):
      return '"' + arg.replace('"', '\\"') + '"'

    return arg

  # .. but for Popen, we cannot have doublequotes, so provide functionality to remove them when needed.
  @staticmethod
  def remove_quotes(arg):
    arg = arg[:] # Operate on a copy of the input string/list
    if isinstance(arg, list):
      for i in range(len(arg)):
        arg[i] = Building.remove_quotes(arg[i])
      return arg

    if arg.startswith('"') and arg.endswith('"'):
      return arg[1:-1].replace('\\"', '"')
    elif arg.startswith("'") and arg.endswith("'"):
      return arg[1:-1].replace("\\'", "'")
    else:
      return arg

  @staticmethod
  def get_building_env(native=False, doublequote_commands=False, cflags=[]):
    def nop(arg):
      return arg
    quote = Building.doublequote_spaces if doublequote_commands else nop
    env = os.environ.copy()
    if native:
      env['CC'] = quote(CLANG_CC)
      env['CXX'] = quote(CLANG_CXX)
      env['LD'] = quote(CLANG_CXX)
      # get a non-native one, and see if we have some of its effects - remove them if so
      non_native = Building.get_building_env(cflags=cflags)
      # the ones that a non-native would modify
      EMSCRIPTEN_MODIFIES = ['LDSHARED', 'AR', 'CROSS_COMPILE', 'NM', 'RANLIB']
      for dangerous in EMSCRIPTEN_MODIFIES:
        if env.get(dangerous) and env.get(dangerous) == non_native.get(dangerous):
          del env[dangerous] # better to delete it than leave it, as the non-native one is definitely wrong
      return env
    # point CC etc. to the em* tools.
    # on windows, we must specify python explicitly. on other platforms, we prefer
    # not to, as some configure scripts expect e.g. CC to be a literal executable
    # (but "python emcc.py" is not a file that exists).
    # note that we point to emcc etc. here, without a suffix, instead of to
    # emcc.py etc.
    env['CC'] = quote(unsuffixed(EMCC)) if not WINDOWS else 'python %s' % quote(EMCC)
    env['CXX'] = quote(unsuffixed(EMXX)) if not WINDOWS else 'python %s' % quote(EMXX)
    env['AR'] = quote(unsuffixed(EMAR)) if not WINDOWS else 'python %s' % quote(EMAR)
    env['LD'] = quote(unsuffixed(EMCC)) if not WINDOWS else 'python %s' % quote(EMCC)
    env['NM'] = quote(LLVM_NM)
    env['LDSHARED'] = quote(unsuffixed(EMCC)) if not WINDOWS else 'python %s' % quote(EMCC)
    env['RANLIB'] = quote(unsuffixed(EMRANLIB)) if not WINDOWS else 'python %s' % quote(EMRANLIB)
    env['EMSCRIPTEN_TOOLS'] = path_from_root('tools')
    if cflags:
      env['CFLAGS'] = env['EMMAKEN_CFLAGS'] = ' '.join(cflags)
    env['HOST_CC'] = quote(CLANG_CC)
    env['HOST_CXX'] = quote(CLANG_CXX)
    env['HOST_CFLAGS'] = "-W" # if set to nothing, CFLAGS is used, which we don't want
    env['HOST_CXXFLAGS'] = "-W" # if set to nothing, CXXFLAGS is used, which we don't want
    env['PKG_CONFIG_LIBDIR'] = path_from_root('system', 'local', 'lib', 'pkgconfig') + os.path.pathsep + path_from_root('system', 'lib', 'pkgconfig')
    env['PKG_CONFIG_PATH'] = os.environ.get('EM_PKG_CONFIG_PATH', '')
    env['EMSCRIPTEN'] = path_from_root()
    env['PATH'] = path_from_root('system', 'bin') + os.pathsep + env['PATH']
    env['CROSS_COMPILE'] = path_from_root('em') # produces /path/to/emscripten/em , which then can have 'cc', 'ar', etc appended to it
    return env

  # Finds the given executable 'program' in PATH. Operates like the Unix tool 'which'.
  @staticmethod
  def which(program):
    def is_exe(fpath):
      return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    if os.path.isabs(program):
      if os.path.isfile(program):
        return program

      if WINDOWS:
        for suffix in ['.exe', '.cmd', '.bat']:
          if is_exe(program + suffix):
            return program + suffix

    fpath, fname = os.path.split(program)
    if fpath:
      if is_exe(program):
        return program
    else:
      for path in os.environ["PATH"].split(os.pathsep):
        path = path.strip('"')
        exe_file = os.path.join(path, program)
        if is_exe(exe_file):
          return exe_file
        if WINDOWS:
          for suffix in ('.exe', '.cmd', '.bat'):
            if is_exe(exe_file + suffix):
              return exe_file + suffix

    return None

  # Returns a clone of the given environment with all directories that contain
  # sh.exe removed from the PATH.  Used to work around CMake limitation with
  # MinGW Makefiles, where sh.exe is not allowed to be present.
  @staticmethod
  def remove_sh_exe_from_path(env):
    env = env.copy()
    if not WINDOWS:
      return env
    path = env['PATH'].split(';')
    path = [p for p in path if not os.path.exists(os.path.join(p, 'sh.exe'))]
    env['PATH'] = ';'.join(path)
    return env

  @staticmethod
  def handle_cmake_toolchain(args, env):
    def has_substr(args, substr):
      return any(substr in s for s in args)

    # Append the Emscripten toolchain file if the user didn't specify one.
    if not has_substr(args, '-DCMAKE_TOOLCHAIN_FILE'):
      args.append('-DCMAKE_TOOLCHAIN_FILE=' + path_from_root('cmake', 'Modules', 'Platform', 'Emscripten.cmake'))
    node_js = NODE_JS

    if not has_substr(args, '-DCMAKE_CROSSCOMPILING_EMULATOR'):
      node_js = NODE_JS[0].replace('"', '\"')
      args.append('-DCMAKE_CROSSCOMPILING_EMULATOR="%s"' % node_js)

    # On Windows specify MinGW Makefiles or ninja if we have them and no other
    # toolchain was specified, to keep CMake from pulling in a native Visual
    # Studio, or Unix Makefiles.
    if WINDOWS and '-G' not in args:
      if Building.which('mingw32-make'):
        args += ['-G', 'MinGW Makefiles']
      elif Building.which('ninja'):
        args += ['-G', 'Ninja']

    # CMake has a requirement that it wants sh.exe off PATH if MinGW Makefiles
    # is being used. This happens quite often, so do this automatically on
    # behalf of the user. See
    # http://www.cmake.org/Wiki/CMake_MinGW_Compiler_Issues
    if WINDOWS and 'MinGW Makefiles' in args:
      env = Building.remove_sh_exe_from_path(env)

    return (args, env)

  @staticmethod
  def configure(args, stdout=None, stderr=None, env=None, cflags=[], **kwargs):
    if env:
      env = env.copy()
    else:
      env = Building.get_building_env(cflags=cflags)
    if 'cmake' in args[0]:
      # Note: EMMAKEN_JUST_CONFIGURE shall not be enabled when configuring with
      #       CMake. This is because CMake does expect to be able to do
      #       config-time builds with emcc.
      args, env = Building.handle_cmake_toolchain(args, env)
    else:
      # When we configure via a ./configure script, don't do config-time
      # compilation with emcc, but instead do builds natively with Clang. This
      # is a heuristic emulation that may or may not work.
      env['EMMAKEN_JUST_CONFIGURE'] = '1'
    if EM_BUILD_VERBOSE >= 2:
      stdout = None
    if EM_BUILD_VERBOSE >= 1:
      stderr = None
    print('configure: ' + ' '.join(args), file=sys.stderr)
    run_process(args, stdout=stdout, stderr=stderr, env=env, **kwargs)

  @staticmethod
  def make(args, stdout=None, stderr=None, env=None, cflags=[], **kwargs):
    if env is None:
      env = Building.get_building_env(cflags=cflags)

    # On Windows prefer building with mingw32-make instead of make, if it exists.
    if WINDOWS:
      if args[0] == 'make':
        mingw32_make = Building.which('mingw32-make')
        if mingw32_make:
          args[0] = mingw32_make

      if 'mingw32-make' in args[0]:
        env = Building.remove_sh_exe_from_path(env)

    # On Windows, run the execution through shell to get PATH expansion and
    # executable extension lookup, e.g. 'sdl2-config' will match with
    # 'sdl2-config.bat' in PATH.
    if EM_BUILD_VERBOSE >= 2:
      stdout = None
    if EM_BUILD_VERBOSE >= 1:
      stderr = None
    print('make: ' + ' '.join(args), file=sys.stderr)
    run_process(args, stdout=stdout, stderr=stderr, env=env, shell=WINDOWS, **kwargs)

  @staticmethod
  def make_paths_absolute(f):
    if f.startswith('-'):  # skip flags
      return f
    else:
      return os.path.abspath(f)

  # Runs llvm-nm in parallel for the given list of files.
  # The results are populated in Building.uninternal_nm_cache
  # multiprocessing_pool: An existing multiprocessing pool to reuse for the operation, or None
  # to have the function allocate its own.
  @staticmethod
  def parallel_llvm_nm(files):
    with ToolchainProfiler.profile_block('parallel_llvm_nm'):
      pool = Building.get_multiprocessing_pool()
      object_contents = pool.map(g_llvm_nm_uncached, files)

      for i, file in enumerate(files):
        if object_contents[i].returncode != 0:
          logger.debug('llvm-nm failed on file ' + file + ': return code ' + str(object_contents[i].returncode) + ', error: ' + object_contents[i].output)
        Building.uninternal_nm_cache[file] = object_contents[i]
      return object_contents

  @staticmethod
  def read_link_inputs(files):
    with ToolchainProfiler.profile_block('read_link_inputs'):
      # Before performing the link, we need to look at each input file to determine which symbols
      # each of them provides. Do this in multiple parallel processes.
      archive_names = [] # .a files passed in to the command line to the link
      object_names = [] # .o/.bc files passed in to the command line to the link
      for f in files:
        absolute_path_f = Building.make_paths_absolute(f)

        if absolute_path_f not in Building.ar_contents and Building.is_ar(absolute_path_f):
          archive_names.append(absolute_path_f)
        elif absolute_path_f not in Building.uninternal_nm_cache and Building.is_bitcode(absolute_path_f):
          object_names.append(absolute_path_f)

      # Archives contain objects, so process all archives first in parallel to obtain the object files in them.
      pool = Building.get_multiprocessing_pool()
      object_names_in_archives = pool.map(extract_archive_contents, archive_names)

      def clean_temporary_archive_contents_directory(directory):
        def clean_at_exit():
          try_delete(directory)
        if directory:
          atexit.register(clean_at_exit)

      for n in range(len(archive_names)):
        if object_names_in_archives[n]['returncode'] != 0:
          raise Exception('llvm-ar failed on archive ' + archive_names[n] + '!')
        Building.ar_contents[archive_names[n]] = object_names_in_archives[n]['files']
        clean_temporary_archive_contents_directory(object_names_in_archives[n]['dir'])

      for o in object_names_in_archives:
        for f in o['files']:
          if f not in Building.uninternal_nm_cache:
            object_names.append(f)

      # Next, extract symbols from all object files (either standalone or inside archives we just extracted)
      # The results are not used here directly, but populated to llvm-nm cache structure.
      Building.parallel_llvm_nm(object_names)

  @staticmethod
  def llvm_backend_args():
    # disable slow and relatively unimportant optimization passes
    args = ['-combiner-global-alias-analysis=false']

    # asm.js-style exception handling
    if Settings.DISABLE_EXCEPTION_CATCHING != 1:
      args += ['-enable-emscripten-cxx-exceptions']
    if Settings.DISABLE_EXCEPTION_CATCHING == 2:
      whitelist = ','.join(Settings.EXCEPTION_CATCHING_WHITELIST or ['__fake'])
      args += ['-emscripten-cxx-exceptions-whitelist=' + whitelist]

    # asm.js-style setjmp/longjmp handling
    args += ['-enable-emscripten-sjlj']

    # better (smaller, sometimes faster) codegen, see binaryen#1054
    # and https://bugs.llvm.org/show_bug.cgi?id=39488
    args += ['-disable-lsr']

    return args

  @staticmethod
  def link_to_object(linker_inputs, target):
    # link using lld for the wasm backend with wasm object files,
    # other otherwise for linking of bitcode we must use our python
    # code (necessary for asm.js, for wasm bitcode see
    # https://bugs.llvm.org/show_bug.cgi?id=40654)
    if Settings.WASM_BACKEND and not Settings.LTO:
      Building.link_lld(linker_inputs + ['--relocatable'], target)
    else:
      Building.link(linker_inputs, target)

  @staticmethod
  def link_llvm(linker_inputs, target):
    # runs llvm-link to link things.
    cmd = [LLVM_LINK] + linker_inputs + ['-o', target]
    cmd = Building.get_command_with_possible_response_file(cmd)
    print_compiler_stage(cmd)
    output = run_process(cmd, stdout=PIPE).stdout
    assert os.path.exists(target) and (output is None or 'Could not open input file' not in output), 'Linking error: ' + output
    return target

  @staticmethod
  def lld_flags_for_executable(external_symbol_list):
    cmd = []
    if external_symbol_list:
      undefs = configuration.get_temp_files().get('.undefined').name
      with open(undefs, 'w') as f:
        f.write('\n'.join(external_symbol_list))
      cmd.append('--allow-undefined-file=%s' % undefs)
    else:
      cmd.append('--allow-undefined')

    # wasi does not import the memory (but for JS it is efficient to do so,
    # as it allows us to set up memory, preload files, etc. even before the
    # wasm module arrives)
    if not Settings.STANDALONE_WASM:
      cmd.append('--import-memory')
      cmd.append('--import-table')

    if Settings.USE_PTHREADS:
      cmd.append('--shared-memory')

    if Settings.DEBUG_LEVEL < 2 and (not Settings.EMIT_SYMBOL_MAP and
                                     not Settings.PROFILING_FUNCS and
                                     not Settings.ASYNCIFY):
      cmd.append('--strip-debug')

    if Settings.RELOCATABLE:
      if Settings.MAIN_MODULE == 2 or Settings.SIDE_MODULE == 2:
        cmd.append('--no-export-dynamic')
      else:
        cmd.append('--no-gc-sections')
        cmd.append('--export-dynamic')

    if Settings.LINKABLE:
      cmd.append('--export-all')
    else:
      c_exports = [e for e in Settings.EXPORTED_FUNCTIONS if is_c_symbol(e)]
      # Strip the leading underscores
      c_exports = [demangle_c_symbol_name(e) for e in c_exports]
      if external_symbol_list:
        # Filter out symbols external/JS symbols
        c_exports = [e for e in c_exports if e not in external_symbol_list]
      for export in c_exports:
        cmd += ['--export', export]

    if Settings.RELOCATABLE:
      if Settings.SIDE_MODULE:
        cmd.append('-shared')
      else:
        cmd.append('-pie')

    if not Settings.SIDE_MODULE:
      cmd += [
        '-z', 'stack-size=%s' % Settings.TOTAL_STACK,
        '--initial-memory=%d' % Settings.INITIAL_MEMORY,
      ]
      use_start_function = Settings.STANDALONE_WASM

      if not use_start_function:
        if Settings.EXPECT_MAIN and not Settings.IGNORE_MISSING_MAIN:
          cmd += ['--entry=main']
        else:
          cmd += ['--no-entry']
      if not Settings.ALLOW_MEMORY_GROWTH:
        cmd.append('--max-memory=%d' % Settings.INITIAL_MEMORY)
      elif Settings.MAXIMUM_MEMORY != -1:
        cmd.append('--max-memory=%d' % Settings.MAXIMUM_MEMORY)
      if not Settings.RELOCATABLE:
        cmd.append('--global-base=%s' % Settings.GLOBAL_BASE)

    return cmd

  @staticmethod
  def link_lld(args, target, external_symbol_list=None):
    if not os.path.exists(WASM_LD):
      exit_with_error('linker binary not found in LLVM directory: %s', WASM_LD)
    # runs lld to link things.
    # lld doesn't currently support --start-group/--end-group since the
    # semantics are more like the windows linker where there is no need for
    # grouping.
    args = [a for a in args if a not in ('--start-group', '--end-group')]

    # Emscripten currently expects linkable output (SIDE_MODULE/MAIN_MODULE) to
    # include all archive contents.
    if Settings.LINKABLE:
      args.insert(0, '--whole-archive')
      args.append('--no-whole-archive')

    if Settings.STRICT:
      args.append('--fatal-warnings')

    cmd = [WASM_LD, '-o', target] + args
    for a in Building.llvm_backend_args():
      cmd += ['-mllvm', a]

    # For relocatable output (generating an object file) we don't pass any of the normal linker
    # flags that are used when building and exectuable
    if '--relocatable' not in args and '-r' not in args:
      cmd += Building.lld_flags_for_executable(external_symbol_list)

    print_compiler_stage(cmd)
    cmd = Building.get_command_with_possible_response_file(cmd)
    check_call(cmd)
    return target

  @staticmethod
  def link(files, target, force_archive_contents=False, just_calculate=False):
    # "Full-featured" linking: looks into archives (duplicates lld functionality)
    actual_files = []
    # Tracking unresolveds is necessary for .a linking, see below.
    # Specify all possible entry points to seed the linking process.
    # For a simple application, this would just be "main".
    unresolved_symbols = set([func[1:] for func in Settings.EXPORTED_FUNCTIONS])
    resolved_symbols = set()
    # Paths of already included object files from archives.
    added_contents = set()
    has_ar = False
    for f in files:
      if not f.startswith('-'):
        has_ar = has_ar or Building.is_ar(Building.make_paths_absolute(f))

    # If we have only one archive or the force_archive_contents flag is set,
    # then we will add every object file we see, regardless of whether it
    # resolves any undefined symbols.
    force_add_all = len(files) == 1 or force_archive_contents

    # Considers an object file for inclusion in the link. The object is included
    # if force_add=True or if the object provides a currently undefined symbol.
    # If the object is included, the symbol tables are updated and the function
    # returns True.
    def consider_object(f, force_add=False):
      new_symbols = Building.llvm_nm(f)
      # Check if the object was valid according to llvm-nm. It also accepts
      # native object files.
      if not new_symbols.is_valid_for_nm():
        diagnostics.warning('emcc', 'object %s is not valid according to llvm-nm, cannot link', f)
        return False
      # Check the object is valid for us, and not a native object file.
      if not Building.is_bitcode(f):
        exit_with_error('unknown file type: %s', f)
      provided = new_symbols.defs.union(new_symbols.commons)
      do_add = force_add or not unresolved_symbols.isdisjoint(provided)
      if do_add:
        logger.debug('adding object %s to link (forced: %d)' % (f, force_add))
        # Update resolved_symbols table with newly resolved symbols
        resolved_symbols.update(provided)
        # Update unresolved_symbols table by adding newly unresolved symbols and
        # removing newly resolved symbols.
        unresolved_symbols.update(new_symbols.undefs.difference(resolved_symbols))
        unresolved_symbols.difference_update(provided)
        actual_files.append(f)
      return do_add

    # Traverse a single archive. The object files are repeatedly scanned for
    # newly satisfied symbols until no new symbols are found. Returns true if
    # any object files were added to the link.
    def consider_archive(f, force_add):
      added_any_objects = False
      loop_again = True
      logger.debug('considering archive %s' % (f))
      contents = Building.ar_contents[f]
      while loop_again: # repeatedly traverse until we have everything we need
        loop_again = False
        for content in contents:
          if content in added_contents:
            continue
          # Link in the .o if it provides symbols, *or* this is a singleton archive (which is
          # apparently an exception in gcc ld)
          if consider_object(content, force_add=force_add):
            added_contents.add(content)
            loop_again = True
            added_any_objects = True
      logger.debug('done running loop of archive %s' % (f))
      return added_any_objects

    Building.read_link_inputs([x for x in files if not x.startswith('-')])

    # Rescan a group of archives until we don't find any more objects to link.
    def scan_archive_group(group):
      loop_again = True
      logger.debug('starting archive group loop')
      while loop_again:
        loop_again = False
        for archive in group:
          if consider_archive(archive, force_add=False):
            loop_again = True
      logger.debug('done with archive group loop')

    current_archive_group = None
    in_whole_archive = False
    for f in files:
      absolute_path_f = Building.make_paths_absolute(f)
      if f.startswith('-'):
        if f in ['--start-group', '-(']:
          assert current_archive_group is None, 'Nested --start-group, missing --end-group?'
          current_archive_group = []
        elif f in ['--end-group', '-)']:
          assert current_archive_group is not None, '--end-group without --start-group'
          scan_archive_group(current_archive_group)
          current_archive_group = None
        elif f in ['--whole-archive', '-whole-archive']:
          in_whole_archive = True
        elif f in ['--no-whole-archive', '-no-whole-archive']:
          in_whole_archive = False
        else:
          # Command line flags should already be vetted by the time this method
          # is called, so this is an internal error
          assert False, 'unsupported link flag: ' + f
      elif Building.is_ar(absolute_path_f):
        # Extract object files from ar archives, and link according to gnu ld semantics
        # (link in an entire .o from the archive if it supplies symbols still unresolved)
        consider_archive(absolute_path_f, in_whole_archive or force_add_all)
        # If we're inside a --start-group/--end-group section, add to the list
        # so we can loop back around later.
        if current_archive_group is not None:
          current_archive_group.append(absolute_path_f)
      elif Building.is_bitcode(absolute_path_f):
        if has_ar:
          consider_object(f, force_add=True)
        else:
          # If there are no archives then we can simply link all valid object
          # files and skip the symbol table stuff.
          actual_files.append(f)
      else:
        exit_with_error('unknown file type: %s', f)

    # We have to consider the possibility that --start-group was used without a matching
    # --end-group; GNU ld permits this behavior and implicitly treats the end of the
    # command line as having an --end-group.
    if current_archive_group:
      logger.debug('--start-group without matching --end-group, rescanning')
      scan_archive_group(current_archive_group)
      current_archive_group = None

    try_delete(target)

    # Finish link
    # tolerate people trying to link a.so a.so etc.
    actual_files = unique_ordered(actual_files)
    if just_calculate:
      # just calculating; return the link arguments which is the final list of files to link
      return actual_files

    logger.debug('emcc: Building.linking: %s to %s', actual_files, target)
    Building.link_llvm(actual_files, target)
    return target

  @staticmethod
  def get_command_with_possible_response_file(cmd):
    # 8k is a bit of an arbitrary limit, but a reasonable one
    # for max command line size before we use a response file
    if len(' '.join(cmd)) <= 8192:
      return cmd

    logger.debug('using response file for %s' % cmd[0])
    filename = response_file.create_response_file(cmd[1:], TEMP_DIR)
    new_cmd = [cmd[0], "@" + filename]
    return new_cmd

  # LLVM optimizations
  # @param opt A list of LLVM optimization parameters
  @staticmethod
  def llvm_opt(filename, opts, out=None):
    inputs = filename
    if not isinstance(inputs, list):
      inputs = [inputs]
    else:
      assert out, 'must provide out if llvm_opt on a list of inputs'
    assert len(opts), 'should not call opt with nothing to do'
    opts = opts[:]
    # TODO: disable inlining when needed
    # if not Building.can_inline():
    #   opts.append('-disable-inlining')
    # opts += ['-debug-pass=Arguments']
    # TODO: move vectorization logic to clang/LLVM?
    if not Settings.WASM_BACKEND:
      if not Settings.SIMD:
        opts += ['-disable-loop-vectorization', '-disable-slp-vectorization', '-vectorize-loops=false', '-vectorize-slp=false']
      else:
        opts += ['-force-vector-width=4']

    target = out or (filename + '.opt.bc')
    cmd = [LLVM_OPT] + inputs + opts + ['-o', target]
    cmd = Building.get_command_with_possible_response_file(cmd)
    print_compiler_stage(cmd)
    check_call(cmd)
    assert os.path.exists(target), 'llvm optimizer emitted no output.'
    if not out:
      shutil.move(filename + '.opt.bc', filename)
    return target

  @staticmethod
  def llvm_dis(input_filename, output_filename):
    # LLVM binary ==> LLVM assembly
    try_delete(output_filename)
    output = run_process([LLVM_DIS, input_filename, '-o', output_filename], stdout=PIPE).stdout
    assert os.path.exists(output_filename), 'Could not create .ll file: ' + output

  @staticmethod
  def llvm_as(input_filename, output_filename):
    # LLVM assembly ==> LLVM binary
    try_delete(output_filename)
    output = run_process([LLVM_AS, input_filename, '-o', output_filename], stdout=PIPE).stdout
    assert os.path.exists(output_filename), 'Could not create bc file: ' + output

  @staticmethod
  def parse_symbols(output, include_internal=False):
    defs = []
    undefs = []
    commons = []
    for line in output.split('\n'):
      if not line or line[0] == '#':
        continue
      # e.g.  filename.o:  , saying which file it's from
      if ':' in line:
        continue
      parts = [seg for seg in line.split(' ') if len(seg)]
      # pnacl-nm will print zero offsets for bitcode, and newer llvm-nm will print present symbols
      # as  -------- T name
      if len(parts) == 3 and parts[0] == "--------" or re.match(r'^[\da-f]{8}$', parts[0]):
        parts.pop(0)
      if len(parts) == 2:
        # ignore lines with absolute offsets, these are not bitcode anyhow
        # e.g. |00000630 t d_source_name|
        status, symbol = parts
        if status == 'U':
          undefs.append(symbol)
        elif status == 'C':
          commons.append(symbol)
        elif (not include_internal and status == status.upper()) or \
             (include_internal and status in ['W', 't', 'T', 'd', 'D']):
          # FIXME: using WTD in the previous line fails due to llvm-nm behavior on macOS,
          #        so for now we assume all uppercase are normally defined external symbols
          defs.append(symbol)
    return ObjectFileInfo(0, None, set(defs), set(undefs), set(commons))

  @staticmethod
  def llvm_nm_uncached(filename, stdout=PIPE, stderr=PIPE, include_internal=False):
    # LLVM binary ==> list of symbols
    proc = run_process([LLVM_NM, filename], stdout=stdout, stderr=stderr, check=False)
    if proc.returncode == 0:
      return Building.parse_symbols(proc.stdout, include_internal)
    else:
      return ObjectFileInfo(proc.returncode, str(proc.stdout) + str(proc.stderr))

  @staticmethod
  def llvm_nm(filename, stdout=PIPE, stderr=PIPE, include_internal=False):
    # Always use absolute paths to maximize cache usage
    filename = os.path.abspath(filename)

    if include_internal and filename in Building.internal_nm_cache:
      return Building.internal_nm_cache[filename]
    elif not include_internal and filename in Building.uninternal_nm_cache:
      return Building.uninternal_nm_cache[filename]

    ret = Building.llvm_nm_uncached(filename, stdout, stderr, include_internal)

    if ret.returncode != 0:
      logger.debug('llvm-nm failed on file ' + filename + ': return code ' + str(ret.returncode) + ', error: ' + ret.output)

    # Even if we fail, write the results to the NM cache so that we don't keep trying to llvm-nm the failing file again later.
    if include_internal:
      Building.internal_nm_cache[filename] = ret
    else:
      Building.uninternal_nm_cache[filename] = ret

    return ret

  @staticmethod
  def emcc(filename, args=[], output_filename=None, stdout=None, stderr=None, env=None):
    if output_filename is None:
      output_filename = filename + '.o'
    try_delete(output_filename)
    run_process([PYTHON, EMCC, filename] + args + ['-o', output_filename], stdout=stdout, stderr=stderr, env=env)

  @staticmethod
  def emar(action, output_filename, filenames, stdout=None, stderr=None, env=None):
    try_delete(output_filename)
    response_filename = response_file.create_response_file(filenames, TEMP_DIR)
    cmd = [PYTHON, EMAR, action, output_filename] + ['@' + response_filename]
    try:
      run_process(cmd, stdout=stdout, stderr=stderr, env=env)
    finally:
      try_delete(response_filename)

    if 'c' in action:
      assert os.path.exists(output_filename), 'emar could not create output file: ' + output_filename

  @staticmethod
  def can_inline():
    return Settings.INLINING_LIMIT == 0

  @staticmethod
  def need_asm_js_file():
    # Explicitly separate asm.js requires it
    if Settings.SEPARATE_ASM:
      return True
    return False

  @staticmethod
  def is_wasm_only():
    # not even wasm, much less wasm-only
    if not Settings.WASM:
      return False
    # llvm backend can only ever produce wasm
    if Settings.WASM_BACKEND:
      return True
    # fastcomp can emit wasm-only code.
    # also disable this mode if it depends on special optimizations that are not yet
    # compatible with it.
    if not Settings.LEGALIZE_JS_FFI:
      # the user has requested no legalization for JS, and so we are not
      # emitting code compatible with JS, and there is no reason not to
      # be wasm-only, regardless of everything else
      return True
    if Settings.RUNNING_JS_OPTS:
      # if the JS optimizer runs, it must run on valid asm.js
      return False
    if Settings.RELOCATABLE and Settings.EMULATED_FUNCTION_POINTERS:
      # FIXME(https://github.com/emscripten-core/emscripten/issues/5370)
      # emulation function pointers work properly, but calling between
      # modules as wasm-only needs more work
      return False
    return True

  @staticmethod
  def get_safe_internalize():
    if Settings.LINKABLE:
      return [] # do not internalize anything

    exps = Settings.EXPORTED_FUNCTIONS
    internalize_public_api = '-internalize-public-api-'
    internalize_list = ','.join([demangle_c_symbol_name(exp) for exp in exps])

    # EXPORTED_FUNCTIONS can potentially be very large.
    # 8k is a bit of an arbitrary limit, but a reasonable one
    # for max command line size before we use a response file
    if len(internalize_list) > 8192:
      logger.debug('using response file for EXPORTED_FUNCTIONS in internalize')
      finalized_exports = '\n'.join([exp[1:] for exp in exps])
      internalize_list_file = configuration.get_temp_files().get('.response').name
      with open(internalize_list_file, 'w') as f:
        f.write(finalized_exports)
      internalize_public_api += 'file=' + internalize_list_file
    else:
      internalize_public_api += 'list=' + internalize_list

    # internalize carefully, llvm 3.2 will remove even main if not told not to
    return ['-internalize', internalize_public_api]

  @staticmethod
  def opt_level_to_str(opt_level, shrink_level=0):
    # convert opt_level/shrink_level pair to a string argument like -O1
    if opt_level == 0:
      return '-O0'
    if shrink_level == 1:
      return '-Os'
    elif shrink_level >= 2:
      return '-Oz'
    else:
      return '-O' + str(min(opt_level, 3))

  @staticmethod
  def maybe_add_license(filename=None, code=None):
    # EMIT_EMSCRIPTEN_LICENSE is not supported in fastcomp since the JS
    # optimization pipeline there is much more complex, and fastcomp is
    # deprecated anyhow
    if Settings.EMIT_EMSCRIPTEN_LICENSE and Settings.WASM_BACKEND:
      if filename:
        with open(filename) as f:
          code = f.read()
      code = JS.emscripten_license + code
      if filename:
        with open(filename, 'w') as f:
          f.write(code)
      else:
        code = JS.emscripten_license + code
    if filename:
      return filename
    else:
      return code

  @staticmethod
  def js_optimizer(filename, passes, debug=False, extra_info=None, output_filename=None, just_split=False, just_concat=False, extra_closure_args=[], no_license=False):
    from . import js_optimizer
    try:
      ret = js_optimizer.run(filename, passes, NODE_JS, debug, extra_info, just_split, just_concat, extra_closure_args)
    except subprocess.CalledProcessError as e:
      exit_with_error("'%s' failed (%d)", ' '.join(e.cmd), e.returncode)
    if output_filename:
      safe_move(ret, output_filename)
      ret = output_filename
    if not no_license:
      ret = Building.maybe_add_license(filename=ret)
    return ret

  # run JS optimizer on some JS, ignoring asm.js contents if any - just run on it all
  @staticmethod
  def acorn_optimizer(filename, passes, extra_info=None, return_output=False):
    optimizer = path_from_root('tools', 'acorn-optimizer.js')
    original_filename = filename
    if extra_info is not None:
      temp_files = configuration.get_temp_files()
      temp = temp_files.get('.js').name
      shutil.copyfile(filename, temp)
      with open(temp, 'a') as f:
        f.write('// EXTRA_INFO: ' + extra_info)
      filename = temp
    cmd = NODE_JS + [optimizer, filename] + passes
    # Keep JS code comments intact through the acorn optimization pass so that JSDoc comments
    # will be carried over to a later Closure run.
    if Settings.USE_CLOSURE_COMPILER:
      cmd += ['--closureFriendly']
    if not return_output:
      next = original_filename + '.jso.js'
      configuration.get_temp_files().note(next)
      check_call(cmd, stdout=open(next, 'w'))
      next = Building.maybe_add_license(filename=next)
      return next
    output = check_call(cmd, stdout=PIPE).stdout
    output = Building.maybe_add_license(code=output)
    return output

  # evals ctors. if binaryen_bin is provided, it is the dir of the binaryen tool for this, and we are in wasm mode
  @staticmethod
  def eval_ctors(js_file, binary_file, binaryen_bin='', debug_info=False):
    if Settings.WASM_BACKEND:
      logger.debug('Ctor evalling in the wasm backend is disabled due to https://github.com/emscripten-core/emscripten/issues/9527')
      return
    cmd = [PYTHON, path_from_root('tools', 'ctor_evaller.py'), js_file, binary_file, str(Settings.INITIAL_MEMORY), str(Settings.TOTAL_STACK), str(Settings.GLOBAL_BASE), binaryen_bin, str(int(debug_info))]
    if binaryen_bin:
      cmd += Building.get_binaryen_feature_flags()
    print_compiler_stage(cmd)
    check_call(cmd)

  @staticmethod
  def eliminate_duplicate_funcs(filename):
    from . import duplicate_function_eliminator
    duplicate_function_eliminator.eliminate_duplicate_funcs(filename)

  @staticmethod
  def calculate_reachable_functions(infile, initial_list, can_reach=True):
    with ToolchainProfiler.profile_block('calculate_reachable_functions'):
      from . import asm_module
      temp = configuration.get_temp_files().get('.js').name
      Building.js_optimizer(infile, ['dumpCallGraph'], output_filename=temp, just_concat=True)
      asm = asm_module.AsmModule(temp)
      lines = asm.funcs_js.split('\n')
      can_call = {}
      for i in range(len(lines)):
        line = lines[i]
        if line.startswith('// REACHABLE '):
          curr = json.loads(line[len('// REACHABLE '):])
          func = curr[0]
          targets = curr[2]
          can_call[func] = set(targets)
      # function tables too - treat a function all as a function that can call anything in it, which is effectively what it is
      for name, funcs in asm.tables.items():
        can_call[name] = set([x.strip() for x in funcs[1:-1].split(',')])
      # print can_call
      # Note: We ignore calls in from outside the asm module, so you could do emterpreted => outside => emterpreted, and we would
      #       miss the first one there. But this is acceptable to do, because we can't save such a stack anyhow, due to the outside!
      # print 'can call', can_call, '\n!!!\n', asm.tables, '!'
      reachable_from = {}
      for func, targets in can_call.items():
        for target in targets:
          if target not in reachable_from:
            reachable_from[target] = set()
          reachable_from[target].add(func)
      # print 'reachable from', reachable_from
      to_check = initial_list[:]
      advised = set()
      if can_reach:
        # find all functions that can reach the initial list
        while len(to_check):
          curr = to_check.pop()
          if curr in reachable_from:
            for reacher in reachable_from[curr]:
              if reacher not in advised:
                if not JS.is_dyn_call(reacher) and not JS.is_function_table(reacher):
                  advised.add(str(reacher))
                to_check.append(reacher)
      else:
        # find all functions that are reachable from the initial list, including it
        # all tables are assumed reachable, as they can be called from dyncall from outside
        for name, funcs in asm.tables.items():
          to_check.append(name)
        while len(to_check):
          curr = to_check.pop()
          if not JS.is_function_table(curr):
            advised.add(curr)
          if curr in can_call:
            for target in can_call[curr]:
              if target not in advised:
                advised.add(str(target))
                to_check.append(target)
      return {'reachable': list(advised), 'total_funcs': len(can_call)}

  @staticmethod
  def check_closure_compiler(args, env):
    if not os.path.exists(CLOSURE_COMPILER[-1]):
      exit_with_error('google-closure-compiler executable (%s) does not exist, check the paths in %s.  To install closure compiler, run "npm install" in emscripten root directory.', CLOSURE_COMPILER[-1], EM_CONFIG)
    try:
      output = run_process(CLOSURE_COMPILER + args + ['--version'], stdout=PIPE, env=env).stdout
    except Exception as e:
      logger.warn(str(e))
      exit_with_error('closure compiler ("%s --version") did not execute properly!' % str(CLOSURE_COMPILER))
    if 'Version:' not in output:
      exit_with_error('unrecognized closure compiler --version output (%s):\n%s' % (str(CLOSURE_COMPILER), output))

  @staticmethod
  def closure_compiler(filename, pretty=True, advanced=True, extra_closure_args=None):
    with ToolchainProfiler.profile_block('closure_compiler'):
      env = env_with_node_in_path()
      user_args = []
      env_args = os.environ.get('EMCC_CLOSURE_ARGS')
      if env_args:
        user_args += shlex.split(env_args)
      if extra_closure_args:
        user_args += extra_closure_args

      # Closure compiler expects JAVA_HOME to be set *and* java.exe to be in the PATH in order
      # to enable use the java backend.  Without this it will only try the native and JavaScript
      # versions of the compiler.
      java_bin = os.path.dirname(JAVA)
      if java_bin:
        def add_to_path(dirname):
          env['PATH'] = env['PATH'] + os.pathsep + dirname
        add_to_path(java_bin)
        java_home = os.path.dirname(java_bin)
        env.setdefault('JAVA_HOME', java_home)

      if WINDOWS and not any(a.startswith('--platform') for a in user_args):
        # Disable native compiler on windows until upstream issue is fixed:
        # https://github.com/google/closure-compiler-npm/issues/147
        user_args.append('--platform=java')

      Building.check_closure_compiler(user_args, env)

      # Closure externs file contains known symbols to be extern to the minification, Closure
      # should not minify these symbol names.
      CLOSURE_EXTERNS = [path_from_root('src', 'closure-externs', 'closure-externs.js')]

      # Closure compiler needs to know about all exports that come from the asm.js/wasm module, because to optimize for small code size,
      # the exported symbols are added to global scope via a foreach loop in a way that evades Closure's static analysis. With an explicit
      # externs file for the exports, Closure is able to reason about the exports.
      if Settings.MODULE_EXPORTS and not Settings.DECLARE_ASM_MODULE_EXPORTS:
        # Generate an exports file that records all the exported symbols from asm.js/wasm module.
        module_exports_suppressions = '\n'.join(['/**\n * @suppress {duplicate, undefinedVars}\n */\nvar %s;\n' % i for i, j in Settings.MODULE_EXPORTS])
        exports_file = configuration.get_temp_files().get('_module_exports.js')
        exports_file.write(module_exports_suppressions.encode())
        exports_file.close()

        CLOSURE_EXTERNS += [exports_file.name]

      # Node.js specific externs
      if Settings.target_environment_may_be('node'):
        NODE_EXTERNS_BASE = path_from_root('third_party', 'closure-compiler', 'node-externs')
        NODE_EXTERNS = os.listdir(NODE_EXTERNS_BASE)
        NODE_EXTERNS = [os.path.join(NODE_EXTERNS_BASE, name) for name in NODE_EXTERNS
                        if name.endswith('.js')]
        CLOSURE_EXTERNS += [path_from_root('src', 'closure-externs', 'node-externs.js')] + NODE_EXTERNS

      # V8/SpiderMonkey shell specific externs
      if Settings.target_environment_may_be('shell'):
        V8_EXTERNS = [path_from_root('src', 'closure-externs', 'v8-externs.js')]
        SPIDERMONKEY_EXTERNS = [path_from_root('src', 'closure-externs', 'spidermonkey-externs.js')]
        CLOSURE_EXTERNS += V8_EXTERNS + SPIDERMONKEY_EXTERNS

      # Web environment specific externs
      if Settings.target_environment_may_be('web') or Settings.target_environment_may_be('worker'):
        BROWSER_EXTERNS_BASE = path_from_root('src', 'closure-externs', 'browser-externs')
        if os.path.isdir(BROWSER_EXTERNS_BASE):
          BROWSER_EXTERNS = os.listdir(BROWSER_EXTERNS_BASE)
          BROWSER_EXTERNS = [os.path.join(BROWSER_EXTERNS_BASE, name) for name in BROWSER_EXTERNS
                             if name.endswith('.js')]
          CLOSURE_EXTERNS += BROWSER_EXTERNS

      if Settings.MINIMAL_RUNTIME and Settings.USE_PTHREADS and not Settings.MODULARIZE:
        CLOSURE_EXTERNS += [path_from_root('src', 'minimal_runtime_worker_externs.js')]
      outfile = filename + '.cc.js'

      args = ['--compilation_level', 'ADVANCED_OPTIMIZATIONS' if advanced else 'SIMPLE_OPTIMIZATIONS',
              '--language_in', 'ECMASCRIPT5']
      for e in CLOSURE_EXTERNS:
        args += ['--externs', e]
      args += ['--js_output_file', outfile]

      if Settings.IGNORE_CLOSURE_COMPILER_ERRORS:
        args.append('--jscomp_off=*')
      if pretty:
        args += ['--formatting', 'PRETTY_PRINT']
      args += ['--js', filename]
      cmd = CLOSURE_COMPILER + args + user_args
      logger.debug('closure compiler: ' + ' '.join(cmd))

      proc = run_process(cmd, stderr=PIPE, check=False, env=env)

      # XXX Closure bug: if Closure is invoked with --create_source_map, Closure should create a
      # outfile.map source map file (https://github.com/google/closure-compiler/wiki/Source-Maps)
      # But it looks like it creates such files on Linux(?) even without setting that command line
      # flag (and currently we don't), so delete the produced source map file to not leak files in
      # temp directory.
      try_delete(outfile + '.map')

      # Print Closure diagnostics result up front.
      if proc.returncode != 0:
        logger.error('Closure compiler run failed:\n')
      elif len(proc.stderr.strip()) > 0:
        if Settings.CLOSURE_WARNINGS == 'error':
          logger.error('Closure compiler completed with warnings and -s CLOSURE_WARNINGS=error enabled, aborting!\n')
        elif Settings.CLOSURE_WARNINGS == 'warn':
          logger.warn('Closure compiler completed with warnings:\n')

      # Print input file (long wall of text!)
      if DEBUG == 2 and (proc.returncode != 0 or (len(proc.stderr.strip()) > 0 and Settings.CLOSURE_WARNINGS != 'quiet')):
        input_file = open(filename, 'r').read().splitlines()
        for i in range(len(input_file)):
          sys.stderr.write(str(i + 1) + ': ' + input_file[i] + '\n')

      if proc.returncode != 0:
        logger.error(proc.stderr) # print list of errors (possibly long wall of text if input was minified)

        # Exit and print final hint to get clearer output
        exit_with_error('closure compiler failed (rc: %d.%s)', proc.returncode, '' if pretty else ' the error message may be clearer with -g1 and EMCC_DEBUG=2 set')

      if len(proc.stderr.strip()) > 0 and Settings.CLOSURE_WARNINGS != 'quiet':
        # print list of warnings (possibly long wall of text if input was minified)
        if Settings.CLOSURE_WARNINGS == 'error':
          logger.error(proc.stderr)
        else:
          logger.warn(proc.stderr)

        # Exit and/or print final hint to get clearer output
        if not pretty:
          logger.warn('(rerun with -g1 linker flag for an unminified output)')
        elif DEBUG != 2:
          logger.warn('(rerun with EMCC_DEBUG=2 enabled to dump Closure input file)')

        if Settings.CLOSURE_WARNINGS == 'error':
          exit_with_error('closure compiler produced warnings and -s CLOSURE_WARNINGS=error enabled')

      # closure compiler will automatically preserve @license blocks, but we
      # have an explicit flag for that (EMIT_EMSCRIPTEN_LICENSE), which we
      # don't have a way to tell closure about. remove the comment here if we
      # don't want it (closure will aggregate all such comments into a single
      # big one at the top of the file)
      if not(Settings.EMIT_EMSCRIPTEN_LICENSE and Settings.WASM_BACKEND):
        with open(outfile) as f:
          code = f.read()
        if code.startswith('/*'):
          code = code[code.find('*/') + 2:]
          with open(outfile, 'w') as f:
            f.write(code)

      return outfile

  # minify the final wasm+JS combination. this is done after all the JS
  # and wasm optimizations; here we do the very final optimizations on them
  @staticmethod
  def minify_wasm_js(js_file, wasm_file, expensive_optimizations, minify_whitespace, debug_info):
    # start with JSDCE, to clean up obvious JS garbage. When optimizing for size,
    # use AJSDCE (aggressive JS DCE, performs multiple iterations). Clean up
    # whitespace if necessary too.
    passes = []
    if not Settings.LINKABLE:
      passes.append('JSDCE' if not expensive_optimizations else 'AJSDCE')
    if minify_whitespace:
      passes.append('minifyWhitespace')
    if passes:
      logger.debug('running cleanup on shell code: ' + ' '.join(passes))
      js_file = Building.acorn_optimizer(js_file, passes)
    # if we can optimize this js+wasm combination under the assumption no one else
    # will see the internals, do so
    if not Settings.LINKABLE:
      # if we are optimizing for size, shrink the combined wasm+JS
      # TODO: support this when a symbol map is used
      if expensive_optimizations:
        js_file = Building.metadce(js_file, wasm_file, minify_whitespace=minify_whitespace, debug_info=debug_info)
        # now that we removed unneeded communication between js and wasm, we can clean up
        # the js some more.
        passes = ['AJSDCE']
        if minify_whitespace:
          passes.append('minifyWhitespace')
        logger.debug('running post-meta-DCE cleanup on shell code: ' + ' '.join(passes))
        js_file = Building.acorn_optimizer(js_file, passes)
        if Settings.MINIFY_WASM_IMPORTS_AND_EXPORTS:
          js_file = Building.minify_wasm_imports_and_exports(js_file, wasm_file, minify_whitespace=minify_whitespace, minify_exports=Settings.MINIFY_ASMJS_EXPORT_NAMES, debug_info=debug_info)
    return js_file

  # run binaryen's wasm-metadce to dce both js and wasm
  @staticmethod
  def metadce(js_file, wasm_file, minify_whitespace, debug_info):
    logger.debug('running meta-DCE')
    temp_files = configuration.get_temp_files()
    # first, get the JS part of the graph
    extra_info = '{ "exports": [' + ','.join(map(lambda x: '["' + x[0] + '","' + x[1] + '"]', Settings.MODULE_EXPORTS)) + ']}'
    txt = Building.acorn_optimizer(js_file, ['emitDCEGraph', 'noPrint'], return_output=True, extra_info=extra_info)
    graph = json.loads(txt)
    # add exports based on the backend output, that are not present in the JS
    if not Settings.DECLARE_ASM_MODULE_EXPORTS:
      exports = set()
      for item in graph:
        if 'export' in item:
          exports.add(item['export'])
      for export, unminified in Settings.MODULE_EXPORTS:
        if export not in exports:
          graph.append({
            'export': export,
            'name': 'emcc$export$' + export,
            'reaches': []
          })
    # ensure that functions expected to be exported to the outside are roots
    for item in graph:
      if 'export' in item:
        export = item['export']
        # wasm backend's exports are prefixed differently inside the wasm
        if Settings.WASM_BACKEND:
          export = asmjs_mangle(export)
        if export in Building.user_requested_exports or Settings.EXPORT_ALL:
          item['root'] = True
    # in standalone wasm, always export the memory
    if Settings.STANDALONE_WASM:
      graph.append({
        'export': 'memory',
        'name': 'emcc$export$memory',
        'reaches': [],
        'root': True
      })
    # fix wasi imports TODO: support wasm stable with an option?
    WASI_IMPORTS = set([
      'environ_get',
      'environ_sizes_get',
      'args_get',
      'args_sizes_get',
      'fd_write',
      'fd_close',
      'fd_read',
      'fd_seek',
      'fd_fdstat_get',
      'fd_sync',
      'proc_exit',
      'clock_res_get',
      'clock_time_get',
    ])
    for item in graph:
      if 'import' in item and item['import'][1][1:] in WASI_IMPORTS:
        item['import'][0] = Settings.WASI_MODULE_NAME
    if Settings.WASM_BACKEND:
      # wasm backend's imports are prefixed differently inside the wasm
      for item in graph:
        if 'import' in item:
          if item['import'][1][0] == '_':
            item['import'][1] = item['import'][1][1:]
    # map import names from wasm to JS, using the actual name the wasm uses for the import
    import_name_map = {}
    for item in graph:
      if 'import' in item:
        import_name_map[item['name']] = 'emcc$import$' + item['import'][1]
    temp = temp_files.get('.txt').name
    txt = json.dumps(graph)
    with open(temp, 'w') as f:
      f.write(txt)
    # run wasm-metadce
    out = Building.run_binaryen_command('wasm-metadce',
                                        wasm_file,
                                        wasm_file,
                                        ['--graph-file=' + temp],
                                        debug=debug_info,
                                        stdout=PIPE)
    # find the unused things in js
    unused = []
    PREFIX = 'unused: '
    for line in out.splitlines():
      if line.startswith(PREFIX):
        name = line.replace(PREFIX, '').strip()
        if name in import_name_map:
          name = import_name_map[name]
        unused.append(name)
    # remove them
    passes = ['applyDCEGraphRemovals']
    if minify_whitespace:
      passes.append('minifyWhitespace')
    extra_info = {'unused': unused}
    return Building.acorn_optimizer(js_file, passes, extra_info=json.dumps(extra_info))

  @staticmethod
  def asyncify_lazy_load_code(wasm_binary_target, debug):
    # create the lazy-loaded wasm. remove the memory segments from it, as memory
    # segments have already been applied by the initial wasm, and apply the knowledge
    # that it will only rewind, after which optimizations can remove some code
    args = ['--remove-memory', '--mod-asyncify-never-unwind']
    if Settings.OPT_LEVEL > 0:
      args.append(Building.opt_level_to_str(Settings.OPT_LEVEL, Settings.SHRINK_LEVEL))
    Building.run_wasm_opt(wasm_binary_target,
                          wasm_binary_target + '.lazy.wasm',
                          args=args,
                          debug=debug)
    # re-optimize the original, by applying the knowledge that imports will
    # definitely unwind, and we never rewind, after which optimizations can remove
    # a lot of code
    # TODO: support other asyncify stuff, imports that don't always unwind?
    # TODO: source maps etc.
    args = ['--mod-asyncify-always-and-only-unwind']
    if Settings.OPT_LEVEL > 0:
      args.append(Building.opt_level_to_str(Settings.OPT_LEVEL, Settings.SHRINK_LEVEL))
    Building.run_wasm_opt(infile=wasm_binary_target,
                          outfile=wasm_binary_target,
                          args=args,
                          debug=debug)

  @staticmethod
  def minify_wasm_imports_and_exports(js_file, wasm_file, minify_whitespace, minify_exports, debug_info):
    logger.debug('minifying wasm imports and exports')
    # run the pass
    if minify_exports:
      # standalone wasm mode means we need to emit a wasi import module.
      # otherwise, minify even the imported module names.
      if Settings.MINIFY_WASM_IMPORTED_MODULES:
        pass_name = '--minify-imports-and-exports-and-modules'
      else:
        pass_name = '--minify-imports-and-exports'
    else:
      pass_name = '--minify-imports'
    out = Building.run_wasm_opt(wasm_file, wasm_file,
                                [pass_name],
                                debug=debug_info,
                                stdout=PIPE)
    # TODO this is the last tool we run, after normal opts and metadce. it
    # might make sense to run Stack IR optimizations here or even -O (as
    # metadce which runs before us might open up new general optimization
    # opportunities). however, the benefit is less than 0.5%.

    # get the mapping
    SEP = ' => '
    mapping = {}
    for line in out.split('\n'):
      if SEP in line:
        old, new = line.strip().split(SEP)
        assert old not in mapping, 'imports must be unique'
        mapping[old] = new
    # apply them
    passes = ['applyImportAndExportNameChanges']
    if minify_whitespace:
      passes.append('minifyWhitespace')
    extra_info = {'mapping': mapping}
    return Building.acorn_optimizer(js_file, passes, extra_info=json.dumps(extra_info))

  @staticmethod
  def wasm2js(js_file, wasm_file, opt_level, minify_whitespace, use_closure_compiler, debug_info, symbols_file=None):
    logger.debug('wasm2js')
    args = ['--emscripten']
    if opt_level > 0:
      args += ['-O']
    if symbols_file:
      args += ['--symbols-file=%s' % symbols_file]
    wasm2js_js = Building.run_binaryen_command('wasm2js', wasm_file,
                                               args=args,
                                               debug=debug_info,
                                               stdout=PIPE)
    if DEBUG:
      with open(os.path.join(get_emscripten_temp_dir(), 'wasm2js-output.js'), 'w') as f:
        f.write(wasm2js_js)
    # JS optimizations
    if opt_level >= 2:
      passes = []
      # it may be useful to also run: simplifyIfs, registerize, asmLastOpts
      # passes += ['simplifyExpressions'] # XXX fails on wasm3js.test_sqlite
      if not debug_info:
        passes += ['minifyNames']
      if minify_whitespace:
        passes += ['minifyWhitespace']
      passes += ['last']
      if passes:
        # hackish fixups to work around wasm2js style and the js optimizer FIXME
        wasm2js_js = '// EMSCRIPTEN_START_ASM\n' + wasm2js_js + '// EMSCRIPTEN_END_ASM\n'
        wasm2js_js = wasm2js_js.replace('// EMSCRIPTEN_START_FUNCS;\n', '// EMSCRIPTEN_START_FUNCS\n')
        wasm2js_js = wasm2js_js.replace('// EMSCRIPTEN_END_FUNCS;\n', '// EMSCRIPTEN_END_FUNCS\n')
        wasm2js_js = wasm2js_js.replace('\n function $', '\nfunction $')
        wasm2js_js = wasm2js_js.replace('\n }', '\n}')
        wasm2js_js += '\n// EMSCRIPTEN_GENERATED_FUNCTIONS\n'
        temp = configuration.get_temp_files().get('.js').name
        with open(temp, 'w') as f:
          f.write(wasm2js_js)
        # pass no_license since we are only optimizing a subset of the JS. if
        # a license is needed, the js as a whole will have it anyhow.
        temp = Building.js_optimizer(temp, passes, no_license=True)
        with open(temp) as f:
          wasm2js_js = f.read()
    # Closure compiler: in mode 1, we just minify the shell. In mode 2, we
    # minify the wasm2js output as well, which is ok since it isn't
    # validating asm.js.
    # TODO: in the non-closure case, we could run a lightweight general-
    #       purpose JS minifier here.
    if use_closure_compiler == 2:
      temp = configuration.get_temp_files().get('.js').name
      with open(temp, 'a') as f:
        f.write(wasm2js_js)
      temp = Building.closure_compiler(temp,
                                       pretty=not minify_whitespace,
                                       advanced=False)
      with open(temp) as f:
        wasm2js_js = f.read()
      # closure may leave a trailing `;`, which would be invalid given where we place
      # this code (inside parens)
      wasm2js_js = wasm2js_js.strip()
      if wasm2js_js[-1] == ';':
        wasm2js_js = wasm2js_js[:-1]
    with open(js_file) as f:
      all_js = f.read()
    # quoted notation, something like Module['__wasm2jsInstantiate__']
    finds = re.findall(r'''[\w\d_$]+\[['"]__wasm2jsInstantiate__['"]\]''', all_js)
    if not finds:
      # post-closure notation, something like a.__wasm2jsInstantiate__
      finds = re.findall(r'''[\w\d_$]+\.__wasm2jsInstantiate__''', all_js)
    assert len(finds) == 1
    marker = finds[0]
    all_js = all_js.replace(marker, '(\n' + wasm2js_js + '\n)')
    # replace the placeholder with the actual code
    js_file = js_file + '.wasm2js.js'
    with open(js_file, 'w') as f:
      f.write(all_js)
    return js_file

  @staticmethod
  def emit_debug_on_side(wasm_file, wasm_file_with_dwarf):
    # extract the DWARF info from the main file, and leave the wasm with
    # debug into as a file on the side
    # TODO: emit only debug sections in the side file, and not the entire
    #       wasm as well
    shutil.move(wasm_file, wasm_file_with_dwarf)
    run_process([LLVM_OBJCOPY, '--remove-section=.debug*', wasm_file_with_dwarf, wasm_file])

    # embed a section in the main wasm to point to the file with external DWARF,
    # see https://yurydelendik.github.io/webassembly-dwarf/#external-DWARF
    section_name = b'\x13external_debug_info' # section name, including prefixed size
    contents = asbytes(wasm_file_with_dwarf)
    section_size = len(section_name) + len(contents)
    with open(wasm_file, 'ab') as f:
      f.write(b'\0') # user section is code 0
      f.write(WebAssembly.toLEB(section_size))
      f.write(section_name)
      f.write(contents)

  @staticmethod
  def apply_wasm_memory_growth(js_file):
    logger.debug('supporting wasm memory growth with pthreads')
    fixed = Building.acorn_optimizer(js_file, ['growableHeap'])
    ret = js_file + '.pgrow.js'
    with open(fixed, 'r') as fixed_f:
      with open(ret, 'w') as ret_f:
        with open(path_from_root('src', 'growableHeap.js')) as support_code_f:
          ret_f.write(support_code_f.read() + '\n' + fixed_f.read())
    return ret

  @staticmethod
  def use_unsigned_pointers_in_js(js_file):
    logger.debug('using unsigned pointers in JS')
    return Building.acorn_optimizer(js_file, ['unsignPointers'])

  @staticmethod
  def instrument_js_for_asan(js_file):
    logger.debug('instrumenting JS memory accesses for ASan')
    return Building.acorn_optimizer(js_file, ['asanify'])

  @staticmethod
  def handle_final_wasm_symbols(wasm_file, symbols_file, debug_info):
    logger.debug('handle_final_wasm_symbols')
    args = []
    if symbols_file:
      args += ['--print-function-map']
    if not debug_info:
      # to remove debug info, we just write to that same file, and without -g
      args += ['-o', wasm_file]
    # ignore stderr because if wasm-opt is run without a -o it will warn
    output = Building.run_wasm_opt(wasm_file, args=args, stdout=PIPE)
    if symbols_file:
      with open(symbols_file, 'w') as f:
        f.write(output)

  # the exports the user requested
  user_requested_exports = []

  @staticmethod
  def is_ar(filename):
    try:
      if Building._is_ar_cache.get(filename):
        return Building._is_ar_cache[filename]
      header = open(filename, 'rb').read(8)
      sigcheck = header == b'!<arch>\n'
      Building._is_ar_cache[filename] = sigcheck
      return sigcheck
    except Exception as e:
      logger.debug('Building.is_ar failed to test whether file \'%s\' is a llvm archive file! Failed on exception: %s' % (filename, e))
      return False

  @staticmethod
  def is_bitcode(filename):
    try:
      # look for magic signature
      b = open(filename, 'rb').read(4)
      if b[:2] == b'BC':
        return True
      # on macOS, there is a 20-byte prefix which starts with little endian
      # encoding of 0x0B17C0DE
      elif b == b'\xDE\xC0\x17\x0B':
        b = bytearray(open(filename, 'rb').read(22))
        return b[20:] == b'BC'
    except IndexError:
      # not enough characters in the input
      # note that logging will be done on the caller function
      pass
    return False

  @staticmethod
  def is_wasm(filename):
    magic = open(filename, 'rb').read(4)
    return magic == b'\0asm'

  @staticmethod
  # Given the name of a special Emscripten-implemented system library, returns an array of absolute paths to JS library
  # files inside emscripten/src/ that corresponds to the library name.
  def path_to_system_js_libraries(library_name):
    # Some native libraries are implemented in Emscripten as system side JS libraries
    js_system_libraries = {
      'c': '',
      'dl': '',
      'EGL': 'library_egl.js',
      'GL': 'library_webgl.js',
      'GLESv2': 'library_webgl.js',
      # N.b. there is no GLESv3 to link to (note [f] in https://www.khronos.org/registry/implementers_guide.html)
      'GLEW': 'library_glew.js',
      'glfw': 'library_glfw.js',
      'glfw3': 'library_glfw.js',
      'GLU': '',
      'glut': 'library_glut.js',
      'm': '',
      'openal': 'library_openal.js',
      'rt': '',
      'pthread': '',
      'X11': 'library_xlib.js',
      'SDL': 'library_sdl.js',
      'stdc++': '',
      'uuid': 'library_uuid.js',
      'websocket': 'library_websocket.js'
    }
    library_files = []
    if library_name in js_system_libraries:
      if len(js_system_libraries[library_name]):
        lib = js_system_libraries[library_name] if isinstance(js_system_libraries[library_name], list) else [js_system_libraries[library_name]]
        library_files += lib
        logger.debug('Linking in JS library ' + str(lib))

    elif library_name.endswith('.js') and os.path.isfile(path_from_root('src', 'library_' + library_name)):
      library_files += ['library_' + library_name]
    elif not Settings.WASM_BACKEND:
      # The wasm backend will report these when wasm-ld runs
      exit_with_error('emcc: cannot find library "%s"', library_name)

    return library_files

  @staticmethod
  def emit_wasm_source_map(wasm_file, map_file):
    # source file paths must be relative to the location of the map (which is
    # emitted alongside the wasm)
    base_path = os.path.dirname(os.path.abspath(Settings.WASM_BINARY_FILE))
    sourcemap_cmd = [PYTHON, path_from_root('tools', 'wasm-sourcemap.py'),
                     wasm_file,
                     '--dwarfdump=' + LLVM_DWARFDUMP,
                     '-o',  map_file,
                     '--basepath=' + base_path]
    check_call(sourcemap_cmd)

  @staticmethod
  def get_binaryen_feature_flags():
    # start with the MVP features, add the rest as needed
    ret = ['--mvp-features']
    if Settings.USE_PTHREADS:
      ret += ['--enable-threads']
    if Settings.SIMD:
      ret += ['--enable-simd']
    ret += Settings.BINARYEN_FEATURES
    return ret

  @staticmethod
  def check_binaryen(bindir):
    opt = os.path.join(bindir, exe_suffix('wasm-opt'))
    if not os.path.exists(opt):
      exit_with_error('binaryen executable not found (%s). Please check your binaryen installation' % opt)
    try:
      output = run_process([opt, '--version'], stdout=PIPE).stdout
    except subprocess.CalledProcessError:
      exit_with_error('error running binaryen executable (%s). Please check your binaryen installation' % opt)
    if output:
      output = output.splitlines()[0]
    try:
      version = output.split()[2]
      version = int(version)
    except (IndexError, ValueError):
      exit_with_error('error parsing binaryen version (%s). Please check your binaryen installation (%s)' % (output, opt))

    # Allow the expected version or the following one in order avoid needing to update both
    # emscripten and binaryen in lock step in emscripten-releases.
    if version not in (EXPECTED_BINARYEN_VERSION, EXPECTED_BINARYEN_VERSION + 1):
      diagnostics.warning('version-check', 'unexpected binaryen version: %s (expected %s)', version, EXPECTED_BINARYEN_VERSION)

  @staticmethod
  def get_binaryen_bin():
    assert Settings.WASM, 'non wasm builds should not ask for binaryen'
    rtn = os.path.join(BINARYEN_ROOT, 'bin')
    if not Building.binaryen_checked:
      Building.check_binaryen(rtn)
      Building.binaryen_checked = True
    return rtn

  @staticmethod
  def run_binaryen_command(tool, infile, outfile=None, args=[], debug=False, stdout=None):
    cmd = [os.path.join(Building.get_binaryen_bin(), tool)]
    if outfile and tool == 'wasm-opt' and Settings.DEBUG_LEVEL != 3:
      # remove any dwarf debug info sections, if the debug level is <3, as
      # we don't need them; also remove them if we the level is 4, as then we
      # want a source map, which is implemented separately from dwarf.
      # note that we add this pass first, so that it doesn't interfere with
      # the final set of passes (which may generate stack IR, and nothing
      # should be run after that)
      # TODO: if lld can strip dwarf then we don't need this. atm though it can
      #       only strip all debug info or none, which includes the name section
      #       which we may need
      # TODO: once fastcomp is gone, either remove source maps entirely, or
      #       support them by emitting a source map at the end from the dwarf,
      #       and use llvm-objcpy to remove that final dwarf
      cmd += ['--strip-dwarf']
    cmd += args
    if infile:
      cmd += [infile]
    if outfile:
      cmd += ['-o', outfile]
    if debug:
      cmd += ['-g'] # preserve the debug info
    # if the features are not already handled, handle them
    if '--detect-features' not in cmd:
      cmd += Building.get_binaryen_feature_flags()
    print_compiler_stage(cmd)
    # if we are emitting a source map, every time we load and save the wasm
    # we must tell binaryen to update it
    emit_source_map = Settings.DEBUG_LEVEL == 4 and outfile
    if emit_source_map:
      cmd += ['--input-source-map=' + infile + '.map']
      cmd += ['--output-source-map=' + outfile + '.map']
    ret = check_call(cmd, stdout=stdout).stdout
    if outfile:
      Building.save_intermediate(outfile, '%s.wasm' % tool)
    return ret

  @staticmethod
  def run_wasm_opt(*args, **kwargs):
    return Building.run_binaryen_command('wasm-opt', *args, **kwargs)

  save_intermediate_counter = 0

  @staticmethod
  def save_intermediate(src, dst):
    if DEBUG:
      dst = 'emcc-%d-%s' % (Building.save_intermediate_counter, dst)
      Building.save_intermediate_counter += 1
      dst = os.path.join(CANONICAL_TEMP_DIR, dst)
      logger.debug('saving debug copy %s' % dst)
      shutil.copyfile(src, dst)


def reconfigure_cache():
  global Cache
  Cache = cache.Cache(CACHE)


# Placeholder strings used for SINGLE_FILE
class FilenameReplacementStrings:
  WASM_TEXT_FILE = '{{{ FILENAME_REPLACEMENT_STRINGS_WASM_TEXT_FILE }}}'
  WASM_BINARY_FILE = '{{{ FILENAME_REPLACEMENT_STRINGS_WASM_BINARY_FILE }}}'
  ASMJS_CODE_FILE = '{{{ FILENAME_REPLACEMENT_STRINGS_ASMJS_CODE_FILE }}}'


class JS(object):
  memory_initializer_pattern = r'/\* memory initializer \*/ allocate\(\[([\d, ]*)\], "i8", ALLOC_NONE, ([\d+\.GLOBAL_BASEHgb]+)\);'
  no_memory_initializer_pattern = r'/\* no memory initializer \*/'

  memory_staticbump_pattern = r'STATICTOP = STATIC_BASE \+ (\d+);'

  global_initializers_pattern = r'/\* global initializers \*/ __ATINIT__.push\((.+)\);'

  emscripten_license = '''\
/**
 * @license
 * Copyright 2010 Emscripten authors
 * SPDX-License-Identifier: MIT
 */
'''

  @staticmethod
  def to_nice_ident(ident): # limited version of the JS function toNiceIdent
    return ident.replace('%', '$').replace('@', '_').replace('.', '_')

  # Returns the given string with escapes added so that it can safely be placed inside a string in JS code.
  @staticmethod
  def escape_for_js_string(s):
    s = s.replace('\\', '/').replace("'", "\\'").replace('"', '\\"')
    return s

  # Returns the subresource location for run-time access
  @staticmethod
  def get_subresource_location(path, data_uri=None):
    if data_uri is None:
      data_uri = Settings.SINGLE_FILE
    if data_uri:
      # if the path does not exist, then there is no data to encode
      if not os.path.exists(path):
        return ''
      with open(path, 'rb') as f:
        data = base64.b64encode(f.read())
      return 'data:application/octet-stream;base64,' + asstr(data)
    else:
      return os.path.basename(path)

  @staticmethod
  def make_initializer(sig, settings=None):
    settings = settings or Settings
    if sig == 'i':
      return '0'
    elif sig == 'f' and settings.get('PRECISE_F32'):
      return 'Math_fround(0)'
    elif sig == 'j':
      if settings:
        assert settings['WASM'], 'j aka i64 only makes sense in wasm-only mode in binaryen'
      return 'i64(0)'
    elif sig == 'F':
      return 'SIMD_Float32x4_check(SIMD_Float32x4(0,0,0,0))'
    elif sig == 'D':
      return 'SIMD_Float64x2_check(SIMD_Float64x2(0,0,0,0))'
    elif sig == 'B':
      return 'SIMD_Int8x16_check(SIMD_Int8x16(0,0,0,0))'
    elif sig == 'S':
      return 'SIMD_Int16x8_check(SIMD_Int16x8(0,0,0,0))'
    elif sig == 'I':
      return 'SIMD_Int32x4_check(SIMD_Int32x4(0,0,0,0))'
    else:
      return '+0'

  FLOAT_SIGS = ['f', 'd']

  @staticmethod
  def make_coercion(value, sig, settings=None, ffi_arg=False, ffi_result=False, convert_from=None):
    settings = settings or Settings
    if sig == 'i':
      if convert_from in JS.FLOAT_SIGS:
        value = '(~~' + value + ')'
      return value + '|0'
    if sig in JS.FLOAT_SIGS and convert_from == 'i':
      value = '(' + value + '|0)'
    if sig == 'f' and settings.get('PRECISE_F32'):
      if ffi_arg:
        return '+Math_fround(' + value + ')'
      elif ffi_result:
        return 'Math_fround(+(' + value + '))'
      else:
        return 'Math_fround(' + value + ')'
    elif sig == 'd' or sig == 'f':
      return '+' + value
    elif sig == 'j':
      if settings:
        assert settings['WASM'], 'j aka i64 only makes sense in wasm-only mode in binaryen'
      return 'i64(' + value + ')'
    elif sig == 'F':
      return 'SIMD_Float32x4_check(' + value + ')'
    elif sig == 'D':
      return 'SIMD_Float64x2_check(' + value + ')'
    elif sig == 'B':
      return 'SIMD_Int8x16_check(' + value + ')'
    elif sig == 'S':
      return 'SIMD_Int16x8_check(' + value + ')'
    elif sig == 'I':
      return 'SIMD_Int32x4_check(' + value + ')'
    else:
      return value

  @staticmethod
  def legalize_sig(sig):
    # with BigInt support all sigs are legal since we can use i64s.
    if Settings.WASM_BIGINT:
      return sig
    legal = [sig[0]]
    # a return of i64 is legalized into an i32 (and the high bits are
    # accessible on the side through getTempRet0).
    if legal[0] == 'j':
      legal[0] = 'i'
    # a parameter of i64 is legalized into i32, i32
    for s in sig[1:]:
      if s != 'j':
        legal.append(s)
      else:
        legal.append('i')
        legal.append('i')
    return ''.join(legal)

  @staticmethod
  def is_legal_sig(sig):
    # with BigInt support all sigs are legal since we can use i64s.
    if Settings.WASM_BIGINT:
      return True
    return sig == JS.legalize_sig(sig)

  @staticmethod
  def make_extcall(sig, named=True):
    args = ','.join(['a' + str(i) for i in range(1, len(sig))])
    args = 'index' + (',' if args else '') + args
    # C++ exceptions are numbers, and longjmp is a string 'longjmp'
    ret = '''function%s(%s) {
  %sModule["dynCall_%s"](%s);
}''' % ((' extCall_' + sig) if named else '', args, 'return ' if sig[0] != 'v' else '', sig, args)
    return ret

  @staticmethod
  def make_jscall(sig):
    fnargs = ','.join('a' + str(i) for i in range(1, len(sig)))
    args = (',' if fnargs else '') + fnargs
    ret = '''\
function jsCall_%s(index%s) {
    %sfunctionPointers[index](%s);
}''' % (sig, args, 'return ' if sig[0] != 'v' else '', fnargs)
    return ret

  @staticmethod
  def make_dynCall(sig):
    # Optimize dynCall accesses in the case when not building with dynamic
    # linking enabled.
    if not Settings.MAIN_MODULE and not Settings.SIDE_MODULE:
      return 'dynCall_' + sig
    else:
      return 'Module["dynCall_' + sig + '"]'

  @staticmethod
  def make_invoke(sig, named=True):
    if sig == 'X':
      # 'X' means the generic unknown signature, used in wasm dynamic linking
      # to indicate an invoke that the main JS may not have defined, so we
      # go through this (which may be slower, as we don't declare the
      # arguments explicitly). In non-wasm dynamic linking, the other modules
      # have JS and so can define their own invokes to be linked in.
      # This only makes sense in function pointer emulation mode, where we
      # can do a direct table call.
      assert Settings.WASM
      assert Settings.WASM_BACKEND or Settings.EMULATED_FUNCTION_POINTERS
      args = ''
      body = '''
        var args = Array.prototype.slice.call(arguments);
        return wasmTable.get(args[0]).apply(null, args.slice(1));
      '''
    else:
      legal_sig = JS.legalize_sig(sig) # TODO: do this in extcall, jscall?
      args = ','.join(['a' + str(i) for i in range(1, len(legal_sig))])
      args = 'index' + (',' if args else '') + args
      ret = 'return ' if sig[0] != 'v' else ''
      body = '%s%s(%s);' % (ret, JS.make_dynCall(sig), args)
    # C++ exceptions are numbers, and longjmp is a string 'longjmp'
    if Settings.SUPPORT_LONGJMP:
      rethrow = "if (e !== e+0 && e !== 'longjmp') throw e;"
    else:
      rethrow = "if (e !== e+0) throw e;"

    ret = '''function%s(%s) {
  var sp = stackSave();
  try {
    %s
  } catch(e) {
    stackRestore(sp);
    %s
    _setThrew(1, 0);
  }
}''' % ((' invoke_' + sig) if named else '', args, body, rethrow)
    return ret

  @staticmethod
  def align(x, by):
    while x % by != 0:
      x += 1
    return x

  @staticmethod
  def generate_string_initializer(s):
    if Settings.ASSERTIONS:
      # append checksum of length and content
      crcTable = []
      for i in range(256):
        crc = i
        for bit in range(8):
          crc = (crc >> 1) ^ ((crc & 1) * 0xedb88320)
        crcTable.append(crc)
      crc = 0xffffffff
      n = len(s)
      crc = crcTable[(crc ^ n) & 0xff] ^ (crc >> 8)
      crc = crcTable[(crc ^ (n >> 8)) & 0xff] ^ (crc >> 8)
      for i in s:
        crc = crcTable[(crc ^ i) & 0xff] ^ (crc >> 8)
      for i in range(4):
        s.append((crc >> (8 * i)) & 0xff)
    s = ''.join(map(chr, s))
    s = s.replace('\\', '\\\\').replace("'", "\\'")
    s = s.replace('\n', '\\n').replace('\r', '\\r')

    # Escape the ^Z (= 0x1a = substitute) ASCII character and all characters higher than 7-bit ASCII.
    def escape(x):
      return '\\x{:02x}'.format(ord(x.group()))

    return re.sub('[\x1a\x80-\xff]', escape, s)

  @staticmethod
  def is_dyn_call(func):
    return func.startswith('dynCall_')

  @staticmethod
  def is_function_table(name):
    return name.startswith('FUNCTION_TABLE_')


class WebAssembly(object):
  @staticmethod
  def toLEB(x):
    assert x >= 0, 'TODO: signed'
    ret = []
    while 1:
      byte = x & 127
      x >>= 7
      more = x != 0
      if more:
        byte = byte | 128
      ret.append(byte)
      if not more:
        break
    return bytearray(ret)

  @staticmethod
  def readLEB(buf, offset):
    result = 0
    shift = 0
    while True:
      byte = bytearray(buf[offset:offset + 1])[0]
      offset += 1
      result |= (byte & 0x7f) << shift
      if not (byte & 0x80):
        break
      shift += 7
    return (result, offset)

  @staticmethod
  def add_emscripten_metadata(js_file, wasm_file):
    WASM_PAGE_SIZE = 65536

    mem_size = Settings.INITIAL_MEMORY // WASM_PAGE_SIZE
    table_size = Settings.WASM_TABLE_SIZE
    global_base = Settings.GLOBAL_BASE

    js = open(js_file).read()
    if Settings.WASM_BACKEND:
      tempdouble_ptr = 0
    else:
      m = re.search(r"(^|\s)tempDoublePtr\s+=\s+(\d+)", js)
      tempdouble_ptr = int(m.group(2))
    m = re.search(r"(^|\s)DYNAMIC_BASE\s+=\s+(\d+)", js)
    dynamic_base = int(m.group(2))
    m = re.search(r"(^|\s)DYNAMICTOP_PTR\s+=\s+(\d+)", js)
    dynamictop_ptr = int(m.group(2))

    logger.debug('creating wasm emscripten metadata section with mem size %d, table size %d' % (mem_size, table_size,))
    name = b'\x13emscripten_metadata' # section name, including prefixed size
    contents = (
      # metadata section version
      WebAssembly.toLEB(EMSCRIPTEN_METADATA_MAJOR) +
      WebAssembly.toLEB(EMSCRIPTEN_METADATA_MINOR) +

      # NB: The structure of the following should only be changed
      #     if EMSCRIPTEN_METADATA_MAJOR is incremented
      # Minimum ABI version
      WebAssembly.toLEB(EMSCRIPTEN_ABI_MAJOR) +
      WebAssembly.toLEB(EMSCRIPTEN_ABI_MINOR) +

      WebAssembly.toLEB(int(Settings.WASM_BACKEND)) +
      WebAssembly.toLEB(mem_size) +
      WebAssembly.toLEB(table_size) +
      WebAssembly.toLEB(global_base) +
      WebAssembly.toLEB(dynamic_base) +
      WebAssembly.toLEB(dynamictop_ptr) +
      WebAssembly.toLEB(tempdouble_ptr) +
      WebAssembly.toLEB(int(Settings.STANDALONE_WASM))

      # NB: more data can be appended here as long as you increase
      #     the EMSCRIPTEN_METADATA_MINOR
    )

    orig = open(wasm_file, 'rb').read()
    with open(wasm_file, 'wb') as f:
      f.write(orig[0:8]) # copy magic number and version
      # write the special section
      f.write(b'\0') # user section is code 0
      # need to find the size of this section
      size = len(name) + len(contents)
      f.write(WebAssembly.toLEB(size))
      f.write(name)
      f.write(contents)
      f.write(orig[8:])

  @staticmethod
  def add_dylink_section(wasm_file, needed_dynlibs):
    # a wasm shared library has a special "dylink" section, see tools-conventions repo
    assert not Settings.WASM_BACKEND
    mem_align = Settings.MAX_GLOBAL_ALIGN
    mem_size = Settings.STATIC_BUMP
    table_size = Settings.WASM_TABLE_SIZE
    mem_align = int(math.log(mem_align, 2))
    logger.debug('creating wasm dynamic library with mem size %d, table size %d, align %d' % (mem_size, table_size, mem_align))

    # Write new wasm binary with 'dylink' section
    wasm = open(wasm_file, 'rb').read()
    section_name = b"\06dylink" # section name, including prefixed size
    contents = (WebAssembly.toLEB(mem_size) + WebAssembly.toLEB(mem_align) +
                WebAssembly.toLEB(table_size) + WebAssembly.toLEB(0))

    # we extend "dylink" section with information about which shared libraries
    # our shared library needs. This is similar to DT_NEEDED entries in ELF.
    #
    # In theory we could avoid doing this, since every import in wasm has
    # "module" and "name" attributes, but currently emscripten almost always
    # uses just "env" for "module". This way we have to embed information about
    # required libraries for the dynamic linker somewhere, and "dylink" section
    # seems to be the most relevant place.
    #
    # Binary format of the extension:
    #
    #   needed_dynlibs_count        varuint32       ; number of needed shared libraries
    #   needed_dynlibs_entries      dynlib_entry*   ; repeated dynamic library entries as described below
    #
    # dynlib_entry:
    #
    #   dynlib_name_len             varuint32       ; length of dynlib_name_str in bytes
    #   dynlib_name_str             bytes           ; name of a needed dynamic library: valid UTF-8 byte sequence
    #
    # a proposal has been filed to include the extension into "dylink" specification:
    # https://github.com/WebAssembly/tool-conventions/pull/77
    contents += WebAssembly.toLEB(len(needed_dynlibs))
    for dyn_needed in needed_dynlibs:
      dyn_needed = bytes(asbytes(dyn_needed))
      contents += WebAssembly.toLEB(len(dyn_needed))
      contents += dyn_needed

    section_size = len(section_name) + len(contents)
    with open(wasm_file, 'wb') as f:
      # copy magic number and version
      f.write(wasm[0:8])
      # write the special section
      f.write(b'\0') # user section is code 0
      f.write(WebAssembly.toLEB(section_size))
      f.write(section_name)
      f.write(contents)
      # copy rest of binary
      f.write(wasm[8:])


# Python 2-3 compatibility helper function:
# Converts a string to the native str type.
def asstr(s):
  if str is bytes:
    if isinstance(s, type(u'')):
      return s.encode('utf-8')
  elif isinstance(s, bytes):
    return s.decode('utf-8')
  return s


def asbytes(s):
  if isinstance(s, bytes):
    # Do not attempt to encode bytes
    return s
  return s.encode('utf-8')


def unsuffixed(name):
  """Return the filename without the extention.

  If there are multiple extensions this strips only the final one.
  """
  return os.path.splitext(name)[0]


def unsuffixed_basename(name):
  return os.path.basename(unsuffixed(name))


def safe_move(src, dst):
  src = os.path.abspath(src)
  dst = os.path.abspath(dst)
  if os.path.isdir(dst):
    dst = os.path.join(dst, os.path.basename(src))
  if src == dst:
    return
  if dst == '/dev/null':
    return
  logging.debug('move: %s -> %s', src, dst)
  shutil.move(src, dst)


def safe_copy(src, dst):
  src = os.path.abspath(src)
  dst = os.path.abspath(dst)
  if os.path.isdir(dst):
    dst = os.path.join(dst, os.path.basename(src))
  if src == dst:
    return
  if dst == '/dev/null':
    return
  shutil.copyfile(src, dst)


def read_and_preprocess(filename, expand_macros=False):
  temp_dir = get_emscripten_temp_dir()
  # Create a settings file with the current settings to pass to the JS preprocessor
  # Note: Settings.serialize returns an array of -s options i.e. ['-s', '<setting1>', '-s', '<setting2>', ...]
  #       we only want the actual settings, hence the [1::2] slice operation.
  settings_str = "var " + ";\nvar ".join(Settings.serialize()[1::2])
  settings_file = os.path.join(temp_dir, 'settings.js')
  open(settings_file, 'w').write(settings_str)

  # Run the JS preprocessor
  # N.B. We can't use the default stdout=PIPE here as it only allows 64K of output before it hangs
  # and shell.html is bigger than that!
  # See https://thraxil.org/users/anders/posts/2008/03/13/Subprocess-Hanging-PIPE-is-your-enemy/
  (path, file) = os.path.split(filename)
  if not path:
    path = None
  stdout = os.path.join(temp_dir, 'stdout')
  args = [settings_file, file]
  if expand_macros:
    args += ['--expandMacros']

  run_js(path_from_root('tools/preprocessor.js'), NODE_JS, args, True, stdout=open(stdout, 'w'), cwd=path)
  out = open(stdout, 'r').read()

  return out


# Generates a suitable fetch-worker.js script from the given input source JS file (which is an asm.js build output),
# and writes it out to location output_file. fetch-worker.js is the root entry point for a dedicated filesystem web
# worker in -s ASMFS=1 mode.
def make_fetch_worker(source_file, output_file):
  src = open(source_file, 'r').read()
  funcs_to_import = ['alignUp', '_emscripten_get_heap_size', '_emscripten_resize_heap', 'stringToUTF8', 'UTF8ToString', 'UTF8ArrayToString', 'intArrayFromString', 'lengthBytesUTF8', 'stringToUTF8Array', '_emscripten_is_main_browser_thread', '_emscripten_futex_wait', '_emscripten_get_sbrk_ptr']
  asm_funcs_to_import = ['_malloc', '_free', '_sbrk', '___pthread_mutex_lock', '___pthread_mutex_unlock', '_pthread_mutexattr_init', '_pthread_mutex_init']
  function_prologue = '''this.onerror = function(e) {
  console.error(e);
}

'''
  asm_start = src.find('// EMSCRIPTEN_START_ASM')
  for func in funcs_to_import + asm_funcs_to_import:
    loc = src.find('function ' + func + '(', asm_start if func in asm_funcs_to_import else 0)
    if loc == -1:
      exit_with_error('failed to find function %s!', func)
    end_loc = src.find('{', loc) + 1
    nesting_level = 1
    while nesting_level > 0:
      if src[end_loc] == '{':
        nesting_level += 1
      if src[end_loc] == '}':
        nesting_level -= 1
      end_loc += 1

    func_code = src[loc:end_loc]
    function_prologue = function_prologue + '\n' + func_code

  fetch_worker_src = function_prologue + '\n' + read_and_preprocess(path_from_root('src', 'fetch-worker.js'), expand_macros=True)
  open(output_file, 'w').write(fetch_worker_src)


# =================================================================================================
# End declarations.
# =================================================================================================

# Everything below this point is top level code that get run when importing this
# file.  TODO(sbc): We should try to reduce that amount we do here and instead
# have consumers explicitly call initialization functions.

# Emscripten configuration is done through the --em-config command line option
# or the EM_CONFIG environment variable. If the specified string value contains
# newline or semicolon-separated definitions, then these definitions will be
# used to configure Emscripten.  Otherwise, the string is understood to be a
# path to a settings file that contains the required definitions.
# The search order from the config file is as follows:
# 1. Specified on the command line (--em-config)
# 2. Specified via EM_CONFIG environment variable
# 3. Local .emscripten file, if found
# 4. Local .emscripten file, as used by `emsdk --embedded` (two levels above,
#    see below)
# 5. Fall back users home directory (~/.emscripten).

embedded_config = path_from_root('.emscripten')
# For compatibility with `emsdk --embedded` mode also look two levels up.  The
# layout of the emsdk puts emcc two levels below emsdk.  For exmaple:
#  - emsdk/upstream/emscripten/emcc
#  - emsdk/emscipten/1.38.31/emcc
# However `emsdk --embedded` stores the config file in the emsdk root.
# Without this check, when emcc is run from within the emsdk in embedded mode
# and the user forgets to first run `emsdk_env.sh` (which sets EM_CONFIG) emcc
# will not see any config file at all and fall back to creating a new/emtpy
# one.
# We could remove this special case if emsdk were to write its embedded config
# file into the emscripten directory itself.
# See: https://github.com/emscripten-core/emsdk/pull/367
emsdk_root = os.path.dirname(os.path.dirname(__rootpath__))
emsdk_embedded_config = os.path.join(emsdk_root, '.emscripten')

if '--em-config' in sys.argv:
  EM_CONFIG = sys.argv[sys.argv.index('--em-config') + 1]
  # And now remove it from sys.argv
  skip = False
  newargs = []
  for arg in sys.argv:
    if not skip and arg != '--em-config':
      newargs += [arg]
    elif arg == '--em-config':
      skip = True
    elif skip:
      skip = False
  sys.argv = newargs
  if not os.path.isfile(EM_CONFIG):
    if EM_CONFIG.startswith('-'):
      exit_with_error('Passed --em-config without an argument. Usage: --em-config /path/to/.emscripten or --em-config LLVM_ROOT=/path;...')
    if '=' not in EM_CONFIG:
      exit_with_error('File ' + EM_CONFIG + ' passed to --em-config does not exist!')
    else:
      EM_CONFIG = EM_CONFIG.replace(';', '\n') + '\n'
elif 'EM_CONFIG' in os.environ:
  EM_CONFIG = os.environ['EM_CONFIG']
elif os.path.exists(embedded_config):
  EM_CONFIG = embedded_config
elif os.path.exists(emsdk_embedded_config):
  EM_CONFIG = emsdk_embedded_config
else:
  EM_CONFIG = '~/.emscripten'

PYTHON = os.getenv('EM_PYTHON', sys.executable)
EMSCRIPTEN_ROOT = __rootpath__

# The following globals can be overridden by the config file.
# See parse_config_file below.
NODE_JS = None
BINARYEN_ROOT = None
EM_POPEN_WORKAROUND = None
SPIDERMONKEY_ENGINE = None
V8_ENGINE = None
LLVM_ROOT = None
LLVM_ADD_VERSION = None
CLANG_ADD_VERSION = None
CLOSURE_COMPILER = None
EMSCRIPTEN_NATIVE_OPTIMIZER = None
JAVA = None
JS_ENGINES = []
WASMER = None
WASMTIME = None
WASM_ENGINES = []
CACHE = None
FROZEN_CACHE = False

# Emscripten compiler spawns other processes, which can reimport shared.py, so
# make sure that those child processes get the same configuration file by
# setting it to the currently active environment.
os.environ['EM_CONFIG'] = EM_CONFIG

if '\n' in EM_CONFIG:
  CONFIG_FILE = None
  logger.debug('EM_CONFIG is specified inline without a file')
else:
  CONFIG_FILE = os.path.expanduser(EM_CONFIG)
  logger.debug('EM_CONFIG is located in ' + CONFIG_FILE)
  if not os.path.exists(CONFIG_FILE):
    generate_config(EM_CONFIG, first_time=True)
    sys.exit(0)

parse_config_file()
SPIDERMONKEY_ENGINE = fix_js_engine(SPIDERMONKEY_ENGINE, listify(SPIDERMONKEY_ENGINE))
NODE_JS = fix_js_engine(NODE_JS, listify(NODE_JS))
V8_ENGINE = fix_js_engine(V8_ENGINE, listify(V8_ENGINE))
JS_ENGINES = [listify(engine) for engine in JS_ENGINES]
WASM_ENGINES = [listify(engine) for engine in WASM_ENGINES]
if not CACHE:
  CACHE = path_from_root('cache')

# Install our replacement Popen handler if we are running on Windows to avoid
# python spawn process function.
# nb. This is by default disabled since it has the adverse effect of buffering
# up all logging messages, which makes builds look unresponsive (messages are
# printed only after the whole build finishes). Whether this workaround is
# needed seems to depend on how the host application that invokes emcc has set
# up its stdout and stderr.
if EM_POPEN_WORKAROUND and os.name == 'nt':
  logger.debug('Installing Popen workaround handler to avoid bug http://bugs.python.org/issue3905')
  Popen = WindowsPopen
else:
  Popen = subprocess.Popen

# Verbosity level control for any intermediate subprocess spawns from the compiler. Useful for internal debugging.
# 0: disabled.
# 1: Log stderr of subprocess spawns.
# 2: Log stdout and stderr of subprocess spawns. Print out subprocess commands that were executed.
# 3: Log stdout and stderr, and pass VERBOSE=1 to CMake configure steps.
EM_BUILD_VERBOSE = int(os.getenv('EM_BUILD_VERBOSE', '0'))

set_version_globals()

# For the Emscripten-specific WASM metadata section, follows semver, changes
# whenever metadata section changes structure.
# NB: major version 0 implies no compatibility
# NB: when changing the metadata format, we should only append new fields, not
#     reorder, modify, or remove existing ones.
EMSCRIPTEN_METADATA_MAJOR, EMSCRIPTEN_METADATA_MINOR = (0, 3)
# For the JS/WASM ABI, specifies the minimum ABI version required of
# the WASM runtime implementation by the generated WASM binary. It follows
# semver and changes whenever C types change size/signedness or
# syscalls change signature. By semver, the maximum ABI version is
# implied to be less than (EMSCRIPTEN_ABI_MAJOR + 1, 0). On an ABI
# change, increment EMSCRIPTEN_ABI_MINOR if EMSCRIPTEN_ABI_MAJOR == 0
# or the ABI change is backwards compatible, otherwise increment
# EMSCRIPTEN_ABI_MAJOR and set EMSCRIPTEN_ABI_MINOR = 0.
EMSCRIPTEN_ABI_MAJOR, EMSCRIPTEN_ABI_MINOR = (0, 25)

# Tools/paths
if LLVM_ADD_VERSION is None:
  LLVM_ADD_VERSION = os.getenv('LLVM_ADD_VERSION')

if CLANG_ADD_VERSION is None:
  CLANG_ADD_VERSION = os.getenv('CLANG_ADD_VERSION')

CLANG_CC = os.path.expanduser(build_clang_tool_path(exe_suffix('clang')))
CLANG_CXX = os.path.expanduser(build_clang_tool_path(exe_suffix('clang++')))
LLVM_LINK = build_llvm_tool_path(exe_suffix('llvm-link'))
LLVM_AR = build_llvm_tool_path(exe_suffix('llvm-ar'))
LLVM_RANLIB = build_llvm_tool_path(exe_suffix('llvm-ranlib'))
LLVM_OPT = os.path.expanduser(build_llvm_tool_path(exe_suffix('opt')))
LLVM_AS = os.path.expanduser(build_llvm_tool_path(exe_suffix('llvm-as')))
LLVM_DIS = os.path.expanduser(build_llvm_tool_path(exe_suffix('llvm-dis')))
LLVM_NM = os.path.expanduser(build_llvm_tool_path(exe_suffix('llvm-nm')))
LLVM_INTERPRETER = os.path.expanduser(build_llvm_tool_path(exe_suffix('lli')))
LLVM_COMPILER = os.path.expanduser(build_llvm_tool_path(exe_suffix('llc')))
LLVM_DWARFDUMP = os.path.expanduser(build_llvm_tool_path(exe_suffix('llvm-dwarfdump')))
LLVM_OBJCOPY = os.path.expanduser(build_llvm_tool_path(exe_suffix('llvm-objcopy')))
WASM_LD = os.path.expanduser(build_llvm_tool_path(exe_suffix('wasm-ld')))

EMSCRIPTEN = path_from_root('emscripten.py')
EMCC = path_from_root('emcc.py')
EMXX = path_from_root('em++.py')
EMAR = path_from_root('emar.py')
EMRANLIB = path_from_root('emranlib.py')
EMCONFIG = path_from_root('em-config.py')
EMLINK = path_from_root('emlink.py')
EMCONFIGURE = path_from_root('emconfigure.py')
EMMAKE = path_from_root('emmake.py')
AUTODEBUGGER = path_from_root('tools', 'autodebugger.py')
EXEC_LLVM = path_from_root('tools', 'exec_llvm.py')
FILE_PACKAGER = path_from_root('tools', 'file_packager.py')

apply_configuration()

# Target choice.
ASM_JS_TARGET = 'asmjs-unknown-emscripten'
WASM_TARGET = 'wasm32-unknown-emscripten'

check_vanilla()

# Engine tweaks
if SPIDERMONKEY_ENGINE:
  new_spidermonkey = SPIDERMONKEY_ENGINE
  if '-w' not in str(new_spidermonkey):
    new_spidermonkey += ['-w']
  SPIDERMONKEY_ENGINE = fix_js_engine(SPIDERMONKEY_ENGINE, new_spidermonkey)

Settings = SettingsManager()
verify_settings()

PRINT_STAGES = int(os.getenv('EMCC_VERBOSE', '0'))

# compatibility with existing emcc, etc. scripts
Cache = cache.Cache(CACHE)
chunkify = cache.chunkify
