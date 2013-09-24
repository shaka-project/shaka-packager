#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Gets and writes the configurations of the attached devices.

This configuration is used by later build steps to determine which devices to
install to and what needs to be installed to those devices.
"""

import logging
import optparse
import os
import subprocess
import sys

from util import build_utils
from util import build_device

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib.utils import apk_helper


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--stamp', action='store')
  parser.add_option('--output', action='store')
  options, _ = parser.parse_args(argv)

  devices = build_device.GetAttachedDevices()

  device_configurations = []
  for d in devices:
    configuration, is_online, has_root = (
        build_device.GetConfigurationForDevice(d))

    if not is_online:
      build_utils.PrintBigWarning(
          '%s is not online. Skipping managed install for this device. '
          'Try rebooting the device to fix this warning.' % d)
      continue

    if not has_root:
      build_utils.PrintBigWarning(
          '"adb root" failed on device: %s\n'
          'Skipping managed install for this device.'
          % configuration['description'])
      continue

    device_configurations.append(configuration)

  if len(device_configurations) == 0:
    build_utils.PrintBigWarning(
        'No valid devices attached. Skipping managed install steps.')
  elif len(devices) > 1:
    # Note that this checks len(devices) and not len(device_configurations).
    # This way, any time there are multiple devices attached it is
    # explicitly stated which device we will install things to even if all but
    # one device were rejected for other reasons (e.g. two devices attached,
    # one w/o root).
    build_utils.PrintBigWarning(
        'Multiple devices attached. '
        'Installing to the preferred device: '
        '%(id)s (%(description)s)' % (device_configurations[0]))


  build_device.WriteConfigurations(device_configurations, options.output)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
