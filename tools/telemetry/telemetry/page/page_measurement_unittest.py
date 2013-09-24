# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os

from telemetry.core import wpr_modes
from telemetry.page import page_measurement
from telemetry.page import page_measurement_unittest_base
from telemetry.page import page as page_module
from telemetry.page import page_set
from telemetry.page import page_set_archive_info
from telemetry.page.actions import all_page_actions
from telemetry.page.actions import page_action
from telemetry.unittest import options_for_unittests

class MeasurementThatFails(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    raise page_measurement.MeasurementFailure('Intentional failure.')

class MeasurementThatHasDefaults(page_measurement.PageMeasurement):
  def AddCommandLineOptions(self, parser):
    parser.add_option('-x', dest='x', default=3)

  def MeasurePage(self, page, tab, results):
    assert self.options.x == 3
    results.Add('x', 'ms', 7)

class MeasurementForBlank(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    contents = tab.EvaluateJavaScript('document.body.textContent')
    assert contents.strip() == 'Hello world'

class MeasurementForReplay(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    # Web Page Replay returns '404 Not found' if a page is not in the archive.
    contents = tab.EvaluateJavaScript('document.body.textContent')
    if '404 Not Found' in contents.strip():
      raise page_measurement.MeasurementFailure('Page not in archive.')

class MeasurementQueryParams(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    query = tab.EvaluateJavaScript('window.location.search')
    assert query.strip() == '?foo=1'

class MeasurementWithAction(page_measurement.PageMeasurement):
  def __init__(self):
    super(MeasurementWithAction, self).__init__('test_action')

  def MeasurePage(self, page, tab, results):
    pass

class PageMeasurementUnitTest(
  page_measurement_unittest_base.PageMeasurementUnitTestBase):

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.wpr_mode = wpr_modes.WPR_OFF

  def testGotToBlank(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    measurement = MeasurementForBlank()
    all_results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(all_results.failures))

  def testGotQueryParams(self):
    ps = self.CreatePageSet('file:///../../unittest_data/blank.html?foo=1')
    measurement = MeasurementQueryParams()
    ps.pages[-1].query_params = '?foo=1'
    all_results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(all_results.failures))

  def testFailure(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    measurement = MeasurementThatFails()
    all_results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(1, len(all_results.failures))

  def testDefaults(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    measurement = MeasurementThatHasDefaults()
    all_results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(len(all_results.page_results), 1)
    self.assertEquals(
      all_results.page_results[0].FindValueByTraceName('x').value, 7)

  def disabled_testRecordAndReplay(self):
    # This test is disabled because it runs against live sites, and needs to be
    # fixed. crbug.com/179038
    test_archive = '/tmp/google.wpr'
    google_url = 'http://www.google.com/'
    foo_url = 'http://www.foo.com/'
    archive_info_template = ("""
{
"archives": {
  "%s": ["%s"]
}
}
""")
    try:
      ps = page_set.PageSet()
      measurement = MeasurementForReplay()

      # First record an archive with only www.google.com.
      self._options.wpr_mode = wpr_modes.WPR_RECORD

      ps.wpr_archive_info = page_set_archive_info.PageSetArchiveInfo(
          '', '', json.loads(archive_info_template %
                             (test_archive, google_url)))
      ps.pages = [page_module.Page(google_url, ps)]
      all_results = self.RunMeasurement(measurement, ps, options=self._options)
      self.assertEquals(0, len(all_results.failures))

      # Now replay it and verify that google.com is found but foo.com is not.
      self._options.wpr_mode = wpr_modes.WPR_REPLAY

      ps.wpr_archive_info = page_set_archive_info.PageSetArchiveInfo(
          '', '', json.loads(archive_info_template % (test_archive, foo_url)))
      ps.pages = [page_module.Page(foo_url, ps)]
      all_results = self.RunMeasurement(measurement, ps, options=self._options)
      self.assertEquals(1, len(all_results.failures))

      ps.wpr_archive_info = page_set_archive_info.PageSetArchiveInfo(
          '', '', json.loads(archive_info_template %
                             (test_archive, google_url)))
      ps.pages = [page_module.Page(google_url, ps)]
      all_results = self.RunMeasurement(measurement, ps, options=self._options)
      self.assertEquals(0, len(all_results.failures))

      self.assertTrue(os.path.isfile(test_archive))

    finally:
      if os.path.isfile(test_archive):
        os.remove(test_archive)

  def testActions(self):
    action_called = [False]
    class MockAction(page_action.PageAction):
      def RunAction(self, page, tab, previous_action):
        action_called[0] = True
    all_page_actions.RegisterClassForTest('mock', MockAction)

    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    setattr(ps.pages[0], 'test_action', {'action': 'mock'})
    measurement = MeasurementWithAction()
    self.RunMeasurement(measurement, ps, options=self._options)
    self.assertTrue(action_called[0])
