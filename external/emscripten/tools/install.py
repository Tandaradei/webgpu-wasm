#!/usr/bin/env python3
# Copyright 2020 The Emscripten Authors.  All rights reserved.
# Emscripten is available under two separate licenses, the MIT license and the
# University of Illinois/NCSA Open Source License.  Both these licenses can be
# found in the LICENSE file.

"""Install the parts of emscripten needed for end users. This works like
a traditional `make dist` target but is written in python so it can be portable
and run on non-unix platforms (basically windows).
"""

from __future__ import print_function

import argparse
import fnmatch
import logging
import os
import shutil
import subprocess
import sys

EXCLUDES = '''
tests/third_party
site
node_modules
Makefile
.git
'''.split()

EXCLUDE_PATTERNS = '''
*.pyc
.*
'''.split()

EXCLUDE_DIRS = '''
__pycache__
'''.split()


def add_revision_file(target):
  # text=True would be better than encoding here, but it's only supported in 3.7+
  git_hash = subprocess.check_output(['git', 'rev-parse', 'HEAD'], encoding='utf-8').strip()
  with open(os.path.join(target, 'emscripten-revision.txt'), 'w') as f:
    f.write(git_hash + '\n')


def copy_emscripten(target):
  script_dir = os.path.dirname(os.path.abspath(__file__))
  emscripten_root = os.path.dirname(script_dir)
  os.chdir(emscripten_root)
  for root, dirs, files in os.walk('.'):
    # Handle the case were the target directory is underneath emscripten_root
    if os.path.abspath(root) == target:
      continue

    remove_dirs = []
    for d in dirs:
      if d in EXCLUDE_DIRS:
        remove_dirs.append(d)
        continue
      fulldir = os.path.normpath(os.path.join(root, d))
      if fulldir in EXCLUDES:
        remove_dirs.append(d)
        continue
      os.makedirs(os.path.join(target, fulldir))

    for d in remove_dirs:
      # Prevent recursion in excluded dirs
      logging.debug('skipping dir: ' + os.path.join(root, d))
      dirs.remove(d)

    for f in files:
      for pat in EXCLUDE_PATTERNS:
        if fnmatch.fnmatch(f, pat):
          logging.debug('skipping file: ' + os.path.join(root, f))
          continue
      full = os.path.normpath(os.path.join(root, f))
      if full in EXCLUDES:
        continue
      shutil.copy2(full, os.path.join(target, root, f), follow_symlinks=False)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('target', help='target directory')
  args = parser.parse_args()
  target = os.path.abspath(args.target)
  if os.path.exists(target):
    print('target directory already exists: %s' % target)
    return 1
  os.makedirs(target)
  copy_emscripten(target)
  add_revision_file(target)
  return 0


if __name__ == '__main__':
  sys.exit(main())
