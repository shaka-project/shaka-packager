# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import unittest

from telemetry.core import browser_finder
from telemetry.unittest import simple_mock
from telemetry.unittest import options_for_unittests
from telemetry.unittest import DisabledTest

_ = simple_mock.DONT_CARE


def _GetCredentialsPath():
  # TODO: This shouldn't depend on tools/perf.
  credentials_path = os.path.join(
      os.path.dirname(__file__),
      '..', '..', '..', '..', 'perf', 'data', 'credentials.json')
  if not os.path.exists(credentials_path):
    return None
  return credentials_path


class FormBasedCredentialsBackendUnitTestBase(unittest.TestCase):
  def setUp(self):
    self._credentials_type = None

  @DisabledTest
  def testRealLoginIfPossible(self):
    credentials_path = _GetCredentialsPath()
    if not credentials_path:
      logging.warning('Credentials file not found, skipping test.')
      return

    options = options_for_unittests.GetCopy()
    with browser_finder.FindBrowser(options).Create() as b:
      b.Start()
      b.credentials.credentials_path = credentials_path
      if not b.credentials.CanLogin(self._credentials_type):
        return
      ret = b.credentials.LoginNeeded(b.tabs[0], self._credentials_type)
      self.assertTrue(ret)

  @DisabledTest
  def testRealLoginWithDontOverrideProfileIfPossible(self):
    credentials_path = _GetCredentialsPath()
    if not credentials_path:
      logging.warning('Credentials file not found, skipping test.')
      return

    options = options_for_unittests.GetCopy()

    # Login once to make sure our default profile is logged in.
    with browser_finder.FindBrowser(options).Create() as b:
      b.Start()
      b.credentials.credentials_path = credentials_path

      if not b.credentials.CanLogin(self._credentials_type):
        return

      tab = b.tabs[0]

      # Should not be logged in, since this is a fresh credentials
      # instance.
      self.assertFalse(b.credentials.IsLoggedIn(self._credentials_type))

      # Log in.
      ret = b.credentials.LoginNeeded(tab, self._credentials_type)

      # Make sure login was successful.
      self.assertTrue(ret)
      self.assertTrue(b.credentials.IsLoggedIn(self._credentials_type))

      # Reset state. Now the backend thinks we're logged out, even
      # though we are logged in in our current browser session. This
      # simulates the effects of running with --dont-override-profile.
      b.credentials._ResetLoggedInState() # pylint: disable=W0212

      # Make sure the backend thinks we're logged out.
      self.assertFalse(b.credentials.IsLoggedIn(self._credentials_type))
      self.assertTrue(b.credentials.CanLogin(self._credentials_type))

      # Attempt to login again. This should detect that we've hit
      # the 'logged in' page instead of the login form, and succeed
      # instead of timing out.
      ret = b.credentials.LoginNeeded(tab, self._credentials_type)

      # Make sure our login attempt did in fact succeed and set the
      # backend's internal state to 'logged in'.
      self.assertTrue(ret)
      self.assertTrue(b.credentials.IsLoggedIn(self._credentials_type))

  def testLoginUsingMock(self):
    raise NotImplementedError()

  def _LoginUsingMock(self, backend, login_page_url, email_element_id,
                      password_element_id): # pylint: disable=R0201
    tab = simple_mock.MockObject()

    config = {'username': 'blah',
              'password': 'blargh'}

    tab.ExpectCall('Navigate', login_page_url)
    tab.ExpectCall('EvaluateJavaScript', _).WillReturn(False)
    tab.ExpectCall('EvaluateJavaScript', _).WillReturn(True)
    tab.ExpectCall('EvaluateJavaScript', _).WillReturn(False)
    tab.ExpectCall('WaitForDocumentReadyStateToBeInteractiveOrBetter')

    def VerifyEmail(js):
      assert email_element_id in js
      assert 'blah' in js
    tab.ExpectCall('ExecuteJavaScript', _).WhenCalled(VerifyEmail)

    def VerifyPw(js):
      assert password_element_id in js
      assert 'largh' in js
    tab.ExpectCall('ExecuteJavaScript', _).WhenCalled(VerifyPw)

    def VerifySubmit(js):
      assert '.submit' in js
    tab.ExpectCall('ExecuteJavaScript', _).WhenCalled(VerifySubmit)

    # Checking for form still up.
    tab.ExpectCall('EvaluateJavaScript', _).WillReturn(False)

    backend.LoginNeeded(tab, config)
