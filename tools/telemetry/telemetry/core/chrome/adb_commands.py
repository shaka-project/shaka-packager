# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Brings in Chrome Android's android_commands module, which itself is a
thin(ish) wrapper around adb."""
import os
import sys

# This is currently a thin wrapper around Chrome Android's
# build scripts, located in chrome/build/android. This file exists mainly to
# deal with locating the module.

# Get build/android scripts into our path.
sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     '..', '..', '..', '..', '..', 'build', 'android')))
try:
  from pylib import android_commands # pylint: disable=F0401
  from pylib import cmd_helper # pylint: disable=F0401
  from pylib import forwarder # pylint: disable=F0401
  from pylib import ports # pylint: disable=F0401
except Exception:
  android_commands = None


def IsAndroidSupported():
  return android_commands != None


def GetAttachedDevices():
  """Returns a list of attached, online android devices.

  If a preferred device has been set with ANDROID_SERIAL, it will be first in
  the returned list."""
  return android_commands.GetAttachedDevices()


def AllocateTestServerPort():
  return ports.AllocateTestServerPort()


def ResetTestServerPortAllocation():
  return ports.ResetTestServerPortAllocation()


def GetOutDirectory():
  return cmd_helper.OutDirectory.get()


class AdbCommands(object):
  """A thin wrapper around ADB"""

  def __init__(self, device):
    self._adb = android_commands.AndroidCommands(device)
    self._device = device

  def device(self):
    return self._device

  def Adb(self):
    return self._adb

  def Forward(self, local, remote):
    ret = self._adb.Adb().SendCommand('forward %s %s' % (local, remote))
    assert ret == ''

  def RunShellCommand(self, command, timeout_time=20, log_result=False):
    """Send a command to the adb shell and return the result.

    Args:
      command: String containing the shell command to send. Must not include
               the single quotes as we use them to escape the whole command.
      timeout_time: Number of seconds to wait for command to respond before
        retrying, used by AdbInterface.SendShellCommand.
      log_result: Boolean to indicate whether we should log the result of the
                  shell command.

    Returns:
      list containing the lines of output received from running the command
    """
    return self._adb.RunShellCommand(command, timeout_time, log_result)

  def CloseApplication(self, package):
    """Attempt to close down the application, using increasing violence.

    Args:
      package: Name of the process to kill off, e.g.
      com.google.android.apps.chrome
    """
    self._adb.CloseApplication(package)

  def KillAll(self, process):
    """Android version of killall, connected via adb.

    Args:
      process: name of the process to kill off

    Returns:
      the number of processess killed
    """
    return self._adb.KillAll(process)

  def ExtractPid(self, process_name):
    """Extracts Process Ids for a given process name from Android Shell.

    Args:
      process_name: name of the process on the device.

    Returns:
      List of all the process ids (as strings) that match the given name.
      If the name of a process exactly matches the given name, the pid of
      that process will be inserted to the front of the pid list.
    """
    return self._adb.ExtractPid(process_name)

  def StartActivity(self, package, activity, wait_for_completion=False,
                    action='android.intent.action.VIEW',
                    category=None, data=None,
                    extras=None, trace_file_name=None):
    """Starts |package|'s activity on the device.

    Args:
      package: Name of package to start (e.g. 'com.google.android.apps.chrome').
      activity: Name of activity (e.g. '.Main' or
        'com.google.android.apps.chrome.Main').
      wait_for_completion: wait for the activity to finish launching (-W flag).
      action: string (e.g. 'android.intent.action.MAIN'). Default is VIEW.
      category: string (e.g. 'android.intent.category.HOME')
      data: Data string to pass to activity (e.g. 'http://www.example.com/').
      extras: Dict of extras to pass to activity. Values are significant.
      trace_file_name: If used, turns on and saves the trace to this file name.
    """
    return self._adb.StartActivity(package, activity, wait_for_completion,
                    action,
                    category, data,
                    extras, trace_file_name)

  def Push(self, local, remote):
    return self._adb.Adb().Push(local, remote)

  def Pull(self, remote, local):
    return self._adb.Adb().Pull(remote, local)

  def FileExistsOnDevice(self, file_name):
    return self._adb.FileExistsOnDevice(file_name)

  def IsRootEnabled(self):
    return self._adb.IsRootEnabled()

def HasForwarder(buildtype=None):
  if not buildtype:
    return (HasForwarder(buildtype='Release') or
            HasForwarder(buildtype='Debug'))
  return (os.path.exists(os.path.join(GetOutDirectory(), buildtype,
                                      'device_forwarder')) and
          os.path.exists(os.path.join(GetOutDirectory(), buildtype,
                                      'host_forwarder')))

class Forwarder(object):
  def __init__(self, adb, *port_pairs):
    self._adb = adb.Adb()
    self._host_port = port_pairs[0].local_port

    new_port_pairs = [(port_pair.local_port, port_pair.remote_port)
                      for port_pair in port_pairs]

    self._port_pairs = new_port_pairs
    forwarder.Forwarder.Map(new_port_pairs, self._adb)

  @staticmethod
  def _GetBuildType():
    assert HasForwarder()
    return 'Debug' if HasForwarder('Debug') else 'Release'

  @property
  def url(self):
    return 'http://localhost:%i' % self._host_port

  def Close(self):
    for (device_port, _) in self._port_pairs:
      forwarder.Forwarder.UnmapDevicePort(device_port, self._adb)
