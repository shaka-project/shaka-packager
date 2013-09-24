#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sets environment variables needed to run a chromium unit test."""

import os
import stat
import subprocess
import sys

# This is hardcoded to be src/ relative to this script.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CHROME_SANDBOX_ENV = 'CHROME_DEVEL_SANDBOX'
CHROME_SANDBOX_PATH = '/opt/chromium/chrome_sandbox'


def should_enable_sandbox(sandbox_path):
  """Return a boolean indicating that the current slave is capable of using the
  sandbox and should enable it.  This should return True iff the slave is a
  Linux host with the sandbox file present and configured correctly."""
  if not (sys.platform.startswith('linux') and
          os.path.exists(sandbox_path)):
    return False
  sandbox_stat = os.stat(sandbox_path)
  if ((sandbox_stat.st_mode & stat.S_ISUID) and
      (sandbox_stat.st_mode & stat.S_IRUSR) and
      (sandbox_stat.st_mode & stat.S_IXUSR) and
      (sandbox_stat.st_uid == 0)):
    return True
  return False


def enable_sandbox_if_required(env, verbose=False):
  """Checks enables the sandbox if it is required, otherwise it disables it."""
  chrome_sandbox_path = env.get(CHROME_SANDBOX_ENV, CHROME_SANDBOX_PATH)

  if should_enable_sandbox(chrome_sandbox_path):
    if verbose:
      print 'Enabling sandbox. Setting environment variable:'
      print '  %s="%s"' % (CHROME_SANDBOX_ENV, chrome_sandbox_path)
    env[CHROME_SANDBOX_ENV] = chrome_sandbox_path
  else:
    if verbose:
      print 'Sandbox not properly installed. Unsetting:'
      print '  %s' % CHROME_SANDBOX_ENV
    # The variable should be removed from the environment, making
    # the variable empty silently disables the sandbox.
    if env.get(CHROME_SANDBOX_ENV):
      env.pop(CHROME_SANDBOX_ENV)


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def run_executable(cmd, env):
  """Runs an executable with:
    - environment variable CR_SOURCE_ROOT set to the root directory.
    - environment variable LANGUAGE to en_US.UTF-8.
    - environment variable CHROME_DEVEL_SANDBOX set if need
    - Reuses sys.executable automatically.
  """
  # Many tests assume a English interface...
  env['LANG'] = 'en_US.UTF-8'
  # Used by base/base_paths_linux.cc as an override. Just make sure the default
  # logic is used.
  env.pop('CR_SOURCE_ROOT', None)
  enable_sandbox_if_required(env)
  # Ensure paths are correctly separated on windows.
  cmd[0] = cmd[0].replace('/', os.path.sep)
  cmd = fix_python_path(cmd)
  try:
    return subprocess.call(cmd, env=env)
  except OSError:
    print >> sys.stderr, 'Failed to start %s' % cmd
    raise


def main():
  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == '__main__':
  sys.exit(main())
