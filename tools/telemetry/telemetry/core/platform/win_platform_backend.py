# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import ctypes
import os
import re
import subprocess
try:
  import pywintypes  # pylint: disable=F0401
  import win32api  # pylint: disable=F0401
  import win32con  # pylint: disable=F0401
  import win32process  # pylint: disable=F0401
except ImportError:
  pywintypes = None
  win32api = None
  win32con = None
  win32process = None

from telemetry.core.platform import desktop_platform_backend


class WinPlatformBackend(desktop_platform_backend.DesktopPlatformBackend):
  def _GetProcessHandle(self, pid):
    mask = (win32con.PROCESS_QUERY_INFORMATION |
            win32con.PROCESS_VM_READ)
    return win32api.OpenProcess(mask, False, pid)

  # pylint: disable=W0613
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
    class PerformanceInfo(ctypes.Structure):
      """Struct for GetPerformanceInfo() call
      http://msdn.microsoft.com/en-us/library/ms683210
      """
      _fields_ = [('size', ctypes.c_ulong),
                  ('CommitTotal', ctypes.c_size_t),
                  ('CommitLimit', ctypes.c_size_t),
                  ('CommitPeak', ctypes.c_size_t),
                  ('PhysicalTotal', ctypes.c_size_t),
                  ('PhysicalAvailable', ctypes.c_size_t),
                  ('SystemCache', ctypes.c_size_t),
                  ('KernelTotal', ctypes.c_size_t),
                  ('KernelPaged', ctypes.c_size_t),
                  ('KernelNonpaged', ctypes.c_size_t),
                  ('PageSize', ctypes.c_size_t),
                  ('HandleCount', ctypes.c_ulong),
                  ('ProcessCount', ctypes.c_ulong),
                  ('ThreadCount', ctypes.c_ulong)]

      def __init__(self):
        self.size = ctypes.sizeof(self)
        super(PerformanceInfo, self).__init__()

    performance_info = PerformanceInfo()
    ctypes.windll.psapi.GetPerformanceInfo(
        ctypes.byref(performance_info), performance_info.size)
    return performance_info.CommitTotal * performance_info.PageSize / 1024

  def GetMemoryStats(self, pid):
    try:
      memory_info = win32process.GetProcessMemoryInfo(
          self._GetProcessHandle(pid))
    except pywintypes.error, e:
      errcode = e[0]
      if errcode == 87:  # The process may have been closed.
        return {}
      raise
    return {'VM': memory_info['PagefileUsage'],
            'VMPeak': memory_info['PeakPagefileUsage'],
            'WorkingSetSize': memory_info['WorkingSetSize'],
            'WorkingSetSizePeak': memory_info['PeakWorkingSetSize']}

  def GetIOStats(self, pid):
    try:
      io_stats = win32process.GetProcessIoCounters(
          self._GetProcessHandle(pid))
    except pywintypes.error, e:
      errcode = e[0]
      if errcode == 87:  # The process may have been closed.
        return {}
      raise
    return {'ReadOperationCount': io_stats['ReadOperationCount'],
            'WriteOperationCount': io_stats['WriteOperationCount'],
            'ReadTransferCount': io_stats['ReadTransferCount'],
            'WriteTransferCount': io_stats['WriteTransferCount']}

  def GetChildPids(self, pid):
    """Retunds a list of child pids of |pid|."""
    creation_ppid_pid_list = subprocess.Popen(
          ['wmic', 'process', 'get', 'CreationDate,ParentProcessId,ProcessId',
           '/format:csv'],
          stdout=subprocess.PIPE).communicate()[0]
    ppid_map = collections.defaultdict(list)
    creation_map = {}
    # [3:] To skip 2 blank lines and header.
    for creation_ppid_pid in creation_ppid_pid_list.splitlines()[3:]:
      if not creation_ppid_pid:
        continue
      _, creation, curr_ppid, curr_pid = creation_ppid_pid.split(',')
      ppid_map[int(curr_ppid)].append(int(curr_pid))
      if creation:
        creation_map[int(curr_pid)] = float(re.split('[+-]', creation)[0])

    def _InnerGetChildPids(pid):
      if not pid or pid not in ppid_map:
        return []
      ret = [p for p in ppid_map[pid] if creation_map[p] >= creation_map[pid]]
      for child in ret:
        if child == pid:
          continue
        ret.extend(_InnerGetChildPids(child))
      return ret

    return _InnerGetChildPids(pid)

  def GetCommandLine(self, pid):
    command_pid_list = subprocess.Popen(
          ['wmic', 'process', 'get', 'CommandLine,ProcessId',
           '/format:csv'],
          stdout=subprocess.PIPE).communicate()[0]
    # [3:] To skip 2 blank lines and header.
    for command_pid in command_pid_list.splitlines()[3:]:
      if not command_pid:
        continue
      parts = command_pid.split(',')
      curr_pid = parts[-1]
      if pid == int(curr_pid):
        command = ','.join(parts[1:-1])
        return command
    raise Exception('Could not get command line for %d' % pid)

  def GetOSName(self):
    return 'win'

  def GetOSVersionName(self):
    os_version = os.uname()[2]

    if os_version.startswith('5.1.'):
      return 'xp'
    if os_version.startswith('6.0.'):
      return 'vista'
    if os_version.startswith('6.1.'):
      return 'win7'
    if os_version.startswith('6.2.'):
      return 'win8'

  def CanFlushIndividualFilesFromSystemCache(self):
    return True

  def GetFlushUtilityName(self):
    return 'clear_system_cache.exe'
