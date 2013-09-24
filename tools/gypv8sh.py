#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests.

Usage:
  python tools/gypv8sh.py v8_shell mock.js test_api.js js2webui.js \
         inputfile inputrelfile cxxoutfile jsoutfile
"""

import json
import optparse
import os
import subprocess
import sys
import shutil


def main ():
  parser = optparse.OptionParser()
  parser.set_usage(
      "%prog v8_shell mock.js axs_testing.js test_api.js js2webui.js "
      "testtype inputfile inputrelfile cxxoutfile jsoutfile")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  (opts, args) = parser.parse_args()

  if len(args) != 10:
    parser.error('all arguments are required.')
  (v8_shell, mock_js, axs_testing_js, test_api, js2webui, test_type,
      inputfile, inputrelfile, cxxoutfile, jsoutfile) = args
  arguments = [js2webui, inputfile, inputrelfile, cxxoutfile, test_type]
  cmd = [v8_shell, '-e', "arguments=" + json.dumps(arguments), mock_js,
         axs_testing_js, test_api, js2webui]
  if opts.verbose or opts.impotent:
    print cmd
  if not opts.impotent:
    try:
      with open(cxxoutfile, 'w') as f:
        subprocess.check_call(cmd, stdin=subprocess.PIPE, stdout=f)
      shutil.copyfile(inputfile, jsoutfile)
    except Exception, ex:
      if os.path.exists(cxxoutfile):
        os.remove(cxxoutfile)
      if os.path.exists(jsoutfile):
        os.remove(jsoutfile)
      raise


if __name__ == '__main__':
 sys.exit(main())
