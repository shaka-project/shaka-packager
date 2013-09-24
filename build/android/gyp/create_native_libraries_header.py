#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes .h file for NativeLibraries.template

This header should contain the list of native libraries to load in the form:
  = { "lib1", "lib2" }
"""

import json
import optparse
import os
import sys

from util import build_utils


def main(argv):
  parser = optparse.OptionParser()

  parser.add_option('--output', help='Path to generated .java file')
  parser.add_option('--ordered-libraries',
      help='Path to json file containing list of ordered libraries')
  parser.add_option('--stamp', help='Path to touch on success')

  # args should be the list of libraries in dependency order.
  options, _ = parser.parse_args()

  build_utils.MakeDirectory(os.path.dirname(options.output))

  with open(options.ordered_libraries, 'r') as libfile:
    libraries = json.load(libfile)
  # Generates string of the form '= { "base", "net",
  # "content_shell_content_view" }' from a list of the form ["libbase.so",
  # libnet.so", "libcontent_shell_content_view.so"]
  libraries = ['"' + lib[3:-3] + '"' for lib in libraries]
  array = '= { ' + ', '.join(libraries) + '}';

  with open(options.output, 'w') as header:
    header.write(array)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
