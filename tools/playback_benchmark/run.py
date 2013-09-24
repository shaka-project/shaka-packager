#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests playback.

Prerequisites:
1. OpenSSL library - http://www.openssl.org/
2. Python interface to the OpenSSL library - https://launchpad.net/pyopenssl

Example usage:
python run.py -t <test_dir>
"""

from optparse import OptionParser
import sys

import playback_driver
import proxy_handler


def Run(options):
  driver = playback_driver.PlaybackRequestHandler(options.test_dir)
  httpd = proxy_handler.CreateServer(driver, options.port)
  sa = httpd.socket.getsockname()
  print "Serving HTTP on", sa[0], "port", sa[1], "..."
  httpd.serve_forever()


def main():
  parser = OptionParser()
  parser.add_option("-t", "--test-dir", dest="test_dir",
                    help="directory containing recorded test data")
  parser.add_option("-p", "--port", dest="port", type="int", default=8000)
  options = parser.parse_args()[0]
  if not options.test_dir:
    raise Exception('please specify test directory')

  Run(options)


if __name__ == '__main__':
  sys.exit(main())
