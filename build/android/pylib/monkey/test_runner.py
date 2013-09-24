# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a monkey test on a single device."""

import random

from pylib.base import base_test_result
from pylib.base import base_test_runner


class TestRunner(base_test_runner.BaseTestRunner):
  """A TestRunner instance runs a monkey test on a single device."""

  def __init__(self, test_options, device, shard_index):
    super(TestRunner, self).__init__(device, None, test_options.build_type)
    self.options = test_options

  def _LaunchMonkeyTest(self):
    """Runs monkey test for a given package.

    Returns:
      Output from the monkey command on the device.
    """

    timeout_ms = self.options.event_count * self.options.throttle * 1.5

    cmd = ['monkey',
           '-p %s' % self.options.package_name,
           ' '.join(['-c %s' % c for c in self.options.category]),
           '--throttle %d' % self.options.throttle,
           '-s %d' % (self.options.seed or random.randint(1, 100)),
           '-v ' * self.options.verbose_count,
           '--monitor-native-crashes',
           '--kill-process-after-error',
           self.options.extra_args,
           '%d' % self.options.event_count]
    return self.adb.RunShellCommand(' '.join(cmd), timeout_time=timeout_ms)

  def RunTest(self, test_name):
    """Run a Monkey test on the device.

    Args:
      test_name: String to use for logging the test result.

    Returns:
      A tuple of (TestRunResults, retry).
    """
    self.adb.StartActivity(self.options.package_name,
                           self.options.activity_name,
                           wait_for_completion=True,
                           action='android.intent.action.MAIN',
                           force_stop=True)

    # Chrome crashes are not always caught by Monkey test runner.
    # Verify Chrome has the same PID before and after the test.
    before_pids = self.adb.ExtractPid(self.options.package_name)

    # Run the test.
    output = ''
    if before_pids:
      output = '\n'.join(self._LaunchMonkeyTest())
      after_pids = self.adb.ExtractPid(self.options.package_name)

    crashed = (not before_pids or not after_pids
               or after_pids[0] != before_pids[0])

    results = base_test_result.TestRunResults()
    if 'Monkey finished' in output and not crashed:
      result = base_test_result.BaseTestResult(
          test_name, base_test_result.ResultType.PASS, log=output)
    else:
      result = base_test_result.BaseTestResult(
          test_name, base_test_result.ResultType.FAIL, log=output)
    results.AddResult(result)
    return results, False
