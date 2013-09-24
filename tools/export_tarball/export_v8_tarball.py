#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a tarball with V8 sources, but without .svn directories.

This allows easy packaging of V8, synchronized with browser releases.

Example usage:

export_v8_tarball.py /foo/bar

The above will create file /foo/bar/v8-VERSION.tar.bz2 if it doesn't exist.
"""

import optparse
import os
import re
import subprocess
import sys
import tarfile

_V8_MAJOR_VERSION_PATTERN = re.compile(r'#define\s+MAJOR_VERSION\s+(.*)')
_V8_MINOR_VERSION_PATTERN = re.compile(r'#define\s+MINOR_VERSION\s+(.*)')
_V8_BUILD_NUMBER_PATTERN = re.compile(r'#define\s+BUILD_NUMBER\s+(.*)')
_V8_PATCH_LEVEL_PATTERN = re.compile(r'#define\s+PATCH_LEVEL\s+(.*)')

_V8_PATTERNS = [
  _V8_MAJOR_VERSION_PATTERN,
  _V8_MINOR_VERSION_PATTERN,
  _V8_BUILD_NUMBER_PATTERN,
  _V8_PATCH_LEVEL_PATTERN]


def GetV8Version(v8_directory):
  """
  Returns version number as string based on the string
  contents of version.cc file.
  """
  with open(os.path.join(v8_directory, 'src', 'version.cc')) as version_file:
    version_contents = version_file.read()

  version_components = []
  for pattern in _V8_PATTERNS:
    version_components.append(pattern.search(version_contents).group(1).strip())

  if version_components[len(version_components) - 1] == '0':
    version_components.pop()

  return '.'.join(version_components)


def GetSourceDirectory():
  return os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))


def GetV8Directory():
  return os.path.join(GetSourceDirectory(), 'v8')


# Workaround lack of the exclude parameter in add method in python-2.4.
# TODO(phajdan.jr): remove the workaround when it's not needed on the bot.
class MyTarFile(tarfile.TarFile):
  def add(self, name, arcname=None, recursive=True, exclude=None, filter=None):
    head, tail = os.path.split(name)
    if tail in ('.svn', '.git'):
      return

    tarfile.TarFile.add(self, name, arcname=arcname, recursive=recursive)


def main(argv):
  parser = optparse.OptionParser()
  options, args = parser.parse_args(argv)

  if len(args) != 1:
    print 'You must provide only one argument: output file directory'
    return 1

  v8_directory = GetV8Directory()
  if not os.path.exists(v8_directory):
    print 'Cannot find the v8 directory.'
    return 1

  v8_version = GetV8Version(v8_directory)
  print 'Packaging V8 version %s...' % v8_version
  output_basename = 'v8-%s' % v8_version
  output_fullname = os.path.join(args[0], output_basename + '.tar.bz2')

  if os.path.exists(output_fullname):
    print 'Already packaged, exiting.'
    return 0

  subprocess.check_call(["make", "dependencies"], cwd=v8_directory)

  archive = MyTarFile.open(output_fullname, 'w:bz2')
  try:
    archive.add(v8_directory, arcname=output_basename)
  finally:
    archive.close()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
