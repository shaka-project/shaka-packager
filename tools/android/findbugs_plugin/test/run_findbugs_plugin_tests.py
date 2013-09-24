#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is used to test the findbugs plugin, it calls
# build/android/pylib/utils/findbugs.py to analyze the classes in
# org.chromium.tools.findbugs.plugin package, and expects to get the same
# issue with those in expected_result.txt.
#
# Useful command line:
# --rebaseline to generate the expected_result.txt, please make sure don't
# remove the expected result of exsting tests.


import optparse
import os
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             '..', '..', '..', '..',
                                             'build', 'android')))

from pylib import constants
from pylib.utils import findbugs


def main(argv):
  parser = findbugs.GetCommonParser()

  options, _ = parser.parse_args()

  if not options.known_bugs:
    options.known_bugs = os.path.join(constants.DIR_SOURCE_ROOT, 'tools',
                                      'android', 'findbugs_plugin', 'test',
                                      'expected_result.txt')
  if not options.only_analyze:
    options.only_analyze = 'org.chromium.tools.findbugs.plugin.*'

  return findbugs.Run(options)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
