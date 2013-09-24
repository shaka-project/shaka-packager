# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from telemetry.core.platform import linux_platform_backend
from telemetry.core.platform import mac_platform_backend
from telemetry.core.platform import win_platform_backend

class Platform(object):
  """The platform that the target browser is running on.

  Provides a limited interface to interact with the platform itself, where
  possible. It's important to note that platforms may not provide a specific
  API, so check with IsFooBar() for availability.
  """
  def __init__(self, platform_backend):
    self._platform_backend = platform_backend

  def IsRawDisplayFrameRateSupported(self):
    """Platforms may be able to collect GL surface stats."""
    return self._platform_backend.IsRawDisplayFrameRateSupported()

  def StartRawDisplayFrameRateMeasurement(self):
    """Start measuring GL surface stats."""
    return self._platform_backend.StartRawDisplayFrameRateMeasurement()

  def StopRawDisplayFrameRateMeasurement(self):
    """Stop measuring GL surface stats."""
    return self._platform_backend.StopRawDisplayFrameRateMeasurement()

  class RawDisplayFrameRateMeasurement(object):
    def __init__(self, name, value, unit):
      self._name = name
      self._value = value
      self._unit = unit

    @property
    def name(self):
      return self._name

    @property
    def value(self):
      return self._value

    @property
    def unit(self):
      return self._unit

  def GetRawDisplayFrameRateMeasurements(self):
    """Returns a list of RawDisplayFrameRateMeasurement."""
    return self._platform_backend.GetRawDisplayFrameRateMeasurements()

  def SetFullPerformanceModeEnabled(self, enabled):
    """Platforms may tweak their CPU governor, system status, etc.

    Most platforms can operate in a battery saving mode. While good for battery
    life, this can cause confusing performance results and add noise. Turning
    full performance mode on disables these features, which is useful for
    performance testing.
    """
    return self._platform_backend.SetFullPerformanceModeEnabled(enabled)

  def CanMonitorThermalThrottling(self):
    """Platforms may be able to detect thermal throttling.

    Some fan-less computers go into a reduced performance mode when their heat
    exceeds a certain threshold. Performance tests in particular should use this
    API to detect if this has happened and interpret results accordingly.
    """
    return self._platform_backend.CanMonitorThermalThrottling()

  def IsThermallyThrottled(self):
    """Returns True if the device is currently thermally throttled."""
    return self._platform_backend.IsThermallyThrottled()

  def HasBeenThermallyThrottled(self):
    """Returns True if the device has been thermally throttled."""
    return self._platform_backend.HasBeenThermallyThrottled()

  def GetOSName(self):
    """Returns a string description of the Platform OS.

    Examples: WIN, MAC, LINUX, CHROMEOS"""
    return self._platform_backend.GetOSName()

  def GetOSVersionName(self):
    """Returns a string description of the Platform OS version.

    Examples: VISTA, WIN7, LION, MOUNTAINLION"""
    return self._platform_backend.GetOSVersionName()

  def CanFlushIndividualFilesFromSystemCache(self):
    """Returns true if the disk cache can be flushed for specific files."""
    return self._platform_backend.CanFlushIndividualFilesFromSystemCache()

  def FlushEntireSystemCache(self):
    """Flushes the OS's file cache completely.

    This function may require root or administrator access."""
    return self._platform_backend.FlushEntireSystemCache()

  def FlushSystemCacheForDirectory(self, directory, ignoring=None):
    """Flushes the OS's file cache for the specified directory.

    Any files or directories inside |directory| matching a name in the
    |ignoring| list will be skipped.

    This function does not require root or administrator access."""
    return self._platform_backend.FlushSystemCacheForDirectory(
        directory, ignoring=ignoring)


def CreatePlatformBackendForCurrentOS():
  if sys.platform.startswith('linux'):
    return linux_platform_backend.LinuxPlatformBackend()
  elif sys.platform == 'darwin':
    return mac_platform_backend.MacPlatformBackend()
  elif sys.platform == 'win32':
    return win_platform_backend.WinPlatformBackend()
  else:
    raise NotImplementedError()
