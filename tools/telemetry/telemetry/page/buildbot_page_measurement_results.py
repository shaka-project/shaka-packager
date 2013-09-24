# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
from itertools import chain

from telemetry.page import page_measurement_results
from telemetry.page import perf_tests_helper

class BuildbotPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self, trace_tag=''):
    super(BuildbotPageMeasurementResults, self).__init__()
    self._trace_tag = trace_tag

  def _PrintPerfResult(self, measurement, trace, values, units,
                       result_type='default'):
    perf_tests_helper.PrintPerfResult(
        measurement, trace, values, units, result_type)

  def PrintSummary(self):
    """Print summary data in a format expected by buildbot for perf dashboards.

    If any failed pages exist, only output individual page results for
    non-failing pages, and do not output any average data.
    """
    if self.errors or self.failures:
      success_page_results = [r for r in self._page_results
                              if r.page.url not in
                              zip(*self.errors + self.failures)[0]]
    else:
      success_page_results = self._page_results

    # Print out the list of unique pages.
    # Use a set and a list to efficiently create an order preserving list of
    # unique URLs.
    unique_page_urls = []
    unique_page_urls_set = set()
    for page_values in success_page_results:
      url = page_values.page.display_url
      if url in unique_page_urls_set:
        continue
      unique_page_urls.append(url)
      unique_page_urls_set.add(url)
    perf_tests_helper.PrintPages(unique_page_urls)

    # Build the results summary.
    results_summary = defaultdict(list)
    for measurement_name in \
          self._all_measurements_that_have_been_seen.iterkeys():
      for page_values in success_page_results:
        value = page_values.FindValueByMeasurementName(measurement_name)
        if not value:
          continue
        measurement_units_type = (measurement_name,
                                  value.units,
                                  value.data_type)
        value_url = (value.value, page_values.page.display_url)
        results_summary[measurement_units_type].append(value_url)

    # Output the results summary sorted by name, then units, then data type.
    for measurement_units_type, value_url_list in sorted(
        results_summary.iteritems()):
      measurement, units, data_type = measurement_units_type

      if 'histogram' in data_type:
        by_url_data_type = 'unimportant-histogram'
      else:
        by_url_data_type = 'unimportant'
      if '.' in measurement and 'histogram' not in data_type:
        measurement, trace = measurement.split('.', 1)
        trace += self._trace_tag
      else:
        trace = measurement + self._trace_tag

      # Print individual _by_url results if there's more than 1 successful page,
      # or if there's exactly 1 successful page but a failure exists.
      if not self._trace_tag and (len(value_url_list) > 1 or
          ((self.errors or self.failures) and len(value_url_list) == 1)):
        url_value_map = defaultdict(list)
        for value, url in value_url_list:
          if 'histogram' in data_type and url_value_map[url]:
            # TODO(tonyg/marja): The histogram processing code only accepts one
            # histogram, so we only report the first histogram. Once histograms
            # support aggregating multiple values, this can be removed.
            continue
          url_value_map[url].append(value)
        for url in unique_page_urls:
          if not len(url_value_map[url]):
            continue
          self._PrintPerfResult(measurement + '_by_url', url,
                                url_value_map[url], units, by_url_data_type)

      # If there were no page failures, print the average data.
      # For histograms, we don't print the average data, only the _by_url,
      # unless there is only 1 page in which case the _by_urls are omitted.
      if not (self.errors or self.failures):
        if 'histogram' not in data_type or len(value_url_list) == 1:
          values = [i[0] for i in value_url_list]
          if isinstance(values[0], list):
            values = list(chain.from_iterable(values))
          self._PrintPerfResult(measurement, trace, values, units, data_type)

    # If there were no failed pages, output the overall results (results not
    # associated with a page).
    if not (self.errors or self.failures):
      for value in self._overall_results:
        values = value.value
        if not isinstance(values, list):
          values = [values]
        measurement_name = value.chart_name
        if not measurement_name:
          measurement_name = value.trace_name
        self._PrintPerfResult(measurement_name,
                              value.trace_name + self._trace_tag,
                              values, value.units, value.data_type)

    super(BuildbotPageMeasurementResults, self).PrintSummary()
