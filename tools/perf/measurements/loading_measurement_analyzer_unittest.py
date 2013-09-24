# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import StringIO
import unittest

from measurements import loading_measurement_analyzer
from telemetry.core import util

class LoadingMeasurementAnalyzerUnitTest(unittest.TestCase):

  # TODO(tonyg): Remove this backfill when we can assume python 2.7 everywhere.
  def assertIn(self, first, second, msg=None):
    self.assertTrue(first in second,
                    msg="'%s' not found in '%s'" % (first, second))

  def testLoadingProfile(self):
    output = StringIO.StringIO()
    csv_path = os.path.join(
        util.GetChromiumSrcDir(),
        'tools', 'perf', 'measurements','test_data', 'loading_profile.csv')
    loading_measurement_analyzer.main([csv_path], stdout=output)
    output = output.getvalue()

    # Get the summary right.
    self.assertIn('Total URLs: 9', output)
    self.assertIn('Total page load time: 51s', output)
    self.assertIn('Average page load time: 5621ms', output)

    # Spot check a few samples.
    self.assertIn('WTF::IntHash::hash:  1359797948period  1.1%', output)
    self.assertIn('WebCore::rangesIntersect:   648335678period  0.5%', output)
    self.assertIn('v8::internal::Scanner::Scan:    19668346period  0.0', output)

  def testLoadingTimeline(self):
    output = StringIO.StringIO()
    csv_path = os.path.join(
        util.GetChromiumSrcDir(),
        'tools', 'perf', 'measurements','test_data', 'loading_timeline.csv')
    loading_measurement_analyzer.main([csv_path], stdout=output)
    output = output.getvalue()

    # Get the summary right.
    self.assertIn('Total URLs: 9', output)
    self.assertIn('Total page load time: 4s', output)
    self.assertIn('Average page load time: 422ms', output)
    self.assertIn('Total CPU time: 4s', output)
    self.assertIn('Average CPU time: 430ms', output)

    # Spot check a few samples.
    self.assertIn('EvaluateScript:           0s  19.0%', output)
    self.assertIn('ParseHTML:           0s  9.4%', output)
    self.assertIn('GCEvent:           0s  3.7%', output)
