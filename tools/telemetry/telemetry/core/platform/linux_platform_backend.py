# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess

from telemetry.core.platform import posix_platform_backend
from telemetry.core.platform import proc_util


class LinuxPlatformBackend(posix_platform_backend.PosixPlatformBackend):

  def StartRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def StopRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def GetRawDisplayFrameRateMeasurements(self):
    raise NotImplementedError()

  def IsThermallyThrottled(self):
    raise NotImplementedError()

  def HasBeenThermallyThrottled(self):
    raise NotImplementedError()

  def GetSystemCommitCharge(self):
    meminfo_contents = self._GetFileContents('/proc/meminfo')
    return proc_util.GetSystemCommitCharge(meminfo_contents)

  def GetMemoryStats(self, pid):
    status = self._GetFileContents('/proc/%s/status' % pid)
    stats = self._GetFileContents('/proc/%s/stat' % pid).split()
    return proc_util.GetMemoryStats(status, stats)

  def GetIOStats(self, pid):
    io_contents = self._GetFileContents('/proc/%s/io' % pid)
    return proc_util.GetIOStats(io_contents)

  def GetOSName(self):
    return 'linux'

  def CanFlushIndividualFilesFromSystemCache(self):
    return True

  def FlushEntireSystemCache(self):
    p = subprocess.Popen(['/sbin/sysctl', '-w', 'vm.drop_caches=3'])
    p.wait()
    assert p.returncode == 0, 'Failed to flush system cache'
