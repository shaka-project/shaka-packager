# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from telemetry.core import util
from telemetry.core.platform import platform_backend


class DesktopPlatformBackend(platform_backend.PlatformBackend):

  # This is an abstract class. It is OK to have abstract methods.
  # pylint: disable=W0223

  def GetFlushUtilityName(self):
    return NotImplementedError()

  def _FindNewestFlushUtility(self):
    flush_command = None
    flush_command_mtime = 0

    chrome_root = util.GetChromiumSrcDir()
    for build_dir, build_type in util.GetBuildDirectories():
      candidate = os.path.join(chrome_root, build_dir, build_type,
                               self.GetFlushUtilityName())
      if os.access(candidate, os.X_OK):
        candidate_mtime = os.stat(candidate).st_mtime
        if candidate_mtime > flush_command_mtime:
          flush_command = candidate
          flush_command_mtime = candidate_mtime

    return flush_command

  def FlushSystemCacheForDirectory(self, directory, ignoring=None):
    assert directory and os.path.exists(directory), \
        'Target directory %s must exist' % directory
    flush_command = self._FindNewestFlushUtility()
    assert flush_command, \
        'You must build %s first' % self.GetFlushUtilityName()

    args = [flush_command, '--recurse']
    directory_contents = os.listdir(directory)
    for item in directory_contents:
      if not ignoring or item not in ignoring:
        args.append(os.path.join(directory, item))

    if len(args) < 3:
      return

    p = subprocess.Popen(args)
    p.wait()
    assert p.returncode == 0, 'Failed to flush system cache'
