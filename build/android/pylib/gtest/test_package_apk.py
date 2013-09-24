# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines TestPackageApk to help run APK-based native tests."""

import logging
import os
import shlex
import sys
import tempfile
import time

from pylib import android_commands
from pylib import cmd_helper
from pylib import constants
from pylib import pexpect
from pylib.android_commands import errors

from test_package import TestPackage


class TestPackageApk(TestPackage):
  """A helper class for running APK-based native tests."""

  def __init__(self, suite_name, build_type):
    """
    Args:
      suite_name: Name of the test suite (e.g. base_unittests).
      build_type: 'Release' or 'Debug'.
    """
    TestPackage.__init__(self, suite_name)
    product_dir = os.path.join(cmd_helper.OutDirectory.get(), build_type)
    if suite_name == 'content_browsertests':
      self.suite_path = os.path.join(
          product_dir, 'apks', '%s.apk' % suite_name)
      self._test_apk_package_name = constants.BROWSERTEST_TEST_PACKAGE_NAME
      self._test_activity_name = constants.BROWSERTEST_TEST_ACTIVITY_NAME
      self._command_line_file = constants.BROWSERTEST_COMMAND_LINE_FILE
    else:
      self.suite_path = os.path.join(
          product_dir, '%s_apk' % suite_name, '%s-debug.apk' % suite_name)
      self._test_apk_package_name = constants.GTEST_TEST_PACKAGE_NAME
      self._test_activity_name = constants.GTEST_TEST_ACTIVITY_NAME
      self._command_line_file = constants.GTEST_COMMAND_LINE_FILE

  def _CreateCommandLineFileOnDevice(self, adb, options):
    command_line_file = tempfile.NamedTemporaryFile()
    # GTest expects argv[0] to be the executable path.
    command_line_file.write(self.suite_name + ' ' + options)
    command_line_file.flush()
    adb.PushIfNeeded(command_line_file.name,
                          constants.TEST_EXECUTABLE_DIR + '/' +
                          self._command_line_file)

  def _GetFifo(self):
    # The test.fifo path is determined by:
    # testing/android/java/src/org/chromium/native_test/
    #     ChromeNativeTestActivity.java and
    # testing/android/native_test_launcher.cc
    return '/data/data/' + self._test_apk_package_name + '/files/test.fifo'

  def _ClearFifo(self, adb):
    adb.RunShellCommand('rm -f ' + self._GetFifo())

  def _WatchFifo(self, adb, timeout, logfile=None):
    for i in range(10):
      if adb.FileExistsOnDevice(self._GetFifo()):
        logging.info('Fifo created.')
        break
      time.sleep(i)
    else:
      raise errors.DeviceUnresponsiveError(
          'Unable to find fifo on device %s ' % self._GetFifo())
    args = shlex.split(adb.Adb()._target_arg)
    args += ['shell', 'cat', self._GetFifo()]
    return pexpect.spawn('adb', args, timeout=timeout, logfile=logfile)

  def _StartActivity(self, adb):
    adb.StartActivity(
        self._test_apk_package_name,
        self._test_activity_name,
        wait_for_completion=True,
        action='android.intent.action.MAIN',
        force_stop=True)

  #override
  def ClearApplicationState(self, adb):
    adb.ClearApplicationState(self._test_apk_package_name)
    # Content shell creates a profile on the sdscard which accumulates cache
    # files over time.
    if self.suite_name == 'content_browsertests':
      adb.RunShellCommand(
          'rm -r %s/content_shell' % adb.GetExternalStorage(),
          timeout_time=60 * 2)

  #override
  def CreateCommandLineFileOnDevice(self, adb, test_filter, test_arguments):
    self._CreateCommandLineFileOnDevice(
        adb, '--gtest_filter=%s %s' % (test_filter, test_arguments))

  #override
  def GetAllTests(self, adb):
    self._CreateCommandLineFileOnDevice(adb, '--gtest_list_tests')
    try:
      self.tool.SetupEnvironment()
      # Clear and start monitoring logcat.
      self._ClearFifo(adb)
      self._StartActivity(adb)
      # Wait for native test to complete.
      p = self._WatchFifo(adb, timeout=30 * self.tool.GetTimeoutScale())
      p.expect('<<ScopedMainEntryLogger')
      p.close()
    finally:
      self.tool.CleanUpEnvironment()
    # We need to strip the trailing newline.
    content = [line.rstrip() for line in p.before.splitlines()]
    return self._ParseGTestListTests(content)

  #override
  def SpawnTestProcess(self, adb):
    try:
      self.tool.SetupEnvironment()
      self._ClearFifo(adb)
      self._StartActivity(adb)
    finally:
      self.tool.CleanUpEnvironment()
    logfile = android_commands.NewLineNormalizer(sys.stdout)
    return self._WatchFifo(adb, timeout=10, logfile=logfile)

  #override
  def Install(self, adb):
    self.tool.CopyFiles()
    adb.ManagedInstall(self.suite_path, False,
                            package_name=self._test_apk_package_name)
