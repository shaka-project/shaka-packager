# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import StringIO
import csv
import os
import unittest

from telemetry.page import csv_page_measurement_results
from telemetry.page import page_set

def _MakePageSet():
  return page_set.PageSet.FromDict({
      "description": "hello",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "http://www.bar.com/"}
        ]
      }, os.path.dirname(__file__))

class NonPrintingCsvPageMeasurementResults(
    csv_page_measurement_results.CsvPageMeasurementResults):
  def __init__(self, *args):
    super(NonPrintingCsvPageMeasurementResults, self).__init__(*args)

  def _PrintPerfResult(self, *args):
    pass

class CsvPageMeasurementResultsTest(unittest.TestCase):
  def setUp(self):
    self._output = StringIO.StringIO()
    self._page_set = _MakePageSet()

  @property
  def lines(self):
    lines = StringIO.StringIO(self._output.getvalue()).readlines()
    return lines

  @property
  def output_header_row(self):
    rows = list(csv.reader(self.lines))
    return rows[0]

  @property
  def output_data_rows(self):
    rows = list(csv.reader(self.lines))
    return rows[1:]

  def test_with_output_after_every_page(self):
    results = NonPrintingCsvPageMeasurementResults(self._output, True)
    results.WillMeasurePage(self._page_set[0])
    results.Add('foo', 'seconds', 3)
    results.DidMeasurePage()
    self.assertEquals(
      self.output_header_row,
      ['url', 'foo (seconds)'])
    self.assertEquals(
      self.output_data_rows[0],
      [self._page_set[0].url, '3'])

    results.WillMeasurePage(self._page_set[1])
    results.Add('foo', 'seconds', 4)
    results.DidMeasurePage()
    self.assertEquals(
      len(self.output_data_rows),
      2)
    self.assertEquals(
      self.output_data_rows[1],
      [self._page_set[1].url, '4'])

  def test_with_output_after_every_page_and_inconsistency(self):
    results = NonPrintingCsvPageMeasurementResults(self._output, True)
    results.WillMeasurePage(self._page_set[0])
    results.Add('foo', 'seconds', 3)
    results.DidMeasurePage()

    # We printed foo, now change to bar
    results.WillMeasurePage(self._page_set[1])
    results.Add('bar', 'seconds', 4)

    self.assertRaises(
      Exception,
      lambda: results.DidMeasurePage()) # pylint: disable=W0108

  def test_with_output_at_print_summary_time(self):
    results = NonPrintingCsvPageMeasurementResults(self._output, False)
    results.WillMeasurePage(self._page_set[0])
    results.Add('foo', 'seconds', 3)
    results.DidMeasurePage()

    results.WillMeasurePage(self._page_set[1])
    results.Add('bar', 'seconds', 4)
    results.DidMeasurePage()

    results.PrintSummary()

    self.assertEquals(
      self.output_header_row,
      ['url', 'bar (seconds)', 'foo (seconds)'])
    self.assertEquals(
      self.output_data_rows,
      [[self._page_set[0].url, '-', '3'],
       [self._page_set[1].url, '4', '-']])

  def test_histogram(self):
    results = NonPrintingCsvPageMeasurementResults(self._output, False)
    results.WillMeasurePage(self._page_set[0])
    results.Add('a', '',
                '{"buckets": [{"low": 1, "high": 2, "count": 1}]}',
                data_type='histogram')
    results.DidMeasurePage()

    results.WillMeasurePage(self._page_set[1])
    results.Add('a', '',
                '{"buckets": [{"low": 2, "high": 3, "count": 1}]}',
                data_type='histogram')
    results.DidMeasurePage()

    results.PrintSummary()

    self.assertEquals(
      self.output_header_row,
      ['url', 'a ()'])
    self.assertEquals(
      self.output_data_rows,
      [[self._page_set[0].url, '1.5'],
       [self._page_set[1].url, '2.5']])
