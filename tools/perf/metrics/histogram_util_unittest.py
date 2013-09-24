# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import unittest

from metrics import histogram_util

class TestHistogram(unittest.TestCase):
  def testSubtractHistogram(self):
    baseline_histogram = """{"count": 3, "buckets": [
{"low": 1, "high": 2, "count": 1},
{"low": 2, "high": 3, "count": 2}]}"""

    histogram = """{"count": 14, "buckets": [
{"low": 1, "high": 2, "count": 1},
{"low": 2, "high": 3, "count": 3},
{"low": 3, "high": 4, "count": 10}]}"""

    new_histogram = json.loads(
        histogram_util.SubtractHistogram(histogram, baseline_histogram))
    new_buckets = dict()
    for b in new_histogram['buckets']:
      new_buckets[b['low']] = b['count']
    self.assertFalse(1 in new_buckets)
    self.assertEquals(1, new_buckets[2])
    self.assertEquals(10, new_buckets[3])
