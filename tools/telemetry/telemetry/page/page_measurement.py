# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

from telemetry.page import block_page_measurement_results
from telemetry.page import buildbot_page_measurement_results
from telemetry.page import csv_page_measurement_results
from telemetry.page import html_page_measurement_results
from telemetry.page import page_measurement_results
from telemetry.page import page_test

class MeasurementFailure(page_test.Failure):
  """Exception that can be thrown from MeasurePage to indicate an undesired but
  designed-for problem."""
  pass

class PageMeasurement(page_test.PageTest):
  """Glue code for running a measurement across a set of pages.

  To use this, subclass from the measurement and override MeasurePage. For
  example:

     class BodyChildElementMeasurement(PageMeasurement):
        def MeasurePage(self, page, tab, results):
           body_child_count = tab.EvaluateJavaScript(
               'document.body.children.length')
           results.Add('body_children', 'count', body_child_count)

     if __name__ == '__main__':
         page_measurement.Main(BodyChildElementMeasurement())

  To add test-specific options:

     class BodyChildElementMeasurement(PageMeasurement):
        def AddCommandLineOptions(parser):
           parser.add_option('--element', action='store', default='body')

        def MeasurePage(self, page, tab, results):
           body_child_count = tab.EvaluateJavaScript(
              'document.querySelector('%s').children.length')
           results.Add('children', 'count', child_count)
  """
  def __init__(self,
               action_name_to_run='',
               needs_browser_restart_after_each_run=False,
               discard_first_result=False,
               clear_cache_before_each_run=False):
    super(PageMeasurement, self).__init__(
      '_RunTest',
      action_name_to_run,
      needs_browser_restart_after_each_run,
      discard_first_result,
      clear_cache_before_each_run)

  def _RunTest(self, page, tab, results):
    results.WillMeasurePage(page)
    self.MeasurePage(page, tab, results)
    results.DidMeasurePage()

  def AddOutputOptions(self, parser):
    super(PageMeasurement, self).AddOutputOptions(parser)
    parser.add_option('-o', '--output',
                      dest='output_file',
                      help='Redirects output to a file. Defaults to stdout.')
    parser.add_option('--output-trace-tag',
                      default='',
                      help='Append a tag to the key of each result trace.')
    parser.add_option('--reset-html-results', action='store_true',
                      help='Delete all stored runs in HTML output')

  @property
  def output_format_choices(self):
    return ['html', 'buildbot', 'block', 'csv', 'none']

  def PrepareResults(self, options):
    if hasattr(options, 'output_file') and options.output_file:
      output_file = os.path.expanduser(options.output_file)
      open(output_file, 'a').close()  # Create file if it doesn't exist.
      output_stream = open(output_file, 'r+')
    else:
      output_stream = sys.stdout
    if not hasattr(options, 'output_format'):
      options.output_format = self.output_format_choices[0]
    if not hasattr(options, 'output_trace_tag'):
      options.output_trace_tag = ''

    if options.output_format == 'csv':
      return csv_page_measurement_results.CsvPageMeasurementResults(
        output_stream,
        self.results_are_the_same_on_every_page)
    elif options.output_format == 'block':
      return block_page_measurement_results.BlockPageMeasurementResults(
        output_stream)
    elif options.output_format == 'buildbot':
      return buildbot_page_measurement_results.BuildbotPageMeasurementResults(
          trace_tag=options.output_trace_tag)
    elif options.output_format == 'html':
      return html_page_measurement_results.HtmlPageMeasurementResults(
          output_stream, self.__class__.__name__, options.reset_html_results,
          options.browser_type, trace_tag=options.output_trace_tag)
    elif options.output_format == 'none':
      return page_measurement_results.PageMeasurementResults(
          trace_tag=options.output_trace_tag)
    else:
      # Should never be reached. The parser enforces the choices.
      raise Exception('Invalid --output-format "%s". Valid choices are: %s'
                      % (options.output_format,
                         ', '.join(self.output_format_choices)))

  @property
  def results_are_the_same_on_every_page(self):
    """By default, measurements are assumed to output the same values for every
    page. This allows incremental output, for example in CSV. If, however, the
    measurement discovers what values it can report as it goes, and those values
    may vary from page to page, you need to override this function and return
    False. Output will not appear in this mode until the entire pageset has
    run."""
    return True

  def MeasurePage(self, page, tab, results):
    """Override to actually measure the page's performance.

    page is a page_set.Page
    tab is an instance of telemetry.core.Tab

    Should call results.Add(name, units, value) for each result, or raise an
    exception on failure. The name and units of each Add() call must be
    the same across all iterations. The name 'url' must not be used.

    Prefer field names that are in accordance with python variable style. E.g.
    field_name.

    Put together:

       def MeasurePage(self, page, tab, results):
         res = tab.EvaluateJavaScript('2+2')
         if res != 4:
           raise Exception('Oh, wow.')
         results.Add('two_plus_two', 'count', res)
    """
    raise NotImplementedError()
