# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" A simple device interface for build steps.

"""

import logging
import os
import re
import sys

import build_utils

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..', '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import android_commands

from pylib.android_commands import GetAttachedDevices


class BuildDevice(object):
  def __init__(self, configuration):
    self.id = configuration['id']
    self.description = configuration['description']
    self.install_metadata = configuration['install_metadata']
    self.adb = android_commands.AndroidCommands(self.id)

  def RunShellCommand(self, *args, **kwargs):
    return self.adb.RunShellCommand(*args, **kwargs)

  def PushIfNeeded(self, *args, **kwargs):
    return self.adb.PushIfNeeded(*args, **kwargs)

  def GetSerialNumber(self):
    return self.id

  def Install(self, *args, **kwargs):
    return self.adb.Install(*args, **kwargs)

  def GetInstallMetadata(self, apk_package):
    """Gets the metadata on the device for the apk_package apk."""
    # Matches lines like:
    # -rw-r--r-- system   system    7376582 2013-04-19 16:34 \
    #   org.chromium.chrome.testshell.apk
    # -rw-r--r-- system   system    7376582 2013-04-19 16:34 \
    #   org.chromium.chrome.testshell-1.apk
    apk_matcher = lambda s: re.match('.*%s(-[0-9]*)?.apk$' % apk_package, s)
    matches = filter(apk_matcher, self.install_metadata)
    return matches[0] if matches else None


def GetConfigurationForDevice(id):
  adb = android_commands.AndroidCommands(id)
  configuration = None
  has_root = False
  is_online = adb.IsOnline()
  if is_online:
    cmd = 'ls -l /data/app; getprop ro.build.description'
    cmd_output = adb.RunShellCommand(cmd)
    has_root = not 'Permission denied' in cmd_output[0]
    if not has_root:
      # Disable warning log messages from EnableAdbRoot()
      logging.getLogger().disabled = True
      has_root = adb.EnableAdbRoot()
      logging.getLogger().disabled = False
      cmd_output = adb.RunShellCommand(cmd)

    configuration = {
        'id': id,
        'description': cmd_output[-1],
        'install_metadata': cmd_output[:-1],
      }
  return configuration, is_online, has_root


def WriteConfigurations(configurations, path):
  # Currently we only support installing to the first device.
  build_utils.WriteJson(configurations[:1], path, only_if_changed=True)


def ReadConfigurations(path):
  return build_utils.ReadJson(path)


def GetBuildDevice(configurations):
  assert len(configurations) == 1
  return BuildDevice(configurations[0])


def GetBuildDeviceFromPath(path):
  configurations = ReadConfigurations(path)
  if len(configurations) > 0:
    return GetBuildDevice(ReadConfigurations(path))
  return None

