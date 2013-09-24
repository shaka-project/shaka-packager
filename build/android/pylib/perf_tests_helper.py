# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

import android_commands
import json
import logging
import math

# Valid values of result type.
RESULT_TYPES = {'unimportant': 'RESULT ',
                'default': '*RESULT ',
                'informational': '',
                'unimportant-histogram': 'HISTOGRAM ',
                'histogram': '*HISTOGRAM '}


def _EscapePerfResult(s):
  """Escapes |s| for use in a perf result."""
  return re.sub('[\:|=/#&,]', '_', s)


def _Flatten(values):
  """Returns a simple list without sub-lists."""
  ret = []
  for entry in values:
    if isinstance(entry, list):
      ret.extend(_Flatten(entry))
    else:
      ret.append(entry)
  return ret


def GeomMeanAndStdDevFromHistogram(histogram_json):
  histogram = json.loads(histogram_json)
  # Handle empty histograms gracefully.
  if not 'buckets' in histogram:
    return 0.0, 0.0
  count = 0
  sum_of_logs = 0
  for bucket in histogram['buckets']:
    if 'high' in bucket:
      bucket['mean'] = (bucket['low'] + bucket['high']) / 2.0
    else:
      bucket['mean'] = bucket['low']
    if bucket['mean'] > 0:
      sum_of_logs += math.log(bucket['mean']) * bucket['count']
      count += bucket['count']

  if count == 0:
    return 0.0, 0.0

  sum_of_squares = 0
  geom_mean = math.exp(sum_of_logs / count)
  for bucket in histogram['buckets']:
    if bucket['mean'] > 0:
      sum_of_squares += (bucket['mean'] - geom_mean) ** 2 * bucket['count']
  return geom_mean, math.sqrt(sum_of_squares / count)


def _MeanAndStdDevFromList(values):
  avg = None
  sd = None
  if len(values) > 1:
    try:
      value = '[%s]' % ','.join([str(v) for v in values])
      avg = sum([float(v) for v in values]) / len(values)
      sqdiffs = [(float(v) - avg) ** 2 for v in values]
      variance = sum(sqdiffs) / (len(values) - 1)
      sd = math.sqrt(variance)
    except ValueError:
      value = ", ".join(values)
  else:
    value = values[0]
  return value, avg, sd


def PrintPages(page_list):
  """Prints list of pages to stdout in the format required by perf tests."""
  print 'Pages: [%s]' % ','.join([_EscapePerfResult(p) for p in page_list])


def PrintPerfResult(measurement, trace, values, units, result_type='default',
                    print_to_stdout=True):
  """Prints numerical data to stdout in the format required by perf tests.

  The string args may be empty but they must not contain any colons (:) or
  equals signs (=).

  Args:
    measurement: A description of the quantity being measured, e.g. "vm_peak".
    trace: A description of the particular data point, e.g. "reference".
    values: A list of numeric measured values. An N-dimensional list will be
        flattened and treated as a simple list.
    units: A description of the units of measure, e.g. "bytes".
    result_type: Accepts values of RESULT_TYPES.
    print_to_stdout: If True, prints the output in stdout instead of returning
        the output to caller.

    Returns:
      String of the formated perf result.
  """
  assert result_type in RESULT_TYPES, 'result type: %s is invalid' % result_type

  trace_name = _EscapePerfResult(trace)

  if result_type in ['unimportant', 'default', 'informational']:
    assert isinstance(values, list)
    assert len(values)
    assert '/' not in measurement
    value, avg, sd = _MeanAndStdDevFromList(_Flatten(values))
    output = '%s%s: %s%s%s %s' % (
        RESULT_TYPES[result_type],
        _EscapePerfResult(measurement),
        trace_name,
        # Do not show equal sign if the trace is empty. Usually it happens when
        # measurement is enough clear to describe the result.
        '= ' if trace_name else '',
        value,
        units)
  else:
    assert(result_type in ['histogram', 'unimportant-histogram'])
    assert isinstance(values, list)
    # The histograms can only be printed individually, there's no computation
    # across different histograms.
    assert len(values) == 1
    value = values[0]
    output = '%s%s: %s= %s' % (
        RESULT_TYPES[result_type],
        _EscapePerfResult(measurement),
        trace_name,
        value)
    avg, sd = GeomMeanAndStdDevFromHistogram(value)

  if avg:
    output += '\nAvg %s: %f%s' % (measurement, avg, units)
  if sd:
    output += '\nSd  %s: %f%s' % (measurement, sd, units)
  if print_to_stdout:
    print output
    sys.stdout.flush()
  return output


class CacheControl(object):
  _DROP_CACHES = '/proc/sys/vm/drop_caches'

  def __init__(self, adb):
    self._adb = adb

  def DropRamCaches(self):
    """Drops the filesystem ram caches for performance testing."""
    self._adb.RunShellCommand('su -c sync')
    self._adb.SetProtectedFileContents(CacheControl._DROP_CACHES, '3')


class PerfControl(object):
  """Provides methods for setting the performance mode of a device."""
  _SCALING_GOVERNOR_FMT = (
      '/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor')

  def __init__(self, adb):
    self._adb = adb
    kernel_max = self._adb.GetFileContents('/sys/devices/system/cpu/kernel_max',
                                           log_result=False)
    assert kernel_max, 'Unable to find /sys/devices/system/cpu/kernel_max'
    self._kernel_max = int(kernel_max[0])
    logging.info('Maximum CPU index: %d' % self._kernel_max)
    self._original_scaling_governor = self._adb.GetFileContents(
      PerfControl._SCALING_GOVERNOR_FMT % 0,
      log_result=False)[0]

  def SetHighPerfMode(self):
    """Sets the highest possible performance mode for the device."""
    self._SetScalingGovernorInternal('performance')

  def SetDefaultPerfMode(self):
    """Sets the performance mode for the device to its default mode."""
    product_model = self._adb.GetProductModel()
    governor_mode = {
                    "GT-I9300" : 'pegasusq',
                    "Galaxy Nexus" : 'interactive',
                    "Nexus 4" : 'ondemand',
                    "Nexus 7" : 'interactive',
                    "Nexus 10": 'interactive'
                    }.get(product_model, 'ondemand')
    self._SetScalingGovernorInternal(governor_mode)

  def RestoreOriginalPerfMode(self):
    """Resets the original performance mode of the device."""
    self._SetScalingGovernorInternal(self._original_scaling_governor)

  def _SetScalingGovernorInternal(self, value):
    for cpu in range(self._kernel_max + 1):
      scaling_governor_file = PerfControl._SCALING_GOVERNOR_FMT % cpu
      if self._adb.FileExistsOnDevice(scaling_governor_file):
        logging.info('Writing scaling governor mode \'%s\' -> %s' %
                     (value, scaling_governor_file))
        self._adb.SetProtectedFileContents(scaling_governor_file, value)
