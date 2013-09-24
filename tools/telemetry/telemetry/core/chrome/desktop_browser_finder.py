# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds desktop browsers that can be controlled by telemetry."""

import logging
from operator import attrgetter
import os
import platform
import subprocess
import sys

from telemetry.core import browser
from telemetry.core import platform as core_platform
from telemetry.core import possible_browser
from telemetry.core import profile_types
from telemetry.core import util
from telemetry.core.chrome import cros_interface
from telemetry.core.chrome import desktop_browser_backend

ALL_BROWSER_TYPES = ','.join([
    'exact',
    'release',
    'release_x64',
    'debug',
    'debug_x64',
    'canary',
    'content-shell-debug',
    'content-shell-release',
    'system'])

class PossibleDesktopBrowser(possible_browser.PossibleBrowser):
  """A desktop browser that can be controlled."""

  def __init__(self, browser_type, options, executable, flash_path,
               is_content_shell, browser_directory, is_local_build=False):
    super(PossibleDesktopBrowser, self).__init__(browser_type, options)
    self._local_executable = executable
    self._flash_path = flash_path
    self._is_content_shell = is_content_shell
    self._browser_directory = browser_directory
    self.is_local_build = is_local_build

  def __repr__(self):
    return 'PossibleDesktopBrowser(browser_type=%s)' % self.browser_type

  # Constructs a browser.
  # Returns a touple of the form: (browser, backend)
  def _CreateBrowserInternal(self, delete_profile_dir_after_run):
    backend = desktop_browser_backend.DesktopBrowserBackend(
        self._options, self._local_executable, self._flash_path,
        self._is_content_shell, self._browser_directory,
        delete_profile_dir_after_run=delete_profile_dir_after_run)
    b = browser.Browser(backend,
                        core_platform.CreatePlatformBackendForCurrentOS())
    return b

  def Create(self):
    # If a dirty profile is needed, instantiate an initial browser object and
    # use that to create a dirty profile.
    creator_class = profile_types.GetProfileCreator(self.options.profile_type)
    if creator_class:
      logging.info(
          'Creating a dirty profile of type: %s', self.options.profile_type)
      (b, backend) = \
          self._CreateBrowserInternal(delete_profile_dir_after_run=False)
      with b as b:
        creator = creator_class(b)
        creator.CreateProfile()
        dirty_profile_dir = backend.profile_directory
        logging.info(
            "Dirty profile created succesfully in '%s'", dirty_profile_dir)

      # Now create another browser to run tests on using the dirty profile
      # we just created.
      b = self._CreateBrowserInternal(delete_profile_dir_after_run=True)
      backend.SetProfileDirectory(dirty_profile_dir)
    else:
      b = self._CreateBrowserInternal(delete_profile_dir_after_run=True)
    return b

  def SupportsOptions(self, options):
    if (len(options.extensions_to_load) != 0) and self._is_content_shell:
      return False
    return True

  @property
  def last_modification_time(self):
    if os.path.exists(self._local_executable):
      return os.path.getmtime(self._local_executable)
    return -1

def SelectDefaultBrowser(possible_browsers):
  local_builds_by_date = [
      b for b in sorted(possible_browsers,
                        key=attrgetter('last_modification_time'))
      if b.is_local_build]
  if local_builds_by_date:
    return local_builds_by_date[-1]
  return None

def CanFindAvailableBrowsers():
  return not cros_interface.IsRunningOnCrosDevice()

def FindAllAvailableBrowsers(options):
  """Finds all the desktop browsers available on this machine."""
  browsers = []

  if not CanFindAvailableBrowsers():
    return []

  has_display = True
  if (sys.platform.startswith('linux') and
      os.getenv('DISPLAY') == None):
    has_display = False

  # Look for a browser in the standard chrome build locations.
  if options.chrome_root:
    chrome_root = options.chrome_root
  else:
    chrome_root = util.GetChromiumSrcDir()

  if sys.platform == 'darwin':
    chromium_app_name = 'Chromium.app/Contents/MacOS/Chromium'
    content_shell_app_name = 'Content Shell.app/Contents/MacOS/Content Shell'
    mac_dir = 'mac'
    if platform.architecture()[0] == '64bit':
      mac_dir = 'mac_64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        mac_dir, 'PepperFlashPlayer.plugin')
  elif sys.platform.startswith('linux'):
    chromium_app_name = 'chrome'
    content_shell_app_name = 'content_shell'
    linux_dir = 'linux'
    if platform.architecture()[0] == '64bit':
      linux_dir = 'linux_x64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        linux_dir, 'libpepflashplayer.so')
  elif sys.platform.startswith('win'):
    chromium_app_name = 'chrome.exe'
    content_shell_app_name = 'content_shell.exe'
    win_dir = 'win'
    if platform.architecture()[0] == '64bit':
      win_dir = 'win_x64'
    flash_path = os.path.join(
        chrome_root, 'third_party', 'adobe', 'flash', 'binaries', 'ppapi',
        win_dir, 'pepflashplayer.dll')
  else:
    raise Exception('Platform not recognized')

  # Add the explicit browser executable if given.
  if options.browser_executable:
    normalized_executable = os.path.expanduser(options.browser_executable)
    if os.path.exists(normalized_executable):
      browser_directory = os.path.dirname(options.browser_executable)
      browsers.append(PossibleDesktopBrowser('exact', options,
                                             normalized_executable, flash_path,
                                             False, browser_directory))
    else:
      logging.warning('%s specified by browser_executable does not exist',
                      normalized_executable)

  def AddIfFound(browser_type, build_dir, type_dir, app_name, content_shell):
    browser_directory = os.path.join(chrome_root, build_dir, type_dir)
    app = os.path.join(browser_directory, app_name)
    if os.path.exists(app):
      browsers.append(PossibleDesktopBrowser(browser_type, options,
                                             app, flash_path, content_shell,
                                             browser_directory,
                                             is_local_build=True))
      return True
    return False

  # Add local builds
  for build_dir, build_type in util.GetBuildDirectories():
    AddIfFound(build_type.lower(), build_dir, build_type,
               chromium_app_name, False)
    AddIfFound('content-shell-' + build_type.lower(), build_dir, build_type,
               content_shell_app_name, True)

  # Mac-specific options.
  if sys.platform == 'darwin':
    mac_canary_root = '/Applications/Google Chrome Canary.app/'
    mac_canary = mac_canary_root + 'Contents/MacOS/Google Chrome Canary'
    mac_system_root = '/Applications/Google Chrome.app'
    mac_system = mac_system_root + '/Contents/MacOS/Google Chrome'
    if os.path.exists(mac_canary):
      browsers.append(PossibleDesktopBrowser('canary', options,
                                             mac_canary, None, False,
                                             mac_canary_root))

    if os.path.exists(mac_system):
      browsers.append(PossibleDesktopBrowser('system', options,
                                             mac_system, None, False,
                                             mac_system_root))

  # Linux specific options.
  if sys.platform.startswith('linux'):
    # Look for a google-chrome instance.
    found = False
    try:
      with open(os.devnull, 'w') as devnull:
        found = subprocess.call(['google-chrome', '--version'],
                                stdout=devnull, stderr=devnull) == 0
    except OSError:
      pass
    if found:
      browsers.append(PossibleDesktopBrowser('system', options,
                                             'google-chrome', None, False,
                                             '/opt/google/chrome'))

  # Win32-specific options.
  if sys.platform.startswith('win'):
    system_path = os.path.join('Google', 'Chrome', 'Application')
    canary_path = os.path.join('Google', 'Chrome SxS', 'Application')

    win_search_paths = [os.getenv('PROGRAMFILES(X86)'),
                        os.getenv('PROGRAMFILES'),
                        os.getenv('LOCALAPPDATA')]

    def AddIfFoundWin(browser_name, app_path):
      browser_directory = os.path.join(path, app_path)
      app = os.path.join(browser_directory, chromium_app_name)
      if os.path.exists(app):
        browsers.append(PossibleDesktopBrowser(browser_name, options,
                                               app, flash_path, False,
                                               browser_directory))
        return True
      return False

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFoundWin('canary', canary_path):
        break

    for path in win_search_paths:
      if not path:
        continue
      if AddIfFoundWin('system', system_path):
        break

  if len(browsers) and not has_display:
    logging.warning(
      'Found (%s), but you do not have a DISPLAY environment set.' %
      ','.join([b.browser_type for b in browsers]))
    return []

  return browsers
