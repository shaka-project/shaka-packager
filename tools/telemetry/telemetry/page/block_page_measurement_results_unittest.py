# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import StringIO
import os
import unittest

from telemetry.page import block_page_measurement_results
from telemetry.page import page_set

BlockPageMeasurementResults = \
    block_page_measurement_results.BlockPageMeasurementResults

def _MakePageSet():
  return page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "http://www.bar.com/"}
        ]
      }, os.path.dirname(__file__))

class NonPrintingBlockPageMeasurementResults(BlockPageMeasurementResults):
  def __init__(self, *args):
    super(NonPrintingBlockPageMeasurementResults, self).__init__(*args)

  def _PrintPerfResult(self, *args):
    pass

class BlockPageMeasurementResultsTest(unittest.TestCase):
  def setUp(self):
    self._output = StringIO.StringIO()
    self._page_set = _MakePageSet()

  @property
  def lines(self):
    lines = StringIO.StringIO(self._output.getvalue()).readlines()
    return [line.strip() for line in lines]

  @property
  def data(self):
    return [line.split(': ', 1) for line in self.lines]

  def test_with_output_after_every_page(self):
    results = NonPrintingBlockPageMeasurementResults(self._output)
    results.WillMeasurePage(self._page_set[0])
    results.Add('foo', 'seconds', 3)
    results.DidMeasurePage()

    results.WillMeasurePage(self._page_set[1])
    results.Add('bar', 'seconds', 4)
    results.DidMeasurePage()

    expected = [
      ['url', 'http://www.foo.com/'],
      ['foo (seconds)', '3'],
      [''],
      ['url', 'http://www.bar.com/'],
      ['bar (seconds)', '4'],
      ['']
    ]
    self.assertEquals(self.data, expected)
