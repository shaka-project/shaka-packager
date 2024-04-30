#!/usr/bin/python3
#
# Copyright 2015 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""This script is used to generate version string for packager."""

import subprocess
import sys

if __name__ == '__main__':
  try:
    version_tag = subprocess.check_output('git tag --points-at HEAD',
        stderr=subprocess.STDOUT, shell=True).decode().rstrip()
    if version_tag:
      print('Found version tag: {}'.format(version_tag), file=sys.stderr)
    else:
      print('Cannot find version tag!', file=sys.stderr)
  except subprocess.CalledProcessError as e:
    # git tag --points-at is not supported in old versions of git. Just ignore
    # version_tag in this case.
    version_tag = None
    print('Old version of git, cannot determine version tag!', file=sys.stderr)

  try:
    version_hash = subprocess.check_output('git rev-parse --short HEAD',
        stderr=subprocess.STDOUT, shell=True).decode().rstrip()
    print('Version hash: {}'.format(version_hash), file=sys.stderr)
  except subprocess.CalledProcessError as e:
    version_hash = 'unknown-version'
    print('Cannot find version hasah!', file=sys.stderr)

  if version_tag:
    output = '{0}-{1}'.format(version_tag, version_hash)
  else:
    output = version_hash

  # Final debug message, mirroring what is used to generate the source file:
  print('Final output: {}'.format(output), file=sys.stderr)

  # Actually used to generate the source file:
  print(output)
