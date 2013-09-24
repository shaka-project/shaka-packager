#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse
import os
import sys

from util import build_utils


def StripLibrary(android_strip, android_strip_args, library_path, output_path):
  if build_utils.IsTimeStale(output_path, [library_path]):
    strip_cmd = ([android_strip] +
                 android_strip_args +
                 ['-o', output_path, library_path])
    build_utils.CheckCallDie(strip_cmd)



def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--android-strip',
      help='Path to the toolchain\'s strip binary')
  parser.add_option('--android-strip-arg', action='append',
      help='Argument to be passed to strip')
  parser.add_option('--libraries-dir',
      help='Directory for un-stripped libraries')
  parser.add_option('--stripped-libraries-dir',
      help='Directory for stripped libraries')
  parser.add_option('--libraries-file',
      help='Path to json file containing list of libraries')
  parser.add_option('--stamp', help='Path to touch on success')


  options, _ = parser.parse_args()

  with open(options.libraries_file, 'r') as libfile:
    libraries = json.load(libfile)

  build_utils.MakeDirectory(options.stripped_libraries_dir)

  for library in libraries:
    library_path = os.path.join(options.libraries_dir, library)
    stripped_library_path = os.path.join(
        options.stripped_libraries_dir, library)
    StripLibrary(options.android_strip, options.android_strip_arg, library_path,
        stripped_library_path)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
