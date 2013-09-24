# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import signal
import subprocess
import sys
import tempfile

from telemetry.core.chrome import android_browser_finder
from telemetry.core.platform import profiler


_TCP_DUMP_BASE_OPTS = ['-i', 'any', '-p', '-s', '0', '-w']

class _TCPDumpProfilerAndroid(object):
  """An internal class to collect TCP dumps on android."""

  _TCP_DUMP = '/data/local/tmp/tcpdump'
  _DEVICE_DUMP_FILE = '/sdcard/capture.pcap'

  def __init__(self, adb, output_path):
    self._adb = adb
    self._output_path = output_path
    self._proc = subprocess.Popen(
        ['adb', '-s', self._adb.device(),
         'shell', self._TCP_DUMP] + _TCP_DUMP_BASE_OPTS +
         [self._DEVICE_DUMP_FILE])

  def CollectProfile(self):
    tcpdump_pid = self._adb.ExtractPid('tcpdump')
    if not tcpdump_pid or not tcpdump_pid[0]:
      raise Exception('Unable to find TCPDump. Check your device is rooted '
          'and tcpdump is installed at ' + self._TCP_DUMP)
    self._adb.RunShellCommand('kill -term ' + tcpdump_pid[0])
    self._proc.terminate()
    host_dump = os.path.join(self._output_path,
                             os.path.basename(self._DEVICE_DUMP_FILE))
    self._adb.Adb().Adb().Pull(self._DEVICE_DUMP_FILE, host_dump)
    print 'TCP dump available at: %s ' % host_dump
    print 'Use Wireshark to open it.'


class _TCPDumpProfilerLinux(object):
  """An internal class to collect TCP dumps on linux desktop."""

  _DUMP_FILE = 'capture.pcap'

  def __init__(self, output_path):
    if not os.path.exists(output_path):
      os.makedirs(output_path)
    self._dump_file = os.path.join(output_path, self._DUMP_FILE)
    self._tmp_output_file = tempfile.NamedTemporaryFile('w', 0)
    try:
      self._proc = subprocess.Popen(
          ['tcpdump'] + _TCP_DUMP_BASE_OPTS + [self._dump_file],
          stdout=self._tmp_output_file, stderr=subprocess.STDOUT)
    except OSError as e:
      raise Exception('Unable to execute TCPDump, please check your '
          'installation. ' + str(e))

  def CollectProfile(self):
    self._proc.send_signal(signal.SIGINT)
    exit_code = self._proc.wait()
    try:
      if exit_code:
        raise Exception(
            'tcpdump failed with exit code %d. Output:\n%s' %
            (exit_code, self._GetStdOut()))
    finally:
      self._tmp_output_file.close()
    print 'TCP dump available at: ', self._dump_file
    print 'Use Wireshark to open it.'

  def _GetStdOut(self):
    self._tmp_output_file.flush()
    try:
      with open(self._tmp_output_file.name) as f:
        return f.read()
    except IOError:
      return ''


class TCPDumpProfiler(profiler.Profiler):
  """A Factory to instantiate the platform-specific profiler."""
  def __init__(self, browser_backend, platform_backend, output_path):
    super(TCPDumpProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    if platform_backend.GetOSName() == 'android':
      self._platform_profiler = _TCPDumpProfilerAndroid(
          browser_backend.adb, output_path)
    else:
      self._platform_profiler = _TCPDumpProfilerLinux(output_path)

  @classmethod
  def name(cls):
    return 'tcpdump'

  @classmethod
  def is_supported(cls, options):
    if options and options.browser_type.startswith('cros'):
      return False
    if sys.platform.startswith('linux'):
      return True
    if not options:
      return android_browser_finder.CanFindAvailableBrowsers()
    return options.browser_type.startswith('android')

  def CollectProfile(self):
    self._platform_profiler.CollectProfile()
