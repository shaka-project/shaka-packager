# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import signal
import subprocess
import sys
import tempfile

from telemetry.core.platform import profiler


class _SingleProcessPerfProfiler(object):
  """An internal class for using perf for a given process."""
  def __init__(self, pid, output_file, platform_backend):
    self._pid = pid
    self._platform_backend = platform_backend
    self._output_file = output_file
    self._tmp_output_file = tempfile.NamedTemporaryFile('w', 0)
    self._proc = subprocess.Popen(
        ['perf', 'record', '--call-graph',
         '--pid', str(pid), '--output', output_file],
        stdout=self._tmp_output_file, stderr=subprocess.STDOUT)

  def CollectProfile(self):
    if ('renderer' in self._output_file and
        not self._platform_backend.GetCommandLine(self._pid)):
      logging.warning('Renderer was swapped out during profiling. '
                      'To collect a full profile rerun with '
                      '"--extra-browser-args=--single-process"')
    self._proc.send_signal(signal.SIGINT)
    exit_code = self._proc.wait()
    try:
      if exit_code == 128:
        raise Exception(
            """perf failed with exit code 128.
Try rerunning this script under sudo or setting
/proc/sys/kernel/perf_event_paranoid to "-1".\nOutput:\n%s""" %
            self._GetStdOut())
      elif exit_code not in (0, -2):
        raise Exception(
            'perf failed with exit code %d. Output:\n%s' % (exit_code,
                                                            self._GetStdOut()))
    finally:
      self._tmp_output_file.close()
    print 'To view the profile, run:'
    print '  perf report -n -i %s' % self._output_file
    return self._output_file

  def _GetStdOut(self):
    self._tmp_output_file.flush()
    try:
      with open(self._tmp_output_file.name) as f:
        return f.read()
    except IOError:
      return ''


class PerfProfiler(profiler.Profiler):

  def __init__(self, browser_backend, platform_backend, output_path):
    super(PerfProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    process_output_file_map = self._GetProcessOutputFileMap()
    self._process_profilers = []
    for pid, output_file in process_output_file_map.iteritems():
      if 'zygote' in output_file:
        continue
      self._process_profilers.append(
          _SingleProcessPerfProfiler(pid, output_file, platform_backend))

  @classmethod
  def name(cls):
    return 'perf'

  @classmethod
  def is_supported(cls, options):
    if sys.platform != 'linux2':
      return False
    if options and (options.browser_type.startswith('android')
                    or options.browser_type.startswith('cros')):
      return False
    try:
      return not subprocess.Popen(['perf', '--version'],
                                  stderr=subprocess.STDOUT,
                                  stdout=subprocess.PIPE).wait()
    except OSError:
      return False

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArg('--no-sandbox')
    options.AppendExtraBrowserArg('--allow-sandbox-debugging')

  def CollectProfile(self):
    output_files = []
    for single_process in self._process_profilers:
      output_files.append(single_process.CollectProfile())
    return output_files

  @classmethod
  def GetTopSamples(cls, file_name, number):
    """Parses the perf generated profile in |file_name| and returns a
    {function: period} dict of the |number| hottests functions.
    """
    assert os.path.exists(file_name)
    report = subprocess.Popen(
        ['perf', 'report', '--show-total-period', '-U', '-t', '^', '-i',
         file_name],
        stdout=subprocess.PIPE, stderr=open(os.devnull, 'w')).communicate()[0]
    period_by_function = {}
    for line in report.split('\n'):
      if not line or line.startswith('#'):
        continue
      fields = line.split('^')
      if len(fields) != 5:
        continue
      period = int(fields[1])
      function = fields[4].partition(' ')[2]
      function = re.sub('<.*>', '', function)  # Strip template params.
      function = re.sub('[(].*[)]', '', function)  # Strip function params.
      period_by_function[function] = period
      if len(period_by_function) == number:
        break
    return period_by_function
