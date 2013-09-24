#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around swig.

Sets the SWIG_LIB environment var to point to Lib dir
and defers control to the platform-specific swig binary.

Depends on swig binaries being available at ../../third_party/swig.
"""

import os
import subprocess
import sys


def main():
  swig_dir = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]),
                             os.pardir, os.pardir, 'third_party', 'swig'))
  lib_dir = os.path.join(swig_dir, "Lib")
  os.putenv("SWIG_LIB", lib_dir)
  dir_map = {
      'darwin': 'mac',
      'linux2': 'linux',
      'linux3': 'linux',
      'win32':  'win',
  }
  # Swig documentation lies that platform macros are provided to swig
  # preprocessor. Provide them ourselves.
  platform_flags = {
      'darwin': '-DSWIGMAC',
      'linux2': '-DSWIGLINUX',
      'linux3': '-DSWIGLINUX',
      'win32':  '-DSWIGWIN',
  }
  swig_bin = os.path.join(swig_dir, dir_map[sys.platform], 'swig')
  args = [swig_bin, platform_flags[sys.platform]] + sys.argv[1:]
  args = [x.replace('/', os.sep) for x in args]
  return subprocess.call(args)


if __name__ == "__main__":
  sys.exit(main())
