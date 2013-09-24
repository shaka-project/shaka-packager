#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import time

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from telemetry.core import browser_finder
from telemetry.core import browser_options

def Main(args):
  options = browser_options.BrowserOptions()
  parser = options.CreateParser('telemetry_perf_test.py')
  options, args = parser.parse_args(args)

  browser_to_create = browser_finder.FindBrowser(options)
  assert browser_to_create
  with browser_to_create.Create() as b:
    tab = b.tabs[0]

    # Measure round-trip-time for evaluate
    times = []
    for i in range(1000):
      start = time.time()
      tab.EvaluateJavaScript('%i * 2' % i)
      times.append(time.time() - start)
    N = float(len(times))
    avg = sum(times, 0.0) / N
    squared_diffs = [(t - avg) * (t - avg) for t in times]
    stdev = sum(squared_diffs, 0.0) / (N - 1)
    times.sort()
    percentile_75 = times[int(0.75 * N)]

    print "%s: avg=%f; stdev=%f; min=%f; 75th percentile = %f" % (
      "Round trip time (seconds)",
      avg, stdev, min(times), percentile_75)

  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
