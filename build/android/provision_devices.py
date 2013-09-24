#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provisions Android devices with settings required for bots.

Usage:
  ./provision_devices.py [-d <device serial number>]
"""

import optparse
import os
import re
import subprocess
import sys
import time

from pylib import android_commands
from pylib import constants

def KillHostHeartbeat():
  ps = subprocess.Popen(['ps', 'aux'], stdout = subprocess.PIPE)
  stdout, _ = ps.communicate()
  matches = re.findall('\\n.*host_heartbeat.*', stdout)
  for match in matches:
    print 'An instance of host heart beart running... will kill'
    pid = re.findall('(\d+)', match)[1]
    subprocess.call(['kill', str(pid)])


def LaunchHostHeartbeat():
  # Kill if existing host_heartbeat
  KillHostHeartbeat()
  # Launch a new host_heartbeat
  print 'Spawning host heartbeat...'
  subprocess.Popen([os.path.join(constants.DIR_SOURCE_ROOT,
                                 'build/android/host_heartbeat.py')])


def PushAndLaunchAdbReboot(devices, target):
  """Pushes and launches the adb_reboot binary on the device.

  Arguments:
    devices: The list of serial numbers of the device to which the
             adb_reboot binary should be pushed.
    target : The build target (example, Debug or Release) which helps in
             locating the adb_reboot binary.
  """
  for device in devices:
    print 'Will push and launch adb_reboot on %s' % device
    android_cmd = android_commands.AndroidCommands(device)
    # Kill if adb_reboot is already running.
    android_cmd.KillAllBlocking('adb_reboot', 2)
    # Push adb_reboot
    print '  Pushing adb_reboot ...'
    adb_reboot = os.path.join(constants.DIR_SOURCE_ROOT,
                              'out/%s/adb_reboot' % target)
    android_cmd.PushIfNeeded(adb_reboot, '/data/local/tmp/')
    # Launch adb_reboot
    print '  Launching adb_reboot ...'
    p = subprocess.Popen(['adb', '-s', device, 'shell'], stdin=subprocess.PIPE)
    p.communicate('/data/local/tmp/adb_reboot; exit\n')
  LaunchHostHeartbeat()


def ProvisionDevices(options):
  if options.device is not None:
    devices = [options.device]
  else:
    devices = android_commands.GetAttachedDevices()
  for device in devices:
    android_cmd = android_commands.AndroidCommands(device)
    android_cmd.RunShellCommand('su -c date -u %f' % time.time())
  if options.auto_reconnect:
    PushAndLaunchAdbReboot(devices, options.target)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option('-d', '--device',
                    help='The serial number of the device to be provisioned')
  parser.add_option('-t', '--target', default='Debug', help='The build target')
  parser.add_option(
      '-r', '--auto-reconnect', action='store_true',
      help='Push binary which will reboot the device on adb disconnections.')
  options, args = parser.parse_args(argv[1:])

  if args:
    print >> sys.stderr, 'Unused args %s' % args
    return 1

  ProvisionDevices(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
