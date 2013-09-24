# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric

class IOMetric(Metric):
  """IO-related metrics, obtained via telemetry.core.Browser."""

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def AddResults(self, tab, results):
    # This metric currently only returns summary results, not per-page results.
    raise NotImplementedError()

  def AddSummaryResults(self, tab, results):
    """Add summary results to the results object."""
    io_stats = tab.browser.io_stats
    if not io_stats['Browser']:
      return

    def AddSummariesForProcessType(process_type_io, process_type_trace):
      """For a given process type, add all relevant summary results.

      Args:
        process_type_io: Type of process (eg Browser or Renderer).
        process_type_trace: String to be added to the trace name in the results.
      """
      if 'ReadOperationCount' in io_stats[process_type_io]:
        results.AddSummary('read_operations_' + process_type_trace, 'count',
                           io_stats[process_type_io]
                           ['ReadOperationCount'],
                           data_type='unimportant')
      if 'WriteOperationCount' in io_stats[process_type_io]:
        results.AddSummary('write_operations_' + process_type_trace, 'count',
                           io_stats[process_type_io]
                           ['WriteOperationCount'],
                           data_type='unimportant')
      if 'ReadTransferCount' in io_stats[process_type_io]:
        results.AddSummary('read_bytes_' + process_type_trace, 'kb',
                           io_stats[process_type_io]
                           ['ReadTransferCount'] / 1024,
                           data_type='unimportant')
      if 'WriteTransferCount' in io_stats[process_type_io]:
        results.AddSummary('write_bytes_' + process_type_trace, 'kb',
                           io_stats[process_type_io]
                           ['WriteTransferCount'] / 1024,
                           data_type='unimportant')

    AddSummariesForProcessType('Browser', 'browser')
    AddSummariesForProcessType('Renderer', 'renderer')
    AddSummariesForProcessType('Gpu', 'gpu')

