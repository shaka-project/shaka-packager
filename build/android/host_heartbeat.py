#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sends a heart beat pulse to the currently online Android devices.
This heart beat lets the devices know that they are connected to a host.
"""

import os
import sys
import time

from pylib import android_commands

PULSE_PERIOD = 20

def main():
  while True:
    try:
      devices = android_commands.GetAttachedDevices()
      for device in devices:
        android_cmd = android_commands.AndroidCommands(device)
        android_cmd.RunShellCommand('touch /sdcard/host_heartbeat')
    except:
      # Keep the heatbeat running bypassing all errors.
      pass
    time.sleep(PULSE_PERIOD)


if __name__ == '__main__':
  sys.exit(main())
