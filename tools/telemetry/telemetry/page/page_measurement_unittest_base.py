# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.page import page_runner
from telemetry.page import page as page_module
from telemetry.page import page_set
from telemetry.page import test_expectations
from telemetry.unittest import options_for_unittests

class PageMeasurementUnitTestBase(unittest.TestCase):
  """unittest.TestCase-derived class to help in the construction of unit tests
  for a measurement."""

  def CreatePageSetFromFileInUnittestDataDir(self, test_filename):
    return self.CreatePageSet('file:///' + os.path.join(
        '..', '..', 'unittest_data', test_filename))

  def CreatePageSet(self, test_filename):
    base_dir = os.path.dirname(__file__)
    ps = page_set.PageSet(file_path=os.path.join(base_dir, 'foo.json'))
    page = page_module.Page(test_filename, ps, base_dir=base_dir)
    setattr(page, 'smoothness', {'action': 'scroll'})
    ps.pages.append(page)
    return ps

  def RunMeasurement(self, measurement, ps,
      expectations=test_expectations.TestExpectations(),
      options=None):
    """Runs a measurement against a pageset, returning the rows its outputs."""
    if options is None:
      options = options_for_unittests.GetCopy()
    assert options
    temp_parser = options.CreateParser()
    measurement.AddCommandLineOptions(temp_parser)
    defaults = temp_parser.get_default_values()
    for k, v in defaults.__dict__.items():
      if hasattr(options, k):
        continue
      setattr(options, k, v)

    measurement.CustomizeBrowserOptions(options)
    options.output_file = None
    options.output_format = 'none'
    options.output_trace_tag = None
    return page_runner.Run(measurement, ps, expectations, options)
