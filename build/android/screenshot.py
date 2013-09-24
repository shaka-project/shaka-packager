#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes and saves a screenshot from an Android device.

Usage: screenshot.py [-s SERIAL] [[-f] FILE]

Options:
  -s SERIAL  connect to device with specified SERIAL
  -f FILE    write screenshot to FILE (default: Screenshot.png)
"""

from optparse import OptionParser
import os
import sys

from pylib import android_commands


def main():
  # Parse options.
  parser = OptionParser(usage='screenshot.py [-s SERIAL] [[-f] FILE]')
  parser.add_option('-s', '--serial', dest='serial',
                    help='connect to device with specified SERIAL',
                    metavar='SERIAL', default=None)
  parser.add_option('-f', '--file', dest='filename',
                    help='write screenshot to FILE (default: %default)',
                    metavar='FILE', default='Screenshot.png')
  (options, args) = parser.parse_args()

  if not options.serial and len(android_commands.GetAttachedDevices()) > 1:
    parser.error('Multiple devices are attached. '
                 'Please specify SERIAL with -s.')

  if len(args) > 1:
    parser.error('Too many positional arguments.')
  filename = os.path.abspath(args[0] if args else options.filename)

  # Grab screenshot and write to disk.
  ac = android_commands.AndroidCommands(options.serial)
  ac.TakeScreenshot(filename)
  return 0


if __name__ == '__main__':
  sys.exit(main())
