# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import tempfile
import unittest

from telemetry.core import user_agent
from telemetry.core import util
from telemetry.page import page as page_module
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.page import page_runner
from telemetry.unittest import options_for_unittests
from telemetry.page import test_expectations

SIMPLE_CREDENTIALS_STRING = """
{
  "test": {
    "username": "example",
    "password": "asdf"
  }
}
"""
class StubCredentialsBackend(object):
  def __init__(self, login_return_value):
    self.did_get_login = False
    self.did_get_login_no_longer_needed = False
    self.login_return_value = login_return_value

  @property
  def credentials_type(self): # pylint: disable=R0201
    return 'test'

  def LoginNeeded(self, tab, config): # pylint: disable=W0613
    self.did_get_login = True
    return self.login_return_value

  def LoginNoLongerNeeded(self, tab): # pylint: disable=W0613
    self.did_get_login_no_longer_needed = True

class PageRunnerTests(unittest.TestCase):
  # TODO(nduca): Move the basic "test failed, test succeeded" tests from
  # page_measurement_unittest to here.

  def testHandlingOfCrashedTab(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    page1 = page_module.Page('chrome://crash', ps)
    ps.pages.append(page1)

    class Test(page_test.PageTest):
      def RunTest(self, *args):
        pass

    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    results = page_runner.Run(Test('RunTest'), ps, expectations, options)
    self.assertEquals(0, len(results.successes))
    self.assertEquals(0, len(results.failures))
    self.assertEquals(1, len(results.errors))

  def testHandlingOfCrashedTabWithExpectedFailure(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    expectations.Fail('chrome://crash')
    page1 = page_module.Page('chrome://crash', ps)
    ps.pages.append(page1)

    class Test(page_test.PageTest):
      def RunTest(self, *args):
        pass

    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    results = page_runner.Run(Test('RunTest'), ps, expectations, options)
    self.assertEquals(1, len(results.successes))
    self.assertEquals(0, len(results.failures))
    self.assertEquals(0, len(results.errors))

  def testDiscardFirstResult(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    ps.pages.append(page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__)))
    ps.pages.append(page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__)))

    class Measurement(page_measurement.PageMeasurement):
      @property
      def discard_first_result(self):
        return True
      def MeasurePage(self, *args):
        pass

    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    options.reset_html_results = None

    options.repeat_options.page_repeat_iters = 1
    options.repeat_options.pageset_repeat_iters = 1
    results = page_runner.Run(Measurement(), ps, expectations, options)
    self.assertEquals(0, len(results.successes))
    self.assertEquals(0, len(results.failures))

    options.repeat_options.page_repeat_iters = 1
    options.repeat_options.pageset_repeat_iters = 2
    results = page_runner.Run(Measurement(), ps, expectations, options)
    self.assertEquals(2, len(results.successes))
    self.assertEquals(0, len(results.failures))

    options.repeat_options.page_repeat_iters = 2
    options.repeat_options.pageset_repeat_iters = 1
    results = page_runner.Run(Measurement(), ps, expectations, options)
    self.assertEquals(2, len(results.successes))
    self.assertEquals(0, len(results.failures))

    options.output_format = 'html'
    options.repeat_options.page_repeat_iters = 1
    options.repeat_options.pageset_repeat_iters = 1
    results = page_runner.Run(Measurement(), ps, expectations, options)
    self.assertEquals(0, len(results.successes))
    self.assertEquals(0, len(results.failures))

  def testCredentialsWhenLoginFails(self):
    credentials_backend = StubCredentialsBackend(login_return_value=False)
    did_run = self.runCredentialsTest(credentials_backend)
    assert credentials_backend.did_get_login == True
    assert credentials_backend.did_get_login_no_longer_needed == False
    assert did_run == False

  def testCredentialsWhenLoginSucceeds(self):
    credentials_backend = StubCredentialsBackend(login_return_value=True)
    did_run = self.runCredentialsTest(credentials_backend)
    assert credentials_backend.did_get_login == True
    assert credentials_backend.did_get_login_no_longer_needed == True
    assert did_run

  def runCredentialsTest(self, # pylint: disable=R0201
                         credentials_backend):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    page = page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__))
    page.credentials = "test"
    ps.pages.append(page)

    did_run = [False]

    try:
      with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(SIMPLE_CREDENTIALS_STRING)
        ps.credentials_path = f.name

      class TestThatInstallsCredentialsBackend(page_test.PageTest):
        def __init__(self, credentials_backend):
          super(TestThatInstallsCredentialsBackend, self).__init__('RunTest')
          self._credentials_backend = credentials_backend

        def DidStartBrowser(self, browser):
          browser.credentials.AddBackend(self._credentials_backend)

        def RunTest(self, page, tab, results): # pylint: disable=W0613,R0201
          did_run[0] = True

      test = TestThatInstallsCredentialsBackend(credentials_backend)
      options = options_for_unittests.GetCopy()
      options.output_format = 'none'
      page_runner.Run(test, ps, expectations, options)
    finally:
      os.remove(f.name)

    return did_run[0]

  def testUserAgent(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    page = page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__))
    ps.pages.append(page)
    ps.user_agent_type = 'tablet'

    class TestUserAgent(page_test.PageTest):
      def RunTest(self, page, tab, results): # pylint: disable=W0613,R0201
        actual_user_agent = tab.EvaluateJavaScript('window.navigator.userAgent')
        expected_user_agent = user_agent.UA_TYPE_MAPPING['tablet']
        assert actual_user_agent.strip() == expected_user_agent

        # This is so we can check later that the test actually made it into this
        # function. Previously it was timing out before even getting here, which
        # should fail, but since it skipped all the asserts, it slipped by.
        self.hasRun = True # pylint: disable=W0201

    test = TestUserAgent('RunTest')
    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    page_runner.Run(test, ps, expectations, options)

    self.assertTrue(hasattr(test, 'hasRun') and test.hasRun)

  # Ensure that page_runner forces exactly 1 tab before running a page.
  def testOneTab(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    page = page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__))
    ps.pages.append(page)

    class TestOneTab(page_test.PageTest):
      def __init__(self,
                   test_method_name,
                   action_name_to_run='',
                   needs_browser_restart_after_each_run=False):
        super(TestOneTab, self).__init__(test_method_name, action_name_to_run,
                                         needs_browser_restart_after_each_run)
        self._browser = None

      def DidStartBrowser(self, browser):
        self._browser = browser
        if self._browser.supports_tab_control:
          self._browser.tabs.New()

      def RunTest(self, page, tab, results): # pylint: disable=W0613,R0201
        if not self._browser.supports_tab_control:
          logging.warning('Browser does not support tab control, skipping test')
          return
        assert len(self._browser.tabs) == 1

    test = TestOneTab('RunTest')
    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    page_runner.Run(test, ps, expectations, options)

  # Ensure that page_runner allows the test to customize the browser before it
  # launches.
  def testBrowserBeforeLaunch(self):
    ps = page_set.PageSet()
    expectations = test_expectations.TestExpectations()
    page = page_module.Page(
        'file:///' + os.path.join(util.GetUnittestDataDir(), 'blank.html'),
        ps,
        base_dir=os.path.dirname(__file__))
    ps.pages.append(page)

    class TestBeforeLaunch(page_test.PageTest):
      def __init__(self,
                   test_method_name,
                   action_name_to_run=''):
        super(TestBeforeLaunch, self).__init__(
            test_method_name, action_name_to_run, False)
        self._did_call_will_start = False
        self._did_call_did_start = False

      def WillStartBrowser(self, browser):
        self._did_call_will_start = True
        # TODO(simonjam): Test that the profile is available.

      def DidStartBrowser(self, browser):
        assert self._did_call_will_start
        self._did_call_did_start = True

      def RunTest(self, page, tab, results): # pylint: disable=W0613,R0201
        assert self._did_call_did_start

    test = TestBeforeLaunch('RunTest')
    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    page_runner.Run(test, ps, expectations, options)
