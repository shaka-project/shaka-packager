# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test_results
from telemetry.page import page_measurement_value

class ValuesForSinglePage(object):
  def __init__(self, page):
    self.page = page
    self.values = []

  def AddValue(self, value):
    self.values.append(value)

  @property
  def measurement_names(self):
    return [value.measurement_name for value in self.values]

  def FindValueByMeasurementName(self, measurement_name):
    values = [value for value in self.values
              if value.measurement_name == measurement_name]
    assert len(values) <= 1
    if len(values):
      return values[0]
    return None

  def __getitem__(self, trace_name):
    return self.FindValueByTraceName(trace_name)

  def __contains__(self, trace_name):
    return self.FindValueByTraceName(trace_name) != None

  def FindValueByTraceName(self, trace_name):
    values = [value for value in self.values
              if value.trace_name == trace_name]
    assert len(values) <= 1
    if len(values):
      return values[0]
    return None

class PageMeasurementResults(page_test_results.PageTestResults):
  def __init__(self, trace_tag=''):
    super(PageMeasurementResults, self).__init__()
    self._trace_tag = trace_tag
    self._page_results = []
    self._overall_results = []

    self._all_measurements_that_have_been_seen = {}

    self._values_for_current_page = {}

  def __getitem__(self, i):
    """Shorthand for self.page_results[i]"""
    return self._page_results[i]

  def __len__(self):
    return len(self._page_results)

  @property
  def values_for_current_page(self):
    return self._values_for_current_page

  @property
  def page_results(self):
    return self._page_results

  def WillMeasurePage(self, page):
    self._values_for_current_page = ValuesForSinglePage(page)

  @property
  def all_measurements_that_have_been_seen(self):
    return self._all_measurements_that_have_been_seen

  def Add(self, trace_name, units, value, chart_name=None, data_type='default'):
    value = self._GetPageMeasurementValue(trace_name, units, value, chart_name,
                                        data_type)
    self._values_for_current_page.AddValue(value)

  def AddSummary(self, trace_name, units, value, chart_name=None,
                 data_type='default'):
    value = self._GetPageMeasurementValue(trace_name, units, value, chart_name,
                                        data_type)
    self._overall_results.append(value)

  def _GetPageMeasurementValue(self, trace_name, units, value, chart_name,
                             data_type):
    value = page_measurement_value.PageMeasurementValue(
        trace_name, units, value, chart_name, data_type)
    measurement_name = value.measurement_name

    # Sanity checks.
    assert measurement_name != 'url', 'The name url cannot be used'
    if measurement_name in self._all_measurements_that_have_been_seen:
      measurement_data = \
          self._all_measurements_that_have_been_seen[measurement_name]
      last_seen_units = measurement_data['units']
      last_seen_data_type = measurement_data['type']
      assert last_seen_units == units, \
          'Unit cannot change for a name once it has been provided'
      assert last_seen_data_type == data_type, \
          'Unit cannot change for a name once it has been provided'
    else:
      self._all_measurements_that_have_been_seen[measurement_name] = {
        'units': units,
        'type': data_type}
    return value

  def DidMeasurePage(self):
    assert self._values_for_current_page, 'Failed to call WillMeasurePage'
    self._page_results.append(self._values_for_current_page)
    self._values_for_current_page = None
