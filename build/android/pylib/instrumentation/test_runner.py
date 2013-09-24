# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running instrumentation tests on a single device."""

import logging
import os
import re
import time

from pylib import android_commands
from pylib import constants
from pylib import json_perf_parser
from pylib import perf_tests_helper
from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import base_test_runner

import test_result


_PERF_TEST_ANNOTATION = 'PerfTest'


def _GetDataFilesForTestSuite(suite_basename):
  """Returns a list of data files/dirs needed by the test suite.

  Args:
    suite_basename: The test suite basename for which to return file paths.

  Returns:
    A list of test file and directory paths.
  """
  test_files = []
  if suite_basename in ['ChromeTest', 'ContentShellTest']:
    test_files += [
        'net/data/ssl/certificates/',
    ]
  return test_files


class TestRunner(base_test_runner.BaseTestRunner):
  """Responsible for running a series of tests connected to a single device."""

  _DEVICE_DATA_DIR = 'chrome/test/data'
  _HOSTMACHINE_PERF_OUTPUT_FILE = '/tmp/chrome-profile'
  _DEVICE_PERF_OUTPUT_SEARCH_PREFIX = (constants.DEVICE_PERF_OUTPUT_DIR +
                                       '/chrome-profile*')
  _DEVICE_HAS_TEST_FILES = {}

  def __init__(self, test_options, device, shard_index, test_pkg,
               ports_to_forward):
    """Create a new TestRunner.

    Args:
      test_options: An InstrumentationOptions object.
      device: Attached android device.
      shard_index: Shard index.
      test_pkg: A TestPackage object.
      ports_to_forward: A list of port numbers for which to set up forwarders.
          Can be optionally requested by a test case.
    """
    super(TestRunner, self).__init__(device, test_options.tool,
                                     test_options.build_type,
                                     test_options.push_deps,
                                     test_options.cleanup_test_files)
    self._lighttp_port = constants.LIGHTTPD_RANDOM_PORT_FIRST + shard_index

    self.options = test_options
    self.test_pkg = test_pkg
    self.ports_to_forward = ports_to_forward

  #override
  def InstallTestPackage(self):
    self.test_pkg.Install(self.adb)

  #override
  def PushDataDeps(self):
    # TODO(frankf): Implement a general approach for copying/installing
    # once across test runners.
    if TestRunner._DEVICE_HAS_TEST_FILES.get(self.device, False):
      logging.warning('Already copied test files to device %s, skipping.',
                      self.device)
      return

    test_data = _GetDataFilesForTestSuite(self.test_pkg.GetApkName())
    if test_data:
      # Make sure SD card is ready.
      self.adb.WaitForSdCardReady(20)
      for p in test_data:
        self.adb.PushIfNeeded(
            os.path.join(constants.DIR_SOURCE_ROOT, p),
            os.path.join(self.adb.GetExternalStorage(), p))

    # TODO(frankf): Specify test data in this file as opposed to passing
    # as command-line.
    for dest_host_pair in self.options.test_data:
      dst_src = dest_host_pair.split(':',1)
      dst_layer = dst_src[0]
      host_src = dst_src[1]
      host_test_files_path = constants.DIR_SOURCE_ROOT + '/' + host_src
      if os.path.exists(host_test_files_path):
        self.adb.PushIfNeeded(host_test_files_path,
                              self.adb.GetExternalStorage() + '/' +
                              TestRunner._DEVICE_DATA_DIR + '/' + dst_layer)
    self.tool.CopyFiles()
    TestRunner._DEVICE_HAS_TEST_FILES[self.device] = True

  def _GetInstrumentationArgs(self):
    ret = {}
    if self.options.wait_for_debugger:
      ret['debug'] = 'true'
    return ret

  def _TakeScreenshot(self, test):
    """Takes a screenshot from the device."""
    screenshot_name = os.path.join(constants.SCREENSHOTS_DIR, test + '.png')
    logging.info('Taking screenshot named %s', screenshot_name)
    self.adb.TakeScreenshot(screenshot_name)

  def SetUp(self):
    """Sets up the test harness and device before all tests are run."""
    super(TestRunner, self).SetUp()
    if not self.adb.IsRootEnabled():
      logging.warning('Unable to enable java asserts for %s, non rooted device',
                      self.device)
    else:
      if self.adb.SetJavaAssertsEnabled(True):
        self.adb.Reboot(full_reboot=False)

    # We give different default value to launch HTTP server based on shard index
    # because it may have race condition when multiple processes are trying to
    # launch lighttpd with same port at same time.
    http_server_ports = self.LaunchTestHttpServer(
        os.path.join(constants.DIR_SOURCE_ROOT), self._lighttp_port)
    if self.ports_to_forward:
      self._ForwardPorts([(port, port) for port in self.ports_to_forward])
    self.flags.AddFlags(['--enable-test-intents'])

  def TearDown(self):
    """Cleans up the test harness and saves outstanding data from test run."""
    if self.ports_to_forward:
      self._UnmapPorts([(port, port) for port in self.ports_to_forward])
    super(TestRunner, self).TearDown()

  def TestSetup(self, test):
    """Sets up the test harness for running a particular test.

    Args:
      test: The name of the test that will be run.
    """
    self.SetupPerfMonitoringIfNeeded(test)
    self._SetupIndividualTestTimeoutScale(test)
    self.tool.SetupEnvironment()

    # Make sure the forwarder is still running.
    self._RestartHttpServerForwarderIfNecessary()

  def _IsPerfTest(self, test):
    """Determines whether a test is a performance test.

    Args:
      test: The name of the test to be checked.

    Returns:
      Whether the test is annotated as a performance test.
    """
    return _PERF_TEST_ANNOTATION in self.test_pkg.GetTestAnnotations(test)

  def SetupPerfMonitoringIfNeeded(self, test):
    """Sets up performance monitoring if the specified test requires it.

    Args:
      test: The name of the test to be run.
    """
    if not self._IsPerfTest(test):
      return
    self.adb.Adb().SendCommand('shell rm ' +
                               TestRunner._DEVICE_PERF_OUTPUT_SEARCH_PREFIX)
    self.adb.StartMonitoringLogcat()

  def TestTeardown(self, test, raw_result):
    """Cleans up the test harness after running a particular test.

    Depending on the options of this TestRunner this might handle performance
    tracking.  This method will only be called if the test passed.

    Args:
      test: The name of the test that was just run.
      raw_result: result for this test.
    """

    self.tool.CleanUpEnvironment()

    # The logic below relies on the test passing.
    if not raw_result or raw_result.GetStatusCode():
      return

    self.TearDownPerfMonitoring(test)

  def TearDownPerfMonitoring(self, test):
    """Cleans up performance monitoring if the specified test required it.

    Args:
      test: The name of the test that was just run.
    Raises:
      Exception: if there's anything wrong with the perf data.
    """
    if not self._IsPerfTest(test):
      return
    raw_test_name = test.split('#')[1]

    # Wait and grab annotation data so we can figure out which traces to parse
    regex = self.adb.WaitForLogMatch(re.compile('\*\*PERFANNOTATION\(' +
                                                raw_test_name +
                                                '\)\:(.*)'), None)

    # If the test is set to run on a specific device type only (IE: only
    # tablet or phone) and it is being run on the wrong device, the test
    # just quits and does not do anything.  The java test harness will still
    # print the appropriate annotation for us, but will add --NORUN-- for
    # us so we know to ignore the results.
    # The --NORUN-- tag is managed by MainActivityTestBase.java
    if regex.group(1) != '--NORUN--':

      # Obtain the relevant perf data.  The data is dumped to a
      # JSON formatted file.
      json_string = self.adb.GetProtectedFileContents(
          '/data/data/com.google.android.apps.chrome/files/PerfTestData.txt')

      if json_string:
        json_string = '\n'.join(json_string)
      else:
        raise Exception('Perf file does not exist or is empty')

      if self.options.save_perf_json:
        json_local_file = '/tmp/chromium-android-perf-json-' + raw_test_name
        with open(json_local_file, 'w') as f:
          f.write(json_string)
        logging.info('Saving Perf UI JSON from test ' +
                     test + ' to ' + json_local_file)

      raw_perf_data = regex.group(1).split(';')

      for raw_perf_set in raw_perf_data:
        if raw_perf_set:
          perf_set = raw_perf_set.split(',')
          if len(perf_set) != 3:
            raise Exception('Unexpected number of tokens in perf annotation '
                            'string: ' + raw_perf_set)

          # Process the performance data
          result = json_perf_parser.GetAverageRunInfoFromJSONString(json_string,
                                                                    perf_set[0])
          perf_tests_helper.PrintPerfResult(perf_set[1], perf_set[2],
                                            [result['average']],
                                            result['units'])

  def _SetupIndividualTestTimeoutScale(self, test):
    timeout_scale = self._GetIndividualTestTimeoutScale(test)
    valgrind_tools.SetChromeTimeoutScale(self.adb, timeout_scale)

  def _GetIndividualTestTimeoutScale(self, test):
    """Returns the timeout scale for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    timeout_scale = 1
    if 'TimeoutScale' in annotations:
      for annotation in annotations:
        scale_match = re.match('TimeoutScale:([0-9]+)', annotation)
        if scale_match:
          timeout_scale = int(scale_match.group(1))
    if self.options.wait_for_debugger:
      timeout_scale *= 100
    return timeout_scale

  def _GetIndividualTestTimeoutSecs(self, test):
    """Returns the timeout in seconds for the given |test|."""
    annotations = self.test_pkg.GetTestAnnotations(test)
    if 'Manual' in annotations:
      return 600 * 60
    if 'External' in annotations:
      return 10 * 60
    if 'LargeTest' in annotations or _PERF_TEST_ANNOTATION in annotations:
      return 5 * 60
    if 'MediumTest' in annotations:
      return 3 * 60
    return 1 * 60

  def _RunTest(self, test, timeout):
    try:
      return self.adb.RunInstrumentationTest(
          test, self.test_pkg.GetPackageName(),
          self._GetInstrumentationArgs(), timeout)
    except android_commands.errors.WaitForResponseTimedOutError:
      logging.info('Ran the test with timeout of %ds.' % timeout)
      raise

  #override
  def RunTest(self, test):
    raw_result = None
    start_date_ms = None
    results = base_test_result.TestRunResults()
    timeout=(self._GetIndividualTestTimeoutSecs(test) *
             self._GetIndividualTestTimeoutScale(test) *
             self.tool.GetTimeoutScale())
    try:
      self.TestSetup(test)
      start_date_ms = int(time.time()) * 1000
      raw_result = self._RunTest(test, timeout)
      duration_ms = int(time.time()) * 1000 - start_date_ms
      status_code = raw_result.GetStatusCode()
      if status_code:
        log = raw_result.GetFailureReason()
        if not log:
          log = 'No information.'
        if (self.options.screenshot_failures or
            log.find('INJECT_EVENTS perm') >= 0):
          self._TakeScreenshot(test)
        result = test_result.InstrumentationTestResult(
            test, base_test_result.ResultType.FAIL, start_date_ms, duration_ms,
            log=log)
      else:
        result = test_result.InstrumentationTestResult(
            test, base_test_result.ResultType.PASS, start_date_ms, duration_ms)
      results.AddResult(result)
    # Catch exceptions thrown by StartInstrumentation().
    # See ../../third_party/android/testrunner/adb_interface.py
    except (android_commands.errors.WaitForResponseTimedOutError,
            android_commands.errors.DeviceUnresponsiveError,
            android_commands.errors.InstrumentationError), e:
      if start_date_ms:
        duration_ms = int(time.time()) * 1000 - start_date_ms
      else:
        start_date_ms = int(time.time()) * 1000
        duration_ms = 0
      message = str(e)
      if not message:
        message = 'No information.'
      results.AddResult(test_result.InstrumentationTestResult(
          test, base_test_result.ResultType.CRASH, start_date_ms, duration_ms,
          log=message))
      raw_result = None
    self.TestTeardown(test, raw_result)
    return (results, None if results.DidRunPass() else test)
