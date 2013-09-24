# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import StringIO
import unittest

from telemetry.page import html_page_measurement_results
from telemetry.page import page_set


def _MakePageSet():
  return page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "http://www.bar.com/"},
        {"url": "http://www.baz.com/"}
        ]
      }, os.path.dirname(__file__))


class DeterministicHtmlPageMeasurementResults(
    html_page_measurement_results.HtmlPageMeasurementResults):
  def _GetBuildTime(self):
    return 'build_time'

  def _GetRevision(self):
    return 'revision'


# Wrap string IO with a .name property so that it behaves more like a file.
class StringIOFile(StringIO.StringIO):
  name = 'fake_output_file'


class HtmlPageMeasurementResultsTest(unittest.TestCase):

  # TODO(tonyg): Remove this backfill when we can assume python 2.7 everywhere.
  def assertIn(self, first, second, msg=None):
    self.assertTrue(first in second,
                    msg="'%s' not found in '%s'" % (first, second))

  def test_basic_summary(self):
    test_page_set = _MakePageSet()
    output_file = StringIOFile()

    # Run the first time and verify the results are written to the HTML file.
    results = DeterministicHtmlPageMeasurementResults(
        output_file, 'test_name', False, 'browser_type')
    results.WillMeasurePage(test_page_set.pages[0])
    results.Add('a', 'seconds', 3)
    results.DidMeasurePage()
    results.WillMeasurePage(test_page_set.pages[1])
    results.Add('a', 'seconds', 7)
    results.DidMeasurePage()

    results.PrintSummary()
    expected = (
        '<script id="results-json" type="application/json">'
          '[{'
            '"platform": "browser_type", '
            '"buildTime": "build_time", '
            '"tests": {'
              '"test_name": {'
                '"metrics": {'
                  '"a": {'
                    '"current": [3, 7], '
                    '"units": "seconds", '
                    '"important": true'
                  '}, '
                  '"a_by_url.http://www.bar.com/": {'
                    '"current": [7], '
                    '"units": "seconds", '
                    '"important": false'
                  '}, '
                  '"a_by_url.http://www.foo.com/": {'
                    '"current": [3], '
                    '"units": "seconds", '
                    '"important": false'
                  '}'
                '}'
              '}'
            '}, '
            '"revision": "revision"'
          '}]'
        '</script>')
    self.assertIn(expected, output_file.getvalue())

    # Run the second time and verify the results are appended to the HTML file.
    output_file.seek(0)
    results = DeterministicHtmlPageMeasurementResults(
        output_file, 'test_name', False, 'browser_type')
    results.WillMeasurePage(test_page_set.pages[0])
    results.Add('a', 'seconds', 4)
    results.DidMeasurePage()
    results.WillMeasurePage(test_page_set.pages[1])
    results.Add('a', 'seconds', 8)
    results.DidMeasurePage()

    results.PrintSummary()
    expected = (
        '<script id="results-json" type="application/json">'
          '[{'
            '"platform": "browser_type", '
            '"buildTime": "build_time", '
            '"tests": {'
              '"test_name": {'
                '"metrics": {'
                  '"a": {'
                    '"current": [3, 7], '
                    '"units": "seconds", '
                    '"important": true'
                  '}, '
                  '"a_by_url.http://www.bar.com/": {'
                    '"current": [7], '
                    '"units": "seconds", '
                    '"important": false'
                  '}, '
                  '"a_by_url.http://www.foo.com/": {'
                    '"current": [3], '
                    '"units": "seconds", '
                    '"important": false'
                  '}'
                '}'
              '}'
            '}, '
            '"revision": "revision"'
          '}, '
          '{'
            '"platform": "browser_type", '
            '"buildTime": "build_time", '
            '"tests": {'
              '"test_name": {'
                '"metrics": {'
                  '"a": {'
                    '"current": [4, 8], '
                    '"units": "seconds", '
                    '"important": true'
                  '}, '
                  '"a_by_url.http://www.bar.com/": {'
                    '"current": [8], '
                    '"units": "seconds", '
                    '"important": false'
                  '}, '
                  '"a_by_url.http://www.foo.com/": {'
                    '"current": [4], '
                    '"units": "seconds", '
                    '"important": false'
                  '}'
                '}'
              '}'
            '}, '
            '"revision": "revision"'
          '}]'
        '</script>')
    self.assertIn(expected, output_file.getvalue())
    last_output_len = len(output_file.getvalue())

    # Now reset the results and verify the old ones are gone.
    output_file.seek(0)
    results = DeterministicHtmlPageMeasurementResults(
        output_file, 'test_name', True, 'browser_type')
    results.WillMeasurePage(test_page_set.pages[0])
    results.Add('a', 'seconds', 5)
    results.DidMeasurePage()
    results.WillMeasurePage(test_page_set.pages[1])
    results.Add('a', 'seconds', 9)
    results.DidMeasurePage()

    results.PrintSummary()
    expected = (
        '<script id="results-json" type="application/json">'
          '[{'
            '"platform": "browser_type", '
            '"buildTime": "build_time", '
            '"tests": {'
              '"test_name": {'
                '"metrics": {'
                  '"a": {'
                    '"current": [5, 9], '
                    '"units": "seconds", '
                    '"important": true'
                  '}, '
                  '"a_by_url.http://www.bar.com/": {'
                    '"current": [9], '
                    '"units": "seconds", '
                    '"important": false'
                  '}, '
                  '"a_by_url.http://www.foo.com/": {'
                    '"current": [5], '
                    '"units": "seconds", '
                    '"important": false'
                  '}'
                '}'
              '}'
            '}, '
            '"revision": "revision"'
          '}]'
        '</script>')
    self.assertIn(expected, output_file.getvalue())
    self.assertTrue(len(output_file.getvalue()) < last_output_len)
