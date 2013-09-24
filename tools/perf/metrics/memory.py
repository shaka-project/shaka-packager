# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

from metrics import Metric

class MemoryMetric(Metric):
  """MemoryMetric gathers memory statistics from the browser object."""

  def __init__(self, browser):
    super(MemoryMetric, self).__init__()
    self._browser = browser
    self._memory_stats = None
    self._start_commit_charge = None

  def Start(self, page=None, tab=None):
    """Record the initial value of 'SystemCommitCharge'."""
    self._start_commit_charge = self._browser.memory_stats['SystemCommitCharge']

  def Stop(self, page=None, tab=None):
    """Fetch the browser memory stats."""
    assert self._start_commit_charge, 'Must call Start() first'
    self._memory_stats = self._browser.memory_stats

  def AddResults(self, tab, results):
    """Add summary results to the results object."""
    assert self._memory_stats, 'Must call Stop() first'
    if not self._memory_stats['Browser']:
      return

    metric = 'resident_set_size'
    if sys.platform == 'win32':
      metric = 'working_set'

    def AddSummariesForProcessTypes(process_types_memory, process_type_trace):
      """Add all summaries to the results for a given set of process types.

      Args:
        process_types_memory: A list of process types, e.g. Browser, 'Renderer'
        process_type_trace: The name of this set of process types in the output
      """
      def AddSummary(value_name_memory, value_name_trace):
        """Add a summary to the results for a given statistic.

        Args:
          value_name_memory: Name of some statistic, e.g. VM, WorkingSetSize
          value_name_trace: Name of this statistic to be used in the output
        """
        if len(process_types_memory) > 1 and value_name_memory.endswith('Peak'):
          return
        values = []
        for process_type_memory in process_types_memory:
          stats = self._memory_stats[process_type_memory]
          if value_name_memory in stats:
            values.append(stats[value_name_memory])
        if values:
          results.AddSummary(value_name_trace + process_type_trace,
                             'bytes', sum(values), data_type='unimportant')

      AddSummary('VM', 'vm_final_size_')
      AddSummary('WorkingSetSize', 'vm_%s_final_size_' % metric)
      AddSummary('PrivateDirty', 'vm_private_dirty_final_')
      AddSummary('ProportionalSetSize', 'vm_proportional_set_size_final_')
      AddSummary('VMPeak', 'vm_peak_size_')
      AddSummary('WorkingSetSizePeak', '%s_peak_size_' % metric)

    AddSummariesForProcessTypes(['Browser'], 'browser')
    AddSummariesForProcessTypes(['Renderer'], 'renderer')
    AddSummariesForProcessTypes(['Gpu'], 'gpu')
    AddSummariesForProcessTypes(['Browser', 'Renderer', 'Gpu'], 'total')

    end_commit_charge = self._memory_stats['SystemCommitCharge']
    commit_charge_difference = end_commit_charge - self._start_commit_charge
    results.AddSummary('commit_charge', 'kb', commit_charge_difference,
                       data_type='unimportant')
    results.AddSummary('processes', 'count', self._memory_stats['ProcessCount'],
                       data_type='unimportant')

