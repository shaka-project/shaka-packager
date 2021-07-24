#!/usr/bin/python
#
# Copyright 2018 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""This script extracts a version or release notes from the changelog."""

from __future__ import print_function

import argparse
import re
import sys

def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--release_notes', action='store_true',
                      help='Print the latest release notes from the changelog')
  parser.add_argument('--version', action='store_true',
                      help='Print the latest version from the changelog')

  args = parser.parse_args()

  with open('CHANGELOG.md', 'r') as f:
    contents = f.read()

  # This excludes the header line with the release name and date, to match the
  # style of releases done before the automation was introduced.
  latest_entry = re.split(r'^(?=## \[)', contents, flags=re.M)[1]

  lines = latest_entry.strip().split('\n')
  first_line = lines[0]
  release_notes = '\n'.join(lines[1:])

  match = re.match(r'^## \[(.*)\]', first_line)
  if not match:
    raise RuntimeError('Unable to parse first line of CHANGELOG.md!')

  version = match[1]
  if not version.startswith('v'):
    version = 'v' + version

  if args.version:
    print(version)
  if args.release_notes:
    print(release_notes)

  return 0

if __name__ == '__main__':
  sys.exit(main())
