#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility script to install APKs from the command line quickly."""

import multiprocessing
import optparse
import os
import sys

from pylib import android_commands
from pylib import constants
from pylib.utils import apk_helper
from pylib.utils import test_options_parser


def AddInstallAPKOption(option_parser):
  """Adds apk option used to install the APK to the OptionParser."""
  test_options_parser.AddBuildTypeOption(option_parser)
  option_parser.add_option('--apk',
                           help=('The name of the apk containing the '
                                 ' application (with the .apk extension).'))
  option_parser.add_option('--apk_package',
                           help=('The package name used by the apk containing '
                                 'the application.'))
  option_parser.add_option('--keep_data',
                           action='store_true',
                           default=False,
                           help=('Keep the package data when installing '
                                 'the application.'))


def ValidateInstallAPKOption(option_parser, options):
  """Validates the apk option and potentially qualifies the path."""
  if not options.apk:
    option_parser.error('--apk is mandatory.')
  if not os.path.exists(options.apk):
    options.apk = os.path.join(constants.DIR_SOURCE_ROOT,
                               'out', options.build_type,
                               'apks', options.apk)


def _InstallApk(args):
  apk_path, apk_package, keep_data, device = args
  android_commands.AndroidCommands(device=device).ManagedInstall(
      apk_path, keep_data, apk_package)
  print '-----  Installed on %s  -----' % device


def main(argv):
  parser = optparse.OptionParser()
  AddInstallAPKOption(parser)
  options, args = parser.parse_args(argv)
  ValidateInstallAPKOption(parser, options)
  if len(args) > 1:
    raise Exception('Error: Unknown argument:', args[1:])

  devices = android_commands.GetAttachedDevices()
  if not devices:
    raise Exception('Error: no connected devices')

  if not options.apk_package:
    options.apk_package = apk_helper.GetPackageName(options.apk)

  pool = multiprocessing.Pool(len(devices))
  # Send a tuple (apk_path, apk_package, device) per device.
  pool.map(_InstallApk, zip([options.apk] * len(devices),
                            [options.apk_package] * len(devices),
                            [options.keep_data] * len(devices),
                            devices))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
