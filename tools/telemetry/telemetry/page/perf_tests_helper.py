# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import

import os
import sys

def __init__():
  path = os.path.join(os.path.dirname(__file__),
                      '..', '..', '..', '..', 'build', 'android')
  path = os.path.abspath(path)
  assert os.path.exists(os.path.join(path,
                                     'pylib', '__init__.py'))
  if path not in sys.path:
    sys.path.append(path)

__init__()

from pylib import perf_tests_helper # pylint: disable=F0401
GeomMeanAndStdDevFromHistogram = \
    perf_tests_helper.GeomMeanAndStdDevFromHistogram
PrintPerfResult = \
    perf_tests_helper.PrintPerfResult
PrintPages = \
    perf_tests_helper.PrintPages

