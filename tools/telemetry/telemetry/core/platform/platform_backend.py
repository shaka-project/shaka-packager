# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class PlatformBackend(object):
  def IsRawDisplayFrameRateSupported(self):
    return False

  # pylint: disable=W0613
  def StartRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def StopRawDisplayFrameRateMeasurement(self):
    raise NotImplementedError()

  def GetRawDisplayFrameRateMeasurements(self):
    raise NotImplementedError()

  def SetFullPerformanceModeEnabled(self, enabled):  # pylint: disable=W0613
    pass

  def CanMonitorThermalThrottling(self):
    return False

  def IsThermallyThrottled(self):
    raise NotImplementedError()

  def HasBeenThermallyThrottled(self):
    raise NotImplementedError()

  def GetSystemCommitCharge(self):
    raise NotImplementedError()

  def GetMemoryStats(self, pid):  # pylint: disable=W0613
    return {}

  def GetIOStats(self, pid):  # pylint: disable=W0613
    return {}

  def GetChildPids(self, pid):  # pylint: disable=W0613
    raise NotImplementedError()

  def GetCommandLine(self, pid):
    raise NotImplementedError()

  def GetOSName(self):
    raise NotImplementedError()

  def GetOSVersionName(self):
    return None

  def CanFlushIndividualFilesFromSystemCache(self):
    raise NotImplementedError()

  def FlushEntireSystemCache(self):
    raise NotImplementedError()

  def FlushSystemCacheForDirectory(self, directory, ignoring=None):
    raise NotImplementedError()
