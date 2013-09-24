# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class for running uiautomator tests on a single device."""

from pylib.instrumentation import test_options as instr_test_options
from pylib.instrumentation import test_runner as instr_test_runner


class TestRunner(instr_test_runner.TestRunner):
  """Responsible for running a series of tests connected to a single device."""

  def __init__(self, test_options, device, shard_index, test_pkg,
               ports_to_forward):
    """Create a new TestRunner.

    Args:
      test_options: A UIAutomatorOptions object.
      device: Attached android device.
      shard_index: Shard index.
      test_pkg: A TestPackage object.
      ports_to_forward: A list of port numbers for which to set up forwarders.
          Can be optionally requested by a test case.
    """
    # Create an InstrumentationOptions object to pass to the super class
    instrumentation_options = instr_test_options.InstrumentationOptions(
        test_options.build_type,
        test_options.tool,
        test_options.cleanup_test_files,
        test_options.push_deps,
        test_options.annotations,
        test_options.exclude_annotations,
        test_options.test_filter,
        test_options.test_data,
        test_options.save_perf_json,
        test_options.screenshot_failures,
        wait_for_debugger=False,
        test_apk=None,
        test_apk_path=None,
        test_apk_jar_path=None)
    super(TestRunner, self).__init__(instrumentation_options, device,
                                     shard_index, test_pkg, ports_to_forward)

    self.package_name = test_options.package_name

  #override
  def InstallTestPackage(self):
    self.test_pkg.Install(self.adb)

  #override
  def PushDataDeps(self):
    pass

  #override
  def _RunTest(self, test, timeout):
    self.adb.ClearApplicationState(self.package_name)
    if 'Feature:FirstRunExperience' in self.test_pkg.GetTestAnnotations(test):
      self.flags.RemoveFlags(['--disable-fre'])
    else:
      self.flags.AddFlags(['--disable-fre'])
    return self.adb.RunUIAutomatorTest(
        test, self.test_pkg.GetPackageName(), timeout)
