# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.page.actions import click_element
from telemetry.unittest import tab_test_case

class ClickElementActionTest(tab_test_case.TabTestCase):
  def testClickWithSelectorWaitForNavigation(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)
    self._tab.Navigate(
      self._browser.http_server.UrlOf('page_with_link.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()
    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/page_with_link.html')

    data = {'selector': 'a[id="clickme"]', 'wait_for_href_change': True}
    i = click_element.ClickElementAction(data)
    i.RunAction(None, self._tab, None)

    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/blank.html')

  def testClickWithTextWaitForRefChange(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)
    self._tab.Navigate(
      self._browser.http_server.UrlOf('page_with_link.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()
    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/page_with_link.html')

    data = {'text': 'Click me', 'wait_for_href_change': True}
    i = click_element.ClickElementAction(data)
    i.RunAction(None, self._tab, None)

    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/blank.html')
