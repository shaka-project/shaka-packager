# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import perf_tests_helper

def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0

class PageMeasurementValue(object):
  def __init__(self, trace_name, units, value, chart_name, data_type):
    self.trace_name = trace_name
    self.units = units
    self.value = value
    self.chart_name = chart_name
    self.data_type = data_type

  @property
  def measurement_name(self):
    if self.chart_name:
      return '%s.%s' % (self.chart_name, self.trace_name)
    else:
      return self.trace_name

  @property
  def output_value(self):
    if 'histogram' in self.data_type:
      (mean, _) = perf_tests_helper.GeomMeanAndStdDevFromHistogram(self.value)
      return mean
    elif isinstance(self.value, list):
      return _Mean(self.value)
    else:
      return self.value
