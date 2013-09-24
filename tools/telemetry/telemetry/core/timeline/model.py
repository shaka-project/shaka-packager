# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''A container for timeline-based events and traces and can handle importing
raw event data from different sources. This model closely resembles that in the
trace_viewer project:
https://code.google.com/p/trace-viewer/
'''

import telemetry.core.timeline.process as tracing_process

# Register importers for data
from telemetry.core.timeline import inspector_importer
from telemetry.core.timeline import bounds
from telemetry.core.timeline import trace_event_importer

_IMPORTERS = [
    inspector_importer.InspectorTimelineImporter,
    trace_event_importer.TraceEventTimelineImporter
]

class TimelineModel(object):
  def __init__(self, event_data=None, shift_world_to_zero=True):
    self._bounds = bounds.Bounds()
    self._processes = {}
    self._frozen = False
    self.import_errors = []
    self.metadata = []

    if event_data is not None:
      self.ImportTraces([event_data], shift_world_to_zero=shift_world_to_zero)

  @property
  def bounds(self):
    return self._bounds

  @property
  def processes(self):
    return self._processes

  def ImportTraces(self, traces, shift_world_to_zero=True):
    if self._frozen:
      raise Exception("Cannot add events once recording is done")

    importers = []
    for event_data in traces:
      importers.append(self._CreateImporter(event_data))

    importers.sort(cmp=lambda x, y: x.import_priority - y.import_priority)

    for importer in importers:
      # TODO: catch exceptions here and add it to error list
      importer.ImportEvents()

    self.UpdateBounds()
    if not self.bounds.is_empty:
      for process in self._processes.itervalues():
        process.AutoCloseOpenSlices(self.bounds.max)

    for importer in importers:
      importer.FinalizeImport()

    for process in self.processes.itervalues():
      process.FinalizeImport()

    if shift_world_to_zero:
      self.ShiftWorldToZero()
    self.UpdateBounds()

    # Because of FinalizeImport, it would probably be a good idea
    # to prevent the timeline from from being modified.
    self._frozen = True

  def ShiftWorldToZero(self):
    self.UpdateBounds()
    if self._bounds.is_empty:
      return
    shift_amount = -self._bounds.min
    for event in self.IterAllEvents():
      event.start += shift_amount

  def UpdateBounds(self):
    self._bounds.Reset()
    for event in self.IterAllEvents():
      self._bounds.AddValue(event.start)
      self._bounds.AddValue(event.end)

  def GetAllContainers(self):
    containers = []
    def Iter(container):
      containers.append(container)
      for container in container.IterChildContainers():
        Iter(container)
    for process in self._processes.itervalues():
      Iter(process)
    return containers

  def IterAllEvents(self):
    for container in self.GetAllContainers():
      for event in container.IterEventsInThisContainer():
        yield event

  def GetAllProcesses(self):
    return self._processes.values()

  def GetAllThreads(self):
    threads = []
    for process in self._processes.values():
      threads.extend(process.threads.values())
    return threads

  def GetAllEvents(self):
    return list(self.IterAllEvents())

  def GetAllEventsOfName(self, name):
    return [e for e in self.IterAllEvents() if e.name == name]

  def GetOrCreateProcess(self, pid):
    if pid not in self._processes:
      assert not self._frozen
      self._processes[pid] = tracing_process.Process(self, pid)
    return self._processes[pid]

  def _CreateImporter(self, event_data):
    for importer_class in _IMPORTERS:
      if importer_class.CanImport(event_data):
        return importer_class(self, event_data)
    raise ValueError("Could not find an importer for the provided event data")
