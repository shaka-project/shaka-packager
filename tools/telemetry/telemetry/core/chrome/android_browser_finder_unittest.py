# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core import browser_options
from telemetry.core.chrome import android_browser_finder
from telemetry.unittest import system_stub

class LoggingStub(object):
  def __init__(self):
    self.warnings = []

  def info(self, msg, *args):
    pass

  def warn(self, msg, *args):
    self.warnings.append(msg % args)

class AndroidBrowserFinderTest(unittest.TestCase):
  def setUp(self):
    self._stubs = system_stub.Override(android_browser_finder,
                                       ['adb_commands', 'subprocess'])
    android_browser_finder.adb_works = None  # Blow cache between runs.

  def tearDown(self):
    self._stubs.Restore()

  def test_no_adb(self):
    options = browser_options.BrowserOptions()

    def NoAdb(*args, **kargs): # pylint: disable=W0613
      raise OSError('not found')
    self._stubs.subprocess.Popen = NoAdb
    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(0, len(browsers))

  def test_adb_no_devices(self):
    options = browser_options.BrowserOptions()

    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(0, len(browsers))


  def test_adb_permissions_error(self):
    options = browser_options.BrowserOptions()

    self._stubs.subprocess.Popen.communicate_result = (
        """List of devices attached
????????????\tno permissions""",
        """* daemon not running. starting it now on port 5037 *
* daemon started successfully *
""")

    log_stub = LoggingStub()
    browsers = android_browser_finder.FindAllAvailableBrowsers(
      options, log_stub)
    self.assertEquals(3, len(log_stub.warnings))
    self.assertEquals(0, len(browsers))


  def test_adb_two_devices(self):
    options = browser_options.BrowserOptions()

    self._stubs.adb_commands.attached_devices = ['015d14fec128220c',
                                                 '015d14fec128220d']

    log_stub = LoggingStub()
    browsers = android_browser_finder.FindAllAvailableBrowsers(
      options, log_stub)
    self.assertEquals(1, len(log_stub.warnings))
    self.assertEquals(0, len(browsers))

  def test_adb_one_device(self):
    options = browser_options.BrowserOptions()

    self._stubs.adb_commands.attached_devices = ['015d14fec128220c']

    def OnPM(args):
      assert args[0] == 'pm'
      assert args[1] == 'list'
      assert args[2] == 'packages'
      return ['package:org.chromium.content_shell_apk',
              'package.com.google.android.setupwizard']

    self._stubs.adb_commands.shell_command_handlers['pm'] = OnPM

    browsers = android_browser_finder.FindAllAvailableBrowsers(options)
    self.assertEquals(1, len(browsers))
