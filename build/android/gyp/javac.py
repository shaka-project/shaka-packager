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


def DoJavac(options):
  output_dir = options.output_dir

  src_dirs = build_utils.ParseGypList(options.src_dirs)
  java_files = build_utils.FindInDirectories(src_dirs, '*.java')
  if options.javac_includes:
    javac_includes = build_utils.ParseGypList(options.javac_includes)
    filtered_java_files = []
    for f in java_files:
      for include in javac_includes:
        if fnmatch.fnmatch(f, include):
          filtered_java_files.append(f)
          break
    java_files = filtered_java_files

  # Compiling guava with certain orderings of input files causes a compiler
  # crash... Sorted order works, so use that.
  # See https://code.google.com/p/guava-libraries/issues/detail?id=950
  java_files.sort()
  classpath = build_utils.ParseGypList(options.classpath)

  jar_inputs = []
  for path in classpath:
    if os.path.exists(path + '.TOC'):
      jar_inputs.append(path + '.TOC')
    else:
      jar_inputs.append(path)

  javac_cmd = [
      'javac',
      '-g',
      '-source', '1.5',
      '-target', '1.5',
      '-classpath', ':'.join(classpath),
      '-d', output_dir,
      '-Xlint:unchecked',
      '-Xlint:deprecation',
      ] + java_files

  def Compile():
    # Delete the classes directory. This ensures that all .class files in the
    # output are actually from the input .java files. For example, if a .java
    # file is deleted or an inner class is removed, the classes directory should
    # not contain the corresponding old .class file after running this action.
    build_utils.DeleteDirectory(output_dir)
    build_utils.MakeDirectory(output_dir)
    suppress_output = not options.chromium_code
    build_utils.CheckCallDie(javac_cmd, suppress_output=suppress_output)

  record_path = '%s/javac.md5.stamp' % options.output_dir
  md5_check.CallAndRecordIfStale(
      Compile,
      record_path=record_path,
      input_paths=java_files + jar_inputs,
      input_strings=javac_cmd)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--src-dirs', help='Directories containing java files.')
  parser.add_option('--javac-includes',
      help='A list of file patterns. If provided, only java files that match' +
        'one of the patterns will be compiled.')
  parser.add_option('--classpath', help='Classpath for javac.')
  parser.add_option('--output-dir', help='Directory for javac output.')
  parser.add_option('--stamp', help='Path to touch on success.')
  parser.add_option('--chromium-code', type='int', help='Whether code being '
                    'compiled should be built with stricter warnings for '
                    'chromium code.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, _ = parser.parse_args()

  DoJavac(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))


