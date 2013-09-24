# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import

import os
import sys

def __init__():
  path = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
  sys.path.append(path)

__init__()

from crx_id import crx_id # pylint: disable=F0401
GetCRXAppID = crx_id.GetCRXAppID
HasPublicKey = crx_id.HasPublicKey