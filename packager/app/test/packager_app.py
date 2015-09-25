#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"""Test wrapper for the sample packager binary."""


import os
import subprocess

import test_env


class PackagerApp(object):
  """Main integration class for testing the packager binary."""

  def __init__(self, build_type='Debug'):
    self.build_dir = os.path.join(test_env.SRC_DIR, 'out', build_type)
    self.binary = os.path.join(self.build_dir, 'packager')

  def BuildSrc(self, clean=False):
    if clean:
      return subprocess.call(['ninja', '-C', self.build_dir, '-t', 'clean'])
    return subprocess.call(['ninja', '-C', self.build_dir])

  def DumpStreamInfo(self, stream):
    input_str = 'input=%s' % stream
    cmd = [self.binary, input_str, '--dump_stream_info']
    return subprocess.check_output(cmd)

  def Package(self, streams, flags=None):
    if flags is None:
      flags = []
    cmd = [self.binary]
    cmd.extend(streams)
    cmd.extend(flags)
    assert 0 == subprocess.call(cmd)
