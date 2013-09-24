# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import telemetry.core.timeline.event_container as event_container
import telemetry.core.timeline.counter as tracing_counter
import telemetry.core.timeline.thread as tracing_thread

class Process(event_container.TimelineEventContainer):
  ''' The Process represents a single userland process in the trace.
  '''
  def __init__(self, parent, pid):
    super(Process, self).__init__('process %s' % pid, parent)
    self.pid = pid
    self._threads = {}
    self._counters = {}

  @property
  def threads(self):
    return self._threads

  @property
  def counters(self):
    return self._counters

  def IterChildContainers(self):
    for thread in self._threads.itervalues():
      yield thread
    for counter in self._counters.itervalues():
      yield counter

  def IterAllSlicesOfName(self, name):
    for thread in self._threads.itervalues():
      for s in thread.IterAllSlicesOfName(name):
        yield s

  def IterEventsInThisContainer(self):
    return
    yield # pylint: disable=W0101

  def GetOrCreateThread(self, tid):
    thread = self.threads.get(tid, None)
    if thread:
      return thread
    thread = tracing_thread.Thread(self, tid)
    self._threads[tid] = thread
    return thread

  def GetCounter(self, category, name):
    counter_id = category + '.' + name
    if counter_id in self.counters:
      return self.counters[counter_id]
    raise ValueError(
        'Counter %s not found in process with id %s.' % (counter_id,
                                                         self.pid))
  def GetOrCreateCounter(self, category, name):
    try:
      return self.GetCounter(category, name)
    except ValueError:
      ctr = tracing_counter.Counter(self, category, name)
      self._counters[ctr.full_name] = ctr
      return ctr

  def AutoCloseOpenSlices(self, max_timestamp):
    for thread in self._threads.itervalues():
      thread.AutoCloseOpenSlices(max_timestamp)

  def FinalizeImport(self):
    for thread in self._threads.itervalues():
      thread.FinalizeImport()
    for counter in self._counters.itervalues():
      counter.FinalizeImport()
