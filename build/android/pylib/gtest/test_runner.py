# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

from pylib import android_commands
from pylib import constants
from pylib import pexpect
from pylib.base import base_test_result
from pylib.base import base_test_runner


def _TestSuiteRequiresMockTestServer(suite_name):
  """Returns True if the test suite requires mock test server."""
  tests_require_net_test_server = ['unit_tests', 'net_unittests',
                                   'content_unittests',
                                   'content_browsertests']
  return (suite_name in
          tests_require_net_test_server)


class TestRunner(base_test_runner.BaseTestRunner):
  def __init__(self, test_options, device, test_package):
    """Single test suite attached to a single device.

    Args:
      test_options: A GTestOptions object.
      device: Device to run the tests.
      test_package: An instance of TestPackage class.
    """

    super(TestRunner, self).__init__(device, test_options.tool,
                                     test_options.build_type,
                                     test_options.push_deps,
                                     test_options.cleanup_test_files)

    self.test_package = test_package
    self.test_package.tool = self.tool
    self._test_arguments = test_options.test_arguments

    timeout = test_options.timeout
    if timeout == 0:
      timeout = 60
    # On a VM (e.g. chromium buildbots), this timeout is way too small.
    if os.environ.get('BUILDBOT_SLAVENAME'):
      timeout = timeout * 2

    self._timeout = timeout * self.tool.GetTimeoutScale()

  #override
  def InstallTestPackage(self):
    self.test_package.Install(self.adb)

  def GetAllTests(self):
    """Install test package and get a list of all tests."""
    self.test_package.Install(self.adb)
    return self.test_package.GetAllTests(self.adb)

  #override
  def PushDataDeps(self):
    self.adb.WaitForSdCardReady(20)
    self.tool.CopyFiles()
    if os.path.exists(constants.ISOLATE_DEPS_DIR):
      device_dir = self.adb.GetExternalStorage()
      # TODO(frankf): linux_dumper_unittest_helper needs to be in the same dir
      # as breakpad_unittests exe. Find a better way to do this.
      if self.test_package.suite_name == 'breakpad_unittests':
        device_dir = constants.TEST_EXECUTABLE_DIR
      for p in os.listdir(constants.ISOLATE_DEPS_DIR):
        self.adb.PushIfNeeded(
            os.path.join(constants.ISOLATE_DEPS_DIR, p),
            os.path.join(device_dir, p))

  def _ParseTestOutput(self, p):
    """Process the test output.

    Args:
      p: An instance of pexpect spawn class.

    Returns:
      A TestRunResults object.
    """
    results = base_test_result.TestRunResults()

    # Test case statuses.
    re_run = re.compile('\[ RUN      \] ?(.*)\r\n')
    re_fail = re.compile('\[  FAILED  \] ?(.*)\r\n')
    re_ok = re.compile('\[       OK \] ?(.*?) .*\r\n')

    # Test run statuses.
    re_passed = re.compile('\[  PASSED  \] ?(.*)\r\n')
    re_runner_fail = re.compile('\[ RUNNER_FAILED \] ?(.*)\r\n')
    # Signal handlers are installed before starting tests
    # to output the CRASHED marker when a crash happens.
    re_crash = re.compile('\[ CRASHED      \](.*)\r\n')

    log = ''
    try:
      while True:
        full_test_name = None
        found = p.expect([re_run, re_passed, re_runner_fail],
                         timeout=self._timeout)
        if found == 1:  # re_passed
          break
        elif found == 2:  # re_runner_fail
          break
        else:  # re_run
          full_test_name = p.match.group(1).replace('\r', '')
          found = p.expect([re_ok, re_fail, re_crash], timeout=self._timeout)
          log = p.before.replace('\r', '')
          if found == 0:  # re_ok
            if full_test_name == p.match.group(1).replace('\r', ''):
              results.AddResult(base_test_result.BaseTestResult(
                  full_test_name, base_test_result.ResultType.PASS,
                  log=log))
          elif found == 2:  # re_crash
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.CRASH,
                log=log))
            break
          else:  # re_fail
            results.AddResult(base_test_result.BaseTestResult(
                full_test_name, base_test_result.ResultType.FAIL, log=log))
    except pexpect.EOF:
      logging.error('Test terminated - EOF')
      # We're here because either the device went offline, or the test harness
      # crashed without outputting the CRASHED marker (crbug.com/175538).
      if not self.adb.IsOnline():
        raise android_commands.errors.DeviceUnresponsiveError(
            'Device %s went offline.' % self.device)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.CRASH,
            log=p.before.replace('\r', '')))
    except pexpect.TIMEOUT:
      logging.error('Test terminated after %d second timeout.',
                    self._timeout)
      if full_test_name:
        results.AddResult(base_test_result.BaseTestResult(
            full_test_name, base_test_result.ResultType.TIMEOUT,
            log=p.before.replace('\r', '')))
    finally:
      p.close()

    ret_code = self.test_package.GetGTestReturnCode(self.adb)
    if ret_code:
      logging.critical(
          'gtest exit code: %d\npexpect.before: %s\npexpect.after: %s',
          ret_code, p.before, p.after)

    return results

  #override
  def RunTest(self, test):
    test_results = base_test_result.TestRunResults()
    if not test:
      return test_results, None

    try:
      self.test_package.ClearApplicationState(self.adb)
      self.test_package.CreateCommandLineFileOnDevice(
          self.adb, test, self._test_arguments)
      test_results = self._ParseTestOutput(
          self.test_package.SpawnTestProcess(self.adb))
    finally:
      self.CleanupSpawningServerState()
    # Calculate unknown test results.
    all_tests = set(test.split(':'))
    all_tests_ran = set([t.GetName() for t in test_results.GetAll()])
    unknown_tests = all_tests - all_tests_ran
    test_results.AddResults(
        [base_test_result.BaseTestResult(t, base_test_result.ResultType.UNKNOWN)
         for t in unknown_tests])
    retry = ':'.join([t.GetName() for t in test_results.GetNotPass()])
    return test_results, retry

  #override
  def SetUp(self):
    """Sets up necessary test enviroment for the test suite."""
    super(TestRunner, self).SetUp()
    if _TestSuiteRequiresMockTestServer(self.test_package.suite_name):
      self.LaunchChromeTestServerSpawner()
    self.tool.SetupEnvironment()

  #override
  def TearDown(self):
    """Cleans up the test enviroment for the test suite."""
    self.test_package.ClearApplicationState(self.adb)
    self.tool.CleanUpEnvironment()
    super(TestRunner, self).TearDown()
