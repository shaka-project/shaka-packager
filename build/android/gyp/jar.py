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


def DoJar(options):
  class_files = build_utils.FindInDirectory(options.classes_dir, '*.class')
  for exclude in build_utils.ParseGypList(options.excluded_classes):
    class_files = filter(
        lambda f: not fnmatch.fnmatch(f, exclude), class_files)

  jar_path = os.path.abspath(options.jar_path)

  # The paths of the files in the jar will be the same as they are passed in to
  # the command. Because of this, the command should be run in
  # options.classes_dir so the .class file paths in the jar are correct.
  jar_cwd = options.classes_dir
  class_files_rel = [os.path.relpath(f, jar_cwd) for f in class_files]
  jar_cmd = ['jar', 'cf0', jar_path] + class_files_rel

  record_path = '%s.md5.stamp' % options.jar_path
  md5_check.CallAndRecordIfStale(
      lambda: build_utils.CheckCallDie(jar_cmd, cwd=jar_cwd),
      record_path=record_path,
      input_paths=class_files,
      input_strings=jar_cmd)

  build_utils.Touch(options.jar_path)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--classes-dir', help='Directory containing .class files.')
  parser.add_option('--jar-path', help='Jar output path.')
  parser.add_option('--excluded-classes',
      help='List of .class file patterns to exclude from the jar.')
  parser.add_option('--stamp', help='Path to touch on success.')

  # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
  parser.add_option('--ignore', help='Ignored.')

  options, _ = parser.parse_args()

  DoJar(options)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

