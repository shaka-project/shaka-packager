# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

TRACING_MODE = 'tracing-mode'
TIMELINE_MODE = 'timeline-mode'

class TimelineMetrics(object):
  def __init__(self, mode):
    assert mode in (TRACING_MODE, TIMELINE_MODE)
    self._mode = mode
    self._model = None
    self._thread_for_tab = None

  def Start(self, tab):
    self._model = None
    self._thread_for_tab = None

    if self._mode == TRACING_MODE:
      if not tab.browser.supports_tracing:
        raise Exception('Not supported')
      tab.browser.StartTracing()
    else:
      assert self._mode == TIMELINE_MODE
      tab.StartTimelineRecording()

  def Stop(self, tab):
    if self._mode == TRACING_MODE:
      # This creates an async trace event in the render process for tab that
      # will allow us to find that tab during the AddTracingResultsForTab
      # function.
      tab.ExecuteJavaScript("""
          console.time("__loading_measurement_was_here__");
          console.timeEnd("__loading_measurement_was_here__");
          """)

      tab.browser.StopTracing()
      trace_result = tab.browser.GetTraceResultAndReset()
      self._model = trace_result.AsTimelineModel()
      events = [s for
                s in self._model.GetAllEventsOfName(
                    '__loading_measurement_was_here__')
                if s.parent_slice == None]
      assert len(events) == 1
      self._thread_for_tab = events[0].start_thread
    else:
      tab.StopTimelineRecording()
      self._model = tab.timeline_model
      self._thread_for_tab = self._model.GetAllThreads()[0]

  def AddResults(self, results):
    assert self._model

    events = self._thread_for_tab.all_slices

    events_by_name = collections.defaultdict(list)
    for e in events:
      events_by_name[e.name].append(e)

    for key, group in events_by_name.items():
      times = [e.self_time for e in group]
      total = sum(times)
      biggest_jank = max(times)
      results.Add(key, 'ms', total)
      results.Add(key + '_max', 'ms', biggest_jank)
      results.Add(key + '_avg', 'ms', total / len(times))
