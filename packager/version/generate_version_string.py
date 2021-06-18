#!/usr/bin/python
#
# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""This script is used to generate version string for packager."""

from __future__ import print_function

import subprocess

if __name__ == '__main__':
  try:
    version_tag = subprocess.check_output('git tag --points-at HEAD',
        stderr=subprocess.STDOUT, shell=True).rstrip()
  except subprocess.CalledProcessError as e:
    # git tag --points-at is not supported in old versions of git. Just ignore
    # version_tag in this case.
    version_tag = None

  try:
    version_hash = subprocess.check_output('git rev-parse --short HEAD',
        stderr=subprocess.STDOUT, shell=True).rstrip()
  except subprocess.CalledProcessError as e:
    version_hash = 'unknown-version'

  if version_tag:
    print('{0}-{1}'.format(version_tag, version_hash))
  else:
    print(version_hash)
