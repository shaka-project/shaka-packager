#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple script which asks user to manually check result of bisection.

Typically used as by the run-bisect-manual-test.py script.
"""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'telemetry'))
from telemetry.core import browser_finder
from telemetry.core import browser_options


def _StartManualTest(options):
  """Start browser then ask the user whether build is good or bad."""
  browser_to_create = browser_finder.FindBrowser(options)
  print 'Starting browser: %s.' % options.browser_type
  with browser_to_create.Create() as browser:

    # Loop until we get a response that we can parse.
    while True:
      sys.stderr.write('Revision is [(g)ood/(b)ad]: ')
      response = raw_input()
      if response and response in ('g', 'b'):
        if response in ('g'):
          print "RESULT manual_test: manual_test= 1"
        else:
          print "RESULT manual_test: manual_test= 0"
        break

    browser.Close()


def main():
  usage = ('%prog [options]\n'
           'Starts browser with an optional url and asks user whether '
           'revision is good or bad.\n')

  options = browser_options.BrowserOptions()
  parser = options.CreateParser(usage)
  options, args = parser.parse_args()

  _StartManualTest(options)


if __name__ == '__main__':
  sys.exit(main())
