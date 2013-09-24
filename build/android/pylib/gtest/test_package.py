# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class representing GTest test packages."""

import logging


class TestPackage(object):

  """A helper base class for both APK and stand-alone executables.

  Args:
    suite_name: Name of the test suite (e.g. base_unittests).
  """
  def __init__(self, suite_name):
    self.suite_name = suite_name

  def ClearApplicationState(self, adb):
    """Clears the application state.

    Args:
      adb: Instance of AndroidCommands.
    """
    raise NotImplementedError('Method must be overriden.')

  def CreateCommandLineFileOnDevice(self, adb, test_filter, test_arguments):
    """Creates a test runner script and pushes to the device.

    Args:
      adb: Instance of AndroidCommands.
      test_filter: A test_filter flag.
      test_arguments: Additional arguments to pass to the test binary.
    """
    raise NotImplementedError('Method must be overriden.')

  def GetAllTests(self, adb):
    """Returns a list of all tests available in the test suite.

    Args:
      adb: Instance of AndroidCommands.
    """
    raise NotImplementedError('Method must be overriden.')

  def GetGTestReturnCode(self, adb):
    return None

  def SpawnTestProcess(self, adb):
    """Spawn the test process.

    Args:
      adb: Instance of AndroidCommands.

    Returns:
      An instance of pexpect spawn class.
    """
    raise NotImplementedError('Method must be overriden.')

  def Install(self, adb):
    """Install the test package to the device.

    Args:
      adb: Instance of AndroidCommands.
    """
    raise NotImplementedError('Method must be overriden.')

  def _ParseGTestListTests(self, raw_list):
    """Parses a raw test list as provided by --gtest_list_tests.

    Args:
      raw_list: The raw test listing with the following format:

      IPCChannelTest.
        SendMessageInChannelConnected
      IPCSyncChannelTest.
        Simple
        DISABLED_SendWithTimeoutMixedOKAndTimeout

    Returns:
      A list of all tests. For the above raw listing:

      [IPCChannelTest.SendMessageInChannelConnected, IPCSyncChannelTest.Simple,
       IPCSyncChannelTest.DISABLED_SendWithTimeoutMixedOKAndTimeout]
    """
    ret = []
    current = ''
    for test in raw_list:
      if not test:
        continue
      if test[0] != ' ' and not test.endswith('.'):
        # Ignore any lines with unexpected format.
        continue
      if test[0] != ' ' and test.endswith('.'):
        current = test
        continue
      if 'YOU HAVE' in test:
        break
      test_name = test[2:]
      ret += [current + test_name]
    return ret
