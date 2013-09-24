# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.unittest import tab_test_case

unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                 '..', '..', '..', 'unittest_data')

class InspectorPageTest(tab_test_case.TabTestCase):
  def __init__(self, *args):
    super(InspectorPageTest, self).__init__(*args)

  def setUp(self):
    super(InspectorPageTest, self).setUp()
    self._browser.SetHTTPServerDirectories(unittest_data_dir)

  def testPageNavigateToNormalUrl(self):
    self._tab.Navigate(self._browser.http_server.UrlOf('blank.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testCustomActionToNavigate(self):
    self._tab.Navigate(
      self._browser.http_server.UrlOf('page_with_link.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()
    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/page_with_link.html')

    custom_action_called = [False]
    def CustomAction():
      custom_action_called[0] = True
      self._tab.ExecuteJavaScript('document.getElementById("clickme").click();')

    self._tab.PerformActionAndWaitForNavigate(CustomAction)

    self.assertTrue(custom_action_called[0])
    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/blank.html')

  def testGetCookieByName(self):
    self._tab.Navigate(
      self._browser.http_server.UrlOf('blank.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()
    self._tab.ExecuteJavaScript('document.cookie="foo=bar"')
    self.assertEquals(self._tab.GetCookieByName('foo'), 'bar')

  def testScriptToEvaluateOnCommit(self):
    self._tab.Navigate(
      self._browser.http_server.UrlOf('blank.html'),
      script_to_evaluate_on_commit='var foo = "bar";')
    self._tab.WaitForDocumentReadyStateToBeComplete()
    self.assertEquals(self._tab.EvaluateJavaScript('foo'), 'bar')
