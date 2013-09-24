# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(dmikurube): Remove this file once Python 2.7 is required.

import os
import sys

try:
  from collections import OrderedDict  # pylint: disable=E0611,W0611
except ImportError:
  _BASE_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  _SIMPLEJSON_PATH = os.path.join(_BASE_PATH,
                                  os.pardir,
                                  os.pardir,
                                  'third_party')
  sys.path.insert(0, _SIMPLEJSON_PATH)
  from simplejson import OrderedDict  # pylint: disable=W0611
