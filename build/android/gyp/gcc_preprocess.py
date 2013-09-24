#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import subprocess
import sys

from util import build_utils

def DoGcc(options):
  build_utils.MakeDirectory(os.path.dirname(options.output))

  gcc_cmd = [
      'gcc',                 # invoke host gcc.
      '-E',                  # stop after preprocessing.
      '-D', 'ANDROID',       # Specify ANDROID define for pre-processor.
      '-x', 'c-header',      # treat sources as C header files
      '-P',                  # disable line markers, i.e. '#line 309'
      '-I', options.include_path,
      '-o', options.output,
      options.template
      ]

  build_utils.CheckCallDie(gcc_cmd)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--include-path', help='Include path for gcc.')
  parser.add_option('--template', help='Path to template.')
  parser.add_option('--output', help='Path for generated file.')
  parser.add_option('--stamp', help='Path to touch on success.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, _ = parser.parse_args()

  DoGcc(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
