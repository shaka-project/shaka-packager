#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Test wrapper for the sample packager binaries."""

import logging
import os
import platform
import subprocess

import test_env


class PackagerApp(object):
  """Main integration class for testing the packager binaries."""

  def __init__(self):
    self.packager_binary = os.path.join(test_env.SCRIPT_DIR,
                                        self._GetBinaryName('packager'))
    self.mpd_generator_binary = os.path.join(
        test_env.SCRIPT_DIR, self._GetBinaryName('mpd_generator'))
    # Set this to empty for now in case GetCommandLine() is called before
    # Package().
    self.packaging_command_line = ''
    assert os.path.exists(
        self.packager_binary), ('Please run from output directory, '
                                'e.g. out/Debug/packager_test.py')

  def _GetBinaryName(self, name):
    if platform.system() == 'Windows':
      name += '.exe'
    return name

  def GetEnv(self):
    env = os.environ.copy()
    if (platform.system() == 'Darwin' and
        test_env.options.libpackager_type == 'shared_library'):
      env['DYLD_FALLBACK_LIBRARY_PATH'] = test_env.SCRIPT_DIR
    return env

  def DumpStreamInfo(self, stream):
    input_str = 'input=%s' % stream
    cmd = [self.packager_binary, input_str, '--dump_stream_info']
    return subprocess.check_output(cmd, env=self.GetEnv()).decode()

  def Version(self):
    return subprocess.check_output(
        [self.packager_binary, '--version'], env=self.GetEnv()).decode()

  def Package(self, streams, flags=None):
    """Executes packager command."""
    if flags is None:
      flags = []
    cmd = [self.packager_binary]
    cmd.extend(streams)
    cmd.extend(flags)

    if test_env.options.v:
      cmd.extend(['--v=%s' % test_env.options.v])
    if test_env.options.vmodule:
      cmd.extend(['--vmodule="%s"' % test_env.options.vmodule])

    # Put single-quotes around each entry so that things like '$' signs in
    # segment templates won't be interpreted as shell variables.
    self.packaging_command_line = ' '.join(["'%s'" % entry for entry in cmd])
    packaging_result = subprocess.call(cmd, env=self.GetEnv())
    if packaging_result != 0:
      logging.error('%s returned non-0 status', self.packaging_command_line)
    return packaging_result

  def GetCommandLine(self):
    return self.packaging_command_line

  def MpdGenerator(self, flags):
    """Executes MPD Generator command."""
    cmd = [self.mpd_generator_binary]
    cmd.extend(flags)

    if test_env.options.v:
      cmd.extend(['--v=%s' % test_env.options.v])
    if test_env.options.vmodule:
      cmd.extend(['--vmodule="%s"' % test_env.options.vmodule])

    return subprocess.call(cmd, env=self.GetEnv())
