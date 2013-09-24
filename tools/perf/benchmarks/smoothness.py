# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import smoothness


class SmoothnessTop25(test.Test):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/top_25.json'
  options = {'report_all_results': False}
