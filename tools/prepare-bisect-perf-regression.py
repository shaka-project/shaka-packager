#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prepare Performance Test Bisect Tool

This script is used by a trybot to create a working directory and sync an
initial copy of the depot for use in bisecting performance regressions.

An example usage:

./tools/prepare-bisect-perf-regressions.py --working_directory "~/builds"
  --output_buildbot_annotations

Would result in creating ~/builds/bisect and then populating it with a copy of
the depot.

"""

import optparse
import sys

import bisect_utils


def main():

  usage = ('%prog [options] [-- chromium-options]\n'
           'Prepares a temporary depot for use on a trybot.')

  parser = optparse.OptionParser(usage=usage)

  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='Path to the working directory where the script will '
                    'do an initial checkout of the chromium depot. The '
                    'files will be placed in a subdirectory "bisect" under '
                    'working_directory and that will be used to perform the '
                    'bisection.')
  parser.add_option('--output_buildbot_annotations',
                    action="store_true",
                    help='Add extra annotation output for buildbot.')
  parser.add_option('--target_platform',
                    type='choice',
                    choices=['chromium', 'cros', 'android'],
                    default='chromium',
                    help='The target platform. Choices are "chromium" (current '
                    'platform), "cros", or "android". If you specify something '
                    'other than "chromium", you must be properly set up to '
                    'build that platform.')
  (opts, args) = parser.parse_args()

  if not opts.working_directory:
    print 'Error: missing required parameter: --working_directory'
    print
    parser.print_help()
    return 1

  return bisect_utils.CreateBisectDirectoryAndSetupDepot(opts)


if __name__ == '__main__':
  sys.exit(main())
