# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core.platform import posix_platform_backend


class PosixPlatformBackendTest(unittest.TestCase):

  def _GetChildPids(self, mock_ps_output, pid):
    class TestBackend(posix_platform_backend.PosixPlatformBackend):

      # pylint: disable=W0223

      def _GetPsOutput(self, columns, pid=None):
        return mock_ps_output

    return TestBackend().GetChildPids(pid)

  def testGetChildPidsWithGrandChildren(self):
    lines = ['1 0 S', '2 1 R', '3 2 S', '4 1 R', '5 4 R']
    result = self._GetChildPids(lines, 1)
    self.assertEquals(set(result), set([2, 3, 4, 5]))

  def testGetChildPidsWithNoSuchPid(self):
    lines = ['1 0 S', '2 1 R', '3 2 S', '4 1 R', '5 4 R']
    result = self._GetChildPids(lines, 6)
    self.assertEquals(set(result), set())

  def testGetChildPidsWithZombieChildren(self):
    lines = ['1 0 S', '2 1 R', '3 2 S', '4 1 R', '5 4 Z']
    result = self._GetChildPids(lines, 1)
    self.assertEquals(set(result), set([2, 3, 4]))
