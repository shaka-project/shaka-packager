# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import time

from telemetry.core import util
from telemetry.core import exceptions
from telemetry.unittest import tab_test_case


def _IsDocumentVisible(tab):
  state = tab.EvaluateJavaScript('document.webkitVisibilityState')
  # TODO(dtu): Remove when crbug.com/166243 is fixed.
  tab.Disconnect()
  return state == 'visible'


class TabTest(tab_test_case.TabTestCase):
  def testNavigateAndWaitToForCompleteState(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)
    self._tab.Navigate(self._browser.http_server.UrlOf('blank.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testNavigateAndWaitToForInteractiveState(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)
    self._tab.Navigate(self._browser.http_server.UrlOf('blank.html'))
    self._tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

  def testTabBrowserIsRightBrowser(self):
    self.assertEquals(self._tab.browser, self._browser)

  def testRendererCrash(self):
    self.assertRaises(exceptions.TabCrashException,
                      lambda: self._tab.Navigate('chrome://crash',
                                                 timeout=5))

  def testActivateTab(self):
    if not self._browser.supports_tab_control:
      logging.warning('Browser does not support tab control, skipping test.')
      return

    self.assertTrue(_IsDocumentVisible(self._tab))
    new_tab = self._browser.tabs.New()
    util.WaitFor(lambda: _IsDocumentVisible(new_tab), timeout=5)
    self.assertFalse(_IsDocumentVisible(self._tab))
    self._tab.Activate()
    util.WaitFor(lambda: _IsDocumentVisible(self._tab), timeout=5)
    self.assertFalse(_IsDocumentVisible(new_tab))


class GpuTabTest(tab_test_case.TabTestCase):
  def setUp(self):
    self._extra_browser_args = ['--enable-gpu-benchmarking']
    super(GpuTabTest, self).setUp()

  def testScreenshot(self):
    if not self._tab.screenshot_supported:
      logging.warning('Browser does not support screenshots, skipping test.')
      return

    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)
    self._tab.Navigate(
      self._browser.http_server.UrlOf('green_rect.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()
    pixel_ratio = self._tab.EvaluateJavaScript('window.devicePixelRatio || 1')

    # TODO(bajones): Sleep for a bit to counter BUG 260878.
    time.sleep(0.5)
    screenshot = self._tab.Screenshot(5)
    assert screenshot
    screenshot.GetPixelColor(0 * pixel_ratio, 0 * pixel_ratio).AssertIsRGB(
        0, 255, 0, tolerance=2)
    screenshot.GetPixelColor(31 * pixel_ratio, 31 * pixel_ratio).AssertIsRGB(
        0, 255, 0, tolerance=2)
    screenshot.GetPixelColor(32 * pixel_ratio, 32 * pixel_ratio).AssertIsRGB(
        255, 255, 255, tolerance=2)
