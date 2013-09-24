#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launches Android Virtual Devices with a set configuration for testing Chrome.

The script will launch a specified number of Android Virtual Devices (AVD's).
"""


import install_emulator_deps
import logging
import optparse
import os
import subprocess
import sys

from pylib import constants
from pylib.utils import emulator


def main(argv):
  # ANDROID_SDK_ROOT needs to be set to the location of the SDK used to launch
  # the emulator to find the system images upon launch.
  emulator_sdk = os.path.join(constants.EMULATOR_SDK_ROOT,
                              'android_tools', 'sdk')
  os.environ['ANDROID_SDK_ROOT'] = emulator_sdk

  opt_parser = optparse.OptionParser(description='AVD script.')
  opt_parser.add_option('-n', '--num', dest='emulator_count',
                        help='Number of emulators to launch (default is 1).',
                        type='int', default='1')
  opt_parser.add_option('--abi', default='x86',
                        help='Platform of emulators to launch (x86 default).')

  options, _ = opt_parser.parse_args(argv[1:])

  logging.basicConfig(level=logging.INFO,
                      format='# %(asctime)-15s: %(message)s')
  logging.root.setLevel(logging.INFO)

  # Check if KVM is enabled for x86 AVD's and check for x86 system images.
  if options.abi =='x86':
    if not install_emulator_deps.CheckKVM():
      logging.critical('ERROR: KVM must be enabled in BIOS, and installed. '
                       'Enable KVM in BIOS and run install_emulator_deps.py')
      return 1
    elif not install_emulator_deps.CheckX86Image():
      logging.critical('ERROR: System image for x86 AVD not installed. Run '
                       'install_emulator_deps.py')
      return 1

  if not install_emulator_deps.CheckSDK():
    logging.critical('ERROR: Emulator SDK not installed. Run '
                     'install_emulator_deps.py.')
    return 1

  emulator.LaunchEmulators(options.emulator_count, options.abi, True)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
