# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Imports event data obtained from the inspector's timeline.'''

from telemetry.core.timeline import importer
import telemetry.core.timeline.thread as timeline_thread

class InspectorTimelineImporter(importer.TimelineImporter):
  def __init__(self, model, event_data):
    super(InspectorTimelineImporter, self).__init__(model, event_data)

  @staticmethod
  def CanImport(event_data):
    ''' Checks if event_data is from the inspector timeline. We assume
    that if the first event is a valid inspector event, we can import the
    entire list.
    '''
    if isinstance(event_data, list) and len(event_data):
      event_datum = event_data[0]
      return 'startTime' in event_datum and 'endTime' in event_datum
    return False

  def ImportEvents(self):
    render_process = self._model.GetOrCreateProcess(0)
    render_thread = render_process.GetOrCreateThread(0)
    for raw_event in self._event_data:
      InspectorTimelineImporter.AddRawEventToThreadRecursive(
          render_thread, raw_event)

  def FinalizeImport(self):
    pass

  @staticmethod
  def AddRawEventToThreadRecursive(thread, raw_inspector_event):
    did_begin_slice = False
    if ('startTime' in raw_inspector_event and
        'endTime' in raw_inspector_event):
      args = {}
      for x in raw_inspector_event:
        if x in ('startTime', 'endTime', 'children'):
          continue
        args[x] = raw_inspector_event[x]
      if len(args) == 0:
        args = None
      thread.BeginSlice('inspector',
                        raw_inspector_event['type'],
                        raw_inspector_event['startTime'],
                        args)
      did_begin_slice = True

    for child in raw_inspector_event.get('children', []):
      InspectorTimelineImporter.AddRawEventToThreadRecursive(
          thread, child)

    if did_begin_slice:
      thread.EndSlice(raw_inspector_event['endTime'])

  @staticmethod
  def RawEventToTimelineEvent(raw_inspector_event):
    """Converts raw_inspector_event to TimelineEvent."""
    thread = timeline_thread.Thread(None, 0)
    InspectorTimelineImporter.AddRawEventToThreadRecursive(
        thread, raw_inspector_event)
    thread.FinalizeImport()
    assert len(thread.toplevel_slices) <= 1
    if len(thread.toplevel_slices) == 0:
      return None
    return thread.toplevel_slices[0]
