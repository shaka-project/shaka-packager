# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
try:
  import resource  # pylint: disable=F0401
except ImportError:
  resource = None  # Not available on all platforms

from telemetry.core.platform import posix_platform_backend


class MacPlatformBackend(posix_platform_backend.PosixPlatformBackend):

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
    vm_stat = self._RunCommand(['vm_stat'])
    for stat in vm_stat.splitlines():
      key, value = stat.split(':')
      if key == 'Pages active':
        pages_active = int(value.strip()[:-1])  # Strip trailing '.'
        return pages_active * resource.getpagesize() / 1024
    return 0

  def GetMemoryStats(self, pid):
    rss_vsz = self._GetPsOutput(['rss', 'vsz'], pid)
    if rss_vsz:
      rss, vsz = rss_vsz[0].split()
      return {'VM': 1024 * int(vsz),
              'WorkingSetSize': 1024 * int(rss)}
    return {}

  def GetOSName(self):
    return 'mac'

  def GetOSVersionName(self):
    os_version = os.uname()[2]

    if os_version.startswith('9.'):
      return 'leopard'
    if os_version.startswith('10.'):
      return 'snowleopard'
    if os_version.startswith('11.'):
      return 'lion'
    if os_version.startswith('12.'):
      return 'mountainlion'
    #if os_version.startswith('13.'):
    #  return 'mavericks'

  def CanFlushIndividualFilesFromSystemCache(self):
    return False

  def FlushEntireSystemCache(self):
    p = subprocess.Popen(['purge'])
    p.wait()
    assert p.returncode == 0, 'Failed to flush system cache'
