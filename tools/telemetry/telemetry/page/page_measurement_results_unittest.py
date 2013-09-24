# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.page import page_measurement_results
from telemetry.page import page_set
from telemetry.page import perf_tests_helper

def _MakePageSet():
  return page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "http://www.bar.com/"}
        ]
      }, os.path.dirname(__file__))

class NonPrintingPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self):
    super(NonPrintingPageMeasurementResults, self).__init__()

  def _PrintPerfResult(self, *args):
    pass

class SummarySavingPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self):
    super(SummarySavingPageMeasurementResults, self).__init__()
    self.results = []

  def _PrintPerfResult(self, *args):
    res = perf_tests_helper.PrintPerfResult(*args, print_to_stdout=False)
    self.results.append(res)

class PageMeasurementResultsTest(unittest.TestCase):
  def test_basic(self):
    test_page_set = _MakePageSet()

    measurement_results = NonPrintingPageMeasurementResults()
    measurement_results.WillMeasurePage(test_page_set.pages[0])
    measurement_results.Add('a', 'seconds', 3)
    measurement_results.DidMeasurePage()

    measurement_results.WillMeasurePage(test_page_set.pages[1])
    measurement_results.Add('a', 'seconds', 3)
    measurement_results.DidMeasurePage()

    measurement_results.PrintSummary()

  def test_url_is_invalid_value(self):
    test_page_set = _MakePageSet()

    measurement_results = NonPrintingPageMeasurementResults()
    measurement_results.WillMeasurePage(test_page_set.pages[0])
    self.assertRaises(
      AssertionError,
      lambda: measurement_results.Add('url', 'string', 'foo'))

  def test_unit_change(self):
    test_page_set = _MakePageSet()

    measurement_results = NonPrintingPageMeasurementResults()
    measurement_results.WillMeasurePage(test_page_set.pages[0])
    measurement_results.Add('a', 'seconds', 3)
    measurement_results.DidMeasurePage()

    measurement_results.WillMeasurePage(test_page_set.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: measurement_results.Add('a', 'foobgrobbers', 3))

  def test_type_change(self):
    test_page_set = _MakePageSet()

    measurement_results = NonPrintingPageMeasurementResults()
    measurement_results.WillMeasurePage(test_page_set.pages[0])
    measurement_results.Add('a', 'seconds', 3)
    measurement_results.DidMeasurePage()

    measurement_results.WillMeasurePage(test_page_set.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: measurement_results.Add('a', 'seconds', 3, data_type='histogram'))

  def test_basic_summary_all_pages_fail(self):
    """If all pages fail, no summary is printed."""
    test_page_set = _MakePageSet()

    measurement_results = SummarySavingPageMeasurementResults()
    measurement_results.WillMeasurePage(test_page_set.pages[0])
    measurement_results.Add('a', 'seconds', 3)
    measurement_results.DidMeasurePage()
    measurement_results.AddFailureMessage(test_page_set.pages[0], 'message')

    measurement_results.WillMeasurePage(test_page_set.pages[1])
    measurement_results.Add('a', 'seconds', 7)
    measurement_results.DidMeasurePage()
    measurement_results.AddFailureMessage(test_page_set.pages[1], 'message')

    measurement_results.PrintSummary()
    self.assertEquals(measurement_results.results, [])
