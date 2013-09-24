#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command line tool for continuously printing Android graphics surface
statistics on the console.
"""

import collections
import optparse
import sys
import time

from pylib import android_commands, surface_stats_collector
from pylib.utils import run_tests_helper


_FIELD_FORMAT = {
  'jank_count (janks)': '%d',
  'max_frame_delay (vsyncs)': '%d',
  'avg_surface_fps (fps)': '%.2f',
  'frame_lengths (vsyncs)': '%.3f',
  'refresh_period (seconds)': '%.6f',
}


def _MergeResults(results, fields):
  merged_results = collections.defaultdict(list)
  for result in results:
    if ((fields != ['all'] and not result.name in fields) or
        result.value is None):
      continue
    name = '%s (%s)' % (result.name, result.unit)
    if isinstance(result.value, list):
      value = result.value
    else:
      value = [result.value]
    merged_results[name] += value
  for name, values in merged_results.iteritems():
    merged_results[name] = sum(values) / float(len(values))
  return merged_results


def _GetTerminalHeight():
  try:
    import fcntl, termios, struct
  except ImportError:
    return 0, 0
  height, _, _, _ = struct.unpack('HHHH',
      fcntl.ioctl(0, termios.TIOCGWINSZ,
          struct.pack('HHHH', 0, 0, 0, 0)))
  return height


def _PrintColumnTitles(results):
  for name in results.keys():
    print '%s ' % name,
  print
  for name in results.keys():
    print '%s ' % ('-' * len(name)),
  print


def _PrintResults(results):
  for name, value in results.iteritems():
    value = _FIELD_FORMAT.get(name, '%s') % value
    print value.rjust(len(name)) + ' ',
  print


def main(argv):
  parser = optparse.OptionParser(usage='Usage: %prog [options]',
                                 description=__doc__)
  parser.add_option('-v',
                    '--verbose',
                    dest='verbose_count',
                    default=0,
                    action='count',
                    help='Verbose level (multiple times for more)')
  parser.add_option('--device',
                    help='Serial number of device we should use.')
  parser.add_option('-f',
                    '--fields',
                    dest='fields',
                    default='jank_count,max_frame_delay,avg_surface_fps,'
                        'frame_lengths',
                    help='Comma separated list of fields to display or "all".')
  parser.add_option('-d',
                    '--delay',
                    dest='delay',
                    default=1,
                    type='float',
                    help='Time in seconds to sleep between updates.')

  options, args = parser.parse_args(argv)
  run_tests_helper.SetLogLevel(options.verbose_count)

  adb = android_commands.AndroidCommands(options.device)
  collector = surface_stats_collector.SurfaceStatsCollector(adb)
  collector.DisableWarningAboutEmptyData()

  fields = options.fields.split(',')
  row_count = None

  try:
    collector.Start()
    while True:
      time.sleep(options.delay)
      results = collector.SampleResults()
      results = _MergeResults(results, fields)

      if not results:
        continue

      terminal_height = _GetTerminalHeight()
      if row_count is None or (terminal_height and
          row_count >= terminal_height - 3):
        _PrintColumnTitles(results)
        row_count = 0

      _PrintResults(results)
      row_count += 1
  except KeyboardInterrupt:
    sys.exit(0)
  finally:
    collector.Stop()


if __name__ == '__main__':
  main(sys.argv)
