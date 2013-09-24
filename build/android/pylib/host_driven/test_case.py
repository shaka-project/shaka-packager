# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for host-driven test cases.

This test case is intended to serve as the base class for any host-driven
test cases. It is similar to the Python unitttest module in that test cases
inherit from this class and add methods which will be run as tests.

When a HostDrivenTestCase object is instantiated, its purpose is to run only one
test method in the derived class. The test runner gives it the name of the test
method the instance will run. The test runner calls SetUp with the device ID
which the test method will run against. The test runner runs the test method
itself, collecting the result, and calls TearDown.

Tests can perform arbitrary Python commands and asserts in test methods. Tests
that run instrumentation tests can make use of the _RunJavaTests helper function
to trigger Java tests and convert results into a single host-driven test result.
"""

import logging
import os
import time

from pylib import android_commands
from pylib.base import base_test_result
from pylib.instrumentation import test_package
from pylib.instrumentation import test_result
from pylib.instrumentation import test_runner

# aka the parent of com.google.android
BASE_ROOT = 'src' + os.sep


class HostDrivenTestCase(object):
  """Base class for host-driven test cases."""

  _HOST_DRIVEN_TAG = 'HostDriven'

  def __init__(self, test_name, instrumentation_options=None):
    """Create a test case initialized to run |test_name|.

    Args:
      test_name: The name of the method to run as the test.
      instrumentation_options: An InstrumentationOptions object.
    """
    self.test_name = test_name
    class_name = self.__class__.__name__
    self.qualified_name = '%s.%s' % (class_name, self.test_name)
    # Use tagged_name when creating results, so that we can identify host-driven
    # tests in the overall results.
    self.tagged_name = '%s_%s' % (self._HOST_DRIVEN_TAG, self.qualified_name)

    self.instrumentation_options = instrumentation_options
    self.ports_to_forward = []

  def SetUp(self, device, shard_index, build_type, push_deps,
            cleanup_test_files):
    self.device_id = device
    self.shard_index = shard_index
    self.build_type = build_type
    self.adb = android_commands.AndroidCommands(self.device_id)
    self.push_deps = push_deps
    self.cleanup_test_files = cleanup_test_files

  def TearDown(self):
    pass

  def GetOutDir(self):
    return os.path.join(os.environ['CHROME_SRC'], 'out',
                        self.build_type)

  def Run(self):
    logging.info('Running host-driven test: %s', self.tagged_name)
    # Get the test method on the derived class and execute it
    return getattr(self, self.test_name)()

  def __RunJavaTest(self, package_name, test_case, test_method):
    """Runs a single Java test method with a Java TestRunner.

    Args:
      package_name: Package name in which the java tests live
          (e.g. foo.bar.baz.tests)
      test_case: Name of the Java test case (e.g. FooTest)
      test_method: Name of the test method to run (e.g. testFooBar)

    Returns:
      TestRunResults object with a single test result.
    """
    test = '%s.%s#%s' % (package_name, test_case, test_method)
    test_pkg = test_package.TestPackage(
        self.instrumentation_options.test_apk_path,
        self.instrumentation_options.test_apk_jar_path)
    java_test_runner = test_runner.TestRunner(self.instrumentation_options,
                                              self.device_id,
                                              self.shard_index, test_pkg,
                                              self.ports_to_forward)
    try:
      java_test_runner.SetUp()
      return java_test_runner.RunTest(test)[0]
    finally:
      java_test_runner.TearDown()

  def _RunJavaTests(self, package_name, tests):
    """Calls a list of tests and stops at the first test failure.

    This method iterates until either it encounters a non-passing test or it
    exhausts the list of tests. Then it returns the appropriate overall result.

    Test cases may make use of this method internally to assist in running
    instrumentation tests. This function relies on instrumentation_options
    being defined.

    Args:
      package_name: Package name in which the java tests live
          (e.g. foo.bar.baz.tests)
      tests: A list of Java test names which will be run

    Returns:
      A TestRunResults object containing an overall result for this set of Java
      tests. If any Java tests do not pass, this is a fail overall.
    """
    test_type = base_test_result.ResultType.PASS
    log = ''

    start_ms = int(time.time()) * 1000
    for test in tests:
      # We're only running one test at a time, so this TestRunResults object
      # will hold only one result.
      suite, test_name = test.split('.')
      java_result = self.__RunJavaTest(package_name, suite, test_name)
      assert len(java_result.GetAll()) == 1
      if not java_result.DidRunPass():
        result = java_result.GetNotPass().pop()
        log = result.GetLog()
        test_type = result.GetType()
        break
    duration_ms = int(time.time()) * 1000 - start_ms

    overall_result = base_test_result.TestRunResults()
    overall_result.AddResult(
        test_result.InstrumentationTestResult(
            self.tagged_name, test_type, start_ms, duration_ms, log=log))
    return overall_result

  def __str__(self):
    return self.tagged_name

  def __repr__(self):
    return self.tagged_name
