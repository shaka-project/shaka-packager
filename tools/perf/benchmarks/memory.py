# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import memory


class MemoryTop25(test.Test):
  test = memory.Memory
  page_set = 'page_sets/top_25.json'


class Reload2012Q3(test.Test):
  test = memory.Memory
  page_set = 'page_sets/2012Q3.json'
