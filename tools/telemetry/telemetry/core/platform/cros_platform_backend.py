# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.platform import platform_backend
from telemetry.core.platform import proc_util


class CrosPlatformBackend(platform_backend.PlatformBackend):

  def __init__(self, cri):
    super(CrosPlatformBackend, self).__init__()
    self._cri = cri

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

  def _RunCommand(self, args):
    return self._cri.RunCmdOnDevice(args)[0]

  def _GetFileContents(self, filename):
    try:
      return self._cri.RunCmdOnDevice(['cat', filename])[0]
    except AssertionError:
      return ''

  def GetSystemCommitCharge(self):
    meminfo_contents = self._GetFileContents('/proc/meminfo')
    return proc_util.GetSystemCommitCharge(meminfo_contents)

  def GetMemoryStats(self, pid):
    status = self._GetFileContents('/proc/%s/status' % pid)
    stats = self._GetFileContents('/proc/%s/stat' % pid).split()
    return proc_util.GetMemoryStats(status, stats)

  def GetIOStats(self, pid):
    # There is no '/proc/<pid>/io' file on CrOS platforms
    # Returns empty dict as it does in PlatformBackend.
    return {}

  def GetOSName(self):
    return 'chromeos'

  def GetChildPids(self, pid):
    """Returns a list of child pids of |pid|."""
    all_process_info = self._cri.ListProcesses()
    processes = []
    for pid, _, ppid, state in all_process_info:
      processes.append((pid, ppid, state))
    return proc_util.GetChildPids(processes, pid)

  def GetCommandLine(self, pid):
    command = self._GetPsOutput(['command'], pid)
    return command[0] if command else None

  def CanFlushIndividualFilesFromSystemCache(self):
    return True

  def FlushEntireSystemCache(self):
    raise NotImplementedError()

  def FlushSystemCacheForDirectory(self, directory, ignoring=None):
    raise NotImplementedError()
