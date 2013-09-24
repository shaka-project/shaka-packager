# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import threading

from telemetry.core import util
from telemetry.core.chrome import android_browser_finder
from telemetry.core.platform import profiler

class JavaHeapProfiler(profiler.Profiler):
  """Android-specific, trigger and fetch java heap dumps."""

  _DEFAULT_DEVICE_DIR = '/data/local/tmp/javaheap'
  # TODO(bulach): expose this as a command line option somehow.
  _DEFAULT_INTERVAL = 20
  def __init__(self, browser_backend, platform_backend, output_path):
    super(JavaHeapProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    self._run_count = 1

    self._DumpJavaHeap(False)

    self._timer = threading.Timer(self._DEFAULT_INTERVAL, self._OnTimer)
    self._timer.start()

  @classmethod
  def name(cls):
    return 'java-heap'

  @classmethod
  def is_supported(cls, options):
    if not options:
      return android_browser_finder.CanFindAvailableBrowsers()
    return options.browser_type.startswith('android')

  def CollectProfile(self):
    self._timer.cancel()
    self._DumpJavaHeap(True)
    self._browser_backend.adb.Adb().Adb().Pull(
        self._DEFAULT_DEVICE_DIR, self._output_path)
    self._browser_backend.adb.RunShellCommand(
        'rm ' + os.path.join(self._DEFAULT_DEVICE_DIR, '*'))
    output_files = []
    for f in os.listdir(self._output_path):
      if os.path.splitext(f)[1] == '.aprof':
        input_file = os.path.join(self._output_path, f)
        output_file = input_file.replace('.aprof', '.hprof')
        subprocess.call(['hprof-conv', input_file, output_file])
        output_files.append(output_file)
    return output_files

  def _OnTimer(self):
    self._DumpJavaHeap(False)

  def _DumpJavaHeap(self, wait_for_completion):
    device_dump_file = None
    for pid in self._GetProcessOutputFileMap().iterkeys():
      device_dump_file = '%s%s.%s.aprof' % (self._DEFAULT_DEVICE_DIR, pid,
                                            self._run_count)
      self._browser_backend.adb.RunShellCommand('am dumpheap %s %s' %
                                                (pid, device_dump_file))
    if device_dump_file and wait_for_completion:
      util.WaitFor(lambda: self._FileSize(device_dump_file) > 0,
                   timeout=2, poll_interval=0.5)
    self._run_count += 1

  def _FileSize(self, file_name):
    f = self._browser_backend.adb.Adb().ListPathContents(file_name)
    return f.get(os.path.basename(file_name), (0, ))[0]
