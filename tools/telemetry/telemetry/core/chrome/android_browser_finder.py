# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds android browsers that can be controlled by telemetry."""

import os
import logging as real_logging
import re
import subprocess
import sys

from telemetry.core import browser
from telemetry.core import possible_browser
from telemetry.core import profile_types
from telemetry.core.chrome import adb_commands
from telemetry.core.chrome import android_browser_backend
from telemetry.core.platform import android_platform_backend

CHROME_PACKAGE_NAMES = {
  'android-chrome': 'com.google.android.apps.chrome',
  'android-chrome-beta': 'com.chrome.beta',
  'android-chrome-dev': 'com.google.android.apps.chrome_dev',
  'android-jb-system-chrome': 'com.android.chrome'
}

ALL_BROWSER_TYPES = ','.join([
                                'android-chromium-testshell',
                                'android-content-shell',
                                'android-webview',
                             ] + CHROME_PACKAGE_NAMES.keys())

CONTENT_SHELL_PACKAGE = 'org.chromium.content_shell_apk'
CHROMIUM_TESTSHELL_PACKAGE = 'org.chromium.chrome.testshell'
WEBVIEW_PACKAGE = 'com.android.webview.chromium.shell'


# adb shell pm list packages
# adb
# intents to run (pass -D url for the rest)
#   com.android.chrome/.Main
#   com.google.android.apps.chrome/.Main

class PossibleAndroidBrowser(possible_browser.PossibleBrowser):
  """A launchable android browser instance."""
  def __init__(self, browser_type, options, backend_settings):
    super(PossibleAndroidBrowser, self).__init__(browser_type, options)
    self._backend_settings = backend_settings

  def __repr__(self):
    return 'PossibleAndroidBrowser(browser_type=%s)' % self.browser_type

  def Create(self):
    if profile_types.GetProfileCreator(self.options.profile_type):
      raise Exception("Profile creation not currently supported on Android")

    backend = android_browser_backend.AndroidBrowserBackend(
        self._options, self._backend_settings)
    platform_backend = android_platform_backend.AndroidPlatformBackend(
        self._backend_settings.adb.Adb(), self._options.no_performance_mode)
    b = browser.Browser(backend, platform_backend)
    return b

  def SupportsOptions(self, options):
    if len(options.extensions_to_load) != 0:
      return False
    return True

def SelectDefaultBrowser(_):
  return None

adb_works = None
def CanFindAvailableBrowsers(logging=real_logging):
  if not adb_commands.IsAndroidSupported():
    return False

  global adb_works

  if adb_works == None:
    try:
      with open(os.devnull, 'w') as devnull:
        proc = subprocess.Popen(['adb', 'devices'],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                stdin=devnull)
        stdout, _ = proc.communicate()
        if re.search(re.escape('????????????\tno permissions'), stdout) != None:
          logging.warn(
              ('adb devices reported a permissions error. Consider '
               'restarting adb as root:'))
          logging.warn('  adb kill-server')
          logging.warn('  sudo `which adb` devices\n\n')
        adb_works = True
    except OSError:
      platform_tools_path = os.path.join(
          os.path.dirname(__file__), '..', '..', '..', '..', '..'
          'third_party', 'android_tools', 'sdk', 'platform-tools')
      if (sys.platform.startswith('linux') and
          os.path.exists(os.path.join(platform_tools_path, 'adb'))):
        os.environ['PATH'] = os.pathsep.join([platform_tools_path,
                                              os.environ['PATH']])
        adb_works = True
      else:
        adb_works = False
  if adb_works and sys.platform.startswith('linux'):
    # Workaround for crbug.com/268450
    import psutil
    adb_commands.GetAttachedDevices()
    pids  = [p.pid for p in psutil.process_iter() if 'adb' in p.name]
    with open(os.devnull, 'w') as devnull:
      for pid in pids:
        subprocess.check_call(['taskset', '-p', '0x1', str(pid)],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE,
                              stdin=devnull)

  return adb_works

def FindAllAvailableBrowsers(options, logging=real_logging):
  """Finds all the desktop browsers available on this machine."""
  if not CanFindAvailableBrowsers(logging=logging):
    logging.info('No adb command found. ' +
                 'Will not try searching for Android browsers.')
    return []

  device = None
  if options.android_device:
    devices = [options.android_device]
  else:
    devices = adb_commands.GetAttachedDevices()

  if len(devices) == 0:
    logging.info('No android devices found.')
    return []

  if len(devices) > 1:
    logging.warn('Multiple devices attached. ' +
                 'Please specify a device explicitly.')
    return []

  device = devices[0]

  adb = adb_commands.AdbCommands(device=device)

  packages = adb.RunShellCommand('pm list packages')
  possible_browsers = []
  if 'package:' + CONTENT_SHELL_PACKAGE in packages:
    b = PossibleAndroidBrowser(
        'android-content-shell',
        options, android_browser_backend.ContentShellBackendSettings(
            adb, CONTENT_SHELL_PACKAGE))
    possible_browsers.append(b)

  if 'package:' + CHROMIUM_TESTSHELL_PACKAGE in packages:
    b = PossibleAndroidBrowser(
        'android-chromium-testshell',
        options, android_browser_backend.ChromiumTestShellBackendSettings(
            adb, CHROMIUM_TESTSHELL_PACKAGE))
    possible_browsers.append(b)

  if 'package:' + WEBVIEW_PACKAGE in packages:
    b = PossibleAndroidBrowser(
        'android-webview',
        options,
        android_browser_backend.WebviewBackendSettings(adb, WEBVIEW_PACKAGE))
    possible_browsers.append(b)

  for name, package in CHROME_PACKAGE_NAMES.iteritems():
    if 'package:' + package in packages:
      b = PossibleAndroidBrowser(
          name,
          options,
          android_browser_backend.ChromeBackendSettings(adb, package))
      possible_browsers.append(b)

  # See if the "forwarder" is installed -- we need this to host content locally
  # but make it accessible to the device.
  if len(possible_browsers) and not adb_commands.HasForwarder():
    logging.warn('telemetry detected an android device. However,')
    logging.warn('Chrome\'s port-forwarder app is not available.')
    logging.warn('To build:')
    logging.warn('  ninja -C out/Release forwarder2 md5sum')
    logging.warn('')
    logging.warn('')
    return []
  return possible_browsers
