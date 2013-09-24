#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import optparse
import os
import sys

from util import build_utils
from util import md5_check


def DoDex(options, paths):
  dx_binary = os.path.join(options.android_sdk_tools, 'dx')

  # See http://crbug.com/272064 for context on --force-jumbo.
  dex_cmd = [dx_binary, '--dex', '--force-jumbo', '--output',
             options.dex_path] + paths

  record_path = '%s.md5.stamp' % options.dex_path
  md5_check.CallAndRecordIfStale(
      lambda: build_utils.CheckCallDie(dex_cmd, suppress_output=True),
      record_path=record_path,
      input_paths=paths,
      input_strings=dex_cmd)

  build_utils.Touch(options.dex_path)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--android-sdk-tools',
                    help='Android sdk build tools directory.')
  parser.add_option('--dex-path', help='Dex output path.')
  parser.add_option('--configuration-name',
      help='The build CONFIGURATION_NAME.')
  parser.add_option('--proguard-enabled',
      help='"true" if proguard is enabled.')
  parser.add_option('--proguard-enabled-input-path',
      help='Path to dex in Release mode when proguard is enabled.')
  parser.add_option('--stamp', help='Path to touch on success.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, paths = parser.parse_args()

  if (options.proguard_enabled == "true"
      and options.configuration_name == "Release"):
    paths = [options.proguard_enabled_input_path]

  DoDex(options, paths)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

