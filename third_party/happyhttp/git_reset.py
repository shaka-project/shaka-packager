#!/usr/bin/env python
#
# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# Perform 'git reset --hard HEAD' on src directory.

import os
import subprocess
import sys

if __name__ == '__main__':
  script_dir = os.path.dirname(os.path.realpath(__file__))
  src_dir = os.path.join(script_dir, 'src')

  # No need to perform a reset if the source hasn't been pulled yet.
  if not os.path.exists(src_dir):
    sys.exit(0)

  sys.exit(subprocess.call(['git', 'reset', '--hard', 'HEAD'], cwd=src_dir))

