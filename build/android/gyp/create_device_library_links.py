#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates symlinks to native libraries for an APK.

The native libraries should have previously been pushed to the device (in
options.target_dir). This script then creates links in an apk's lib/ folder to
those native libraries.
"""

import json
import optparse
import os
import sys

from util import build_device
from util import build_utils
from util import md5_check

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib.utils import apk_helper

def RunShellCommand(device, cmd):
  output = device.RunShellCommand(cmd)

  if output:
    raise Exception(
        'Unexpected output running command: ' + cmd + '\n' +
        '\n'.join(output))


def CreateSymlinkScript(options):
  libraries = build_utils.ReadJson(options.libraries_json)

  link_cmd = (
      'rm $APK_LIBRARIES_DIR/%(lib_basename)s > /dev/null 2>&1 \n'
      'ln -s $STRIPPED_LIBRARIES_DIR/%(lib_basename)s '
        '$APK_LIBRARIES_DIR/%(lib_basename)s \n'
      )

  script = '#!/bin/sh \n'

  for lib in libraries:
    script += link_cmd % { 'lib_basename': lib }

  with open(options.script_host_path, 'w') as scriptfile:
    scriptfile.write(script)


def TriggerSymlinkScript(options):
  device = build_device.GetBuildDeviceFromPath(
      options.build_device_configuration)
  if not device:
    return

  apk_package = apk_helper.GetPackageName(options.apk)
  apk_libraries_dir = '/data/data/%s/lib' % apk_package

  device_dir = os.path.dirname(options.script_device_path)
  mkdir_cmd = ('if [ ! -e %(dir)s ]; then mkdir -p %(dir)s; fi ' %
      { 'dir': device_dir })
  RunShellCommand(device, mkdir_cmd)
  device.PushIfNeeded(options.script_host_path, options.script_device_path)

  trigger_cmd = (
      'APK_LIBRARIES_DIR=%(apk_libraries_dir)s; '
      'STRIPPED_LIBRARIES_DIR=%(target_dir)s; '
      '. %(script_device_path)s'
      ) % {
          'apk_libraries_dir': apk_libraries_dir,
          'target_dir': options.target_dir,
          'script_device_path': options.script_device_path
          }
  RunShellCommand(device, trigger_cmd)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--apk', help='Path to the apk.')
  parser.add_option('--script-host-path',
      help='Path on the host for the symlink script.')
  parser.add_option('--script-device-path',
      help='Path on the device to push the created symlink script.')
  parser.add_option('--libraries-json',
      help='Path to the json list of native libraries.')
  parser.add_option('--target-dir',
      help='Device directory that contains the target libraries for symlinks.')
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option('--build-device-configuration',
      help='Path to build device configuration.')
  options, _ = parser.parse_args()

  required_options = ['apk', 'libraries_json', 'script_host_path',
      'script_device_path', 'target_dir']
  build_utils.CheckOptions(options, parser, required=required_options)

  CreateSymlinkScript(options)
  TriggerSymlinkScript(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
