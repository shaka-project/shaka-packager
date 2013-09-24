# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

class ThermalThrottle(object):
  """Class to detect and track thermal throttling

  Usage:
    Wait for IsThrottled() to be False before running test
    After running test call HasBeenThrottled() to find out if the
    test run was affected by thermal throttling.

    Currently assumes an OMap device.
  """
  def __init__(self, adb):
    self._adb = adb
    self._throttled = False


  def HasBeenThrottled(self):
    """ True if there has been any throttling since the last call to
        HasBeenThrottled or IsThrottled
    """
    return self._ReadLog()

  def IsThrottled(self):
    """True if currently throttled"""
    self._ReadLog()
    return self._throttled

  def _ReadLog(self):
    has_been_throttled = False
    serial_number = self._adb.Adb().GetSerialNumber()
    log = self._adb.RunShellCommand('dmesg -c')
    degree_symbol = unichr(0x00B0)
    for line in log:
      if 'omap_thermal_throttle' in line:
        if not self._throttled:
          logging.warning('>>> Device %s Thermally Throttled', serial_number)
        self._throttled = True
        has_been_throttled = True
      if 'omap_thermal_unthrottle' in line:
        if self._throttled:
          logging.warning('>>> Device %s Thermally Unthrottled', serial_number)
        self._throttled = False
        has_been_throttled = True
      if 'throttle_delayed_work_fn' in line:
        temp = float([s for s in line.split() if s.isdigit()][0]) / 1000.0
        logging.info(u' Device %s Thermally Thottled at %3.1f%sC',
                     serial_number, temp, degree_symbol)

    if logging.getLogger().isEnabledFor(logging.DEBUG):
      # Print temperature of CPU SoC.
      omap_temp_file = ('/sys/devices/platform/omap/omap_temp_sensor.0/'
                        'temperature')
      if self._adb.FileExistsOnDevice(omap_temp_file):
        tempdata = self._adb.GetFileContents(omap_temp_file)
        temp = float(tempdata[0]) / 1000.0
        logging.debug(u'Current OMAP Temperature of %s = %3.1f%sC',
                      serial_number, temp, degree_symbol)

      # Print temperature of battery, to give a system temperature
      dumpsys_log = self._adb.RunShellCommand('dumpsys battery')
      for line in dumpsys_log:
        if 'temperature' in line:
          btemp = float([s for s in line.split() if s.isdigit()][0]) / 10.0
          logging.debug(u'Current battery temperature of %s = %3.1f%sC',
                        serial_number, btemp, degree_symbol)

    return has_been_throttled
