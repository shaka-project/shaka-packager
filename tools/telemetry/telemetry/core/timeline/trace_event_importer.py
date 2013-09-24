# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
''' TraceEventImporter imports TraceEvent-formatted data
into the provided model.
This is a port of the trace event importer from
https://code.google.com/p/trace-viewer/
'''

import copy
import json
import re

from telemetry.core.timeline import importer
import telemetry.core.timeline.async_slice as tracing_async_slice

class TraceEventTimelineImporter(importer.TimelineImporter):
  def __init__(self, model, event_data):
    super(TraceEventTimelineImporter, self).__init__(
        model, event_data, import_priority=1)

    self._events_were_from_string = False
    self._all_async_events = []
    self._all_object_events = []

    if type(event_data) is str:
      # If the event data begins with a [, then we know it should end with a ].
      # The reason we check for this is because some tracing implementations
      # cannot guarantee that a ']' gets written to the trace file. So, we are
      # forgiving and if this is obviously the case, we fix it up before
      # throwing the string at JSON.parse.
      if event_data[0] == '[':
        event_data = re.sub(r'[\r|\n]*$', '', event_data)
        event_data = re.sub(r'\s*,\s*$', '', event_data)
        if event_data[-1] != ']':
          event_data = event_data + ']'

      self._events = json.loads(event_data)
      self._events_were_from_string = True
    else:
      self._events = event_data

    # Some trace_event implementations put the actual trace events
    # inside a container. E.g { ... , traceEvents: [ ] }
    # If we see that, just pull out the trace events.
    if 'traceEvents' in self._events:
      container = self._events
      self._events = self._events['traceEvents']
      for field_name in container:
        if field_name == 'traceEvents':
          continue

        # Any other fields in the container should be treated as metadata.
        self._model.metadata.append({
            'name' : field_name,
            'value' : container['field_name']})

  @staticmethod
  def CanImport(event_data):
    ''' Returns whether obj is a TraceEvent array. '''
    # May be encoded JSON. But we dont want to parse it fully yet.
    # Use a simple heuristic:
    #   - event_data that starts with [ are probably trace_event
    #   - event_data that starts with { are probably trace_event
    # May be encoded JSON. Treat files that start with { as importable by us.
    if isinstance(event_data, str):
      return len(event_data) > 0 and (event_data[0] == '{'
          or event_data[0] == '[')

    # Might just be an array of events
    if (isinstance(event_data, list) and len(event_data)
        and 'ph' in event_data[0]):
      return True

    # Might be an object with a traceEvents field in it.
    if 'traceEvents' in event_data:
      trace_events = event_data.get('traceEvents', None)
      return (type(trace_events) is list and
          len(trace_events) > 0 and 'ph' in trace_events[0])

    return False

  def _GetOrCreateProcess(self, pid):
    return self._model.GetOrCreateProcess(pid)

  def _DeepCopyIfNeeded(self, obj):
    if self._events_were_from_string:
      return obj
    return copy.deepcopy(obj)

  def _ProcessAsyncEvent(self, event):
    '''Helper to process an 'async finish' event, which will close an
    open slice.
    '''
    thread = (self._GetOrCreateProcess(event['pid'])
        .GetOrCreateThread(event['tid']))
    self._all_async_events.append({
        'event': event,
        'thread': thread})

  def _ProcessCounterEvent(self, event):
    '''Helper that creates and adds samples to a Counter object based on
    'C' phase events.
    '''
    if 'id' in event:
      ctr_name = event['name'] + '[' + str(event['id']) + ']'
    else:
      ctr_name = event['name']

    ctr = (self._GetOrCreateProcess(event['pid'])
        .GetOrCreateCounter(event['cat'], ctr_name))
    # Initialize the counter's series fields if needed.
    if len(ctr.series_names) == 0:
      #TODO: implement counter object
      for series_name in event['args']:
        ctr.series_names.append(series_name)
      if len(ctr.series_names) == 0:
        self._model.import_errors.append('Expected counter ' + event['name'] +
            ' to have at least one argument to use as a value.')
        # Drop the counter.
        del ctr.parent.counters[ctr.full_name]
        return

    # Add the sample values.
    ctr.timestamps.append(event['ts'] / 1000.0)
    for series_name in ctr.series_names:
      if series_name not in event['args']:
        ctr.samples.append(0)
        continue
      ctr.samples.append(event['args'][series_name])

  def _ProcessObjectEvent(self, event):
    thread = (self._GetOrCreateProcess(event['pid'])
      .GetOrCreateThread(event['tid']))
    self._all_object_events.append({
        'event': event,
        'thread': thread})

  def _ProcessDurationEvent(self, event):
    thread = (self._GetOrCreateProcess(event['pid'])
      .GetOrCreateThread(event['tid']))
    if not thread.IsTimestampValidForBeginOrEnd(event['ts'] / 1000.0):
      self._model.import_errors.append(
          'Timestamps are moving backward.')
      return

    if event['ph'] == 'B':
      thread.BeginSlice(event['cat'],
                        event['name'],
                        event['ts'] / 1000.0,
                        event['args'])
    elif event['ph'] == 'E':
      thread = (self._GetOrCreateProcess(event['pid'])
        .GetOrCreateThread(event['tid']))
      if not thread.IsTimestampValidForBeginOrEnd(event['ts'] / 1000.0):
        self._model.import_errors.append(
            'Timestamps are moving backward.')
        return
      if not thread.open_slice_count:
        self._model.import_errors.append(
            'E phase event without a matching B phase event.')
        return

      new_slice = thread.EndSlice(event['ts'] / 1000.0)
      for arg_name, arg_value in event.get('args', {}).iteritems():
        if arg_name in new_slice.args:
          self._model.import_errors.append(
              'Both the B and E phases of ' + new_slice.name +
              ' provided values for argument ' + arg_name + '. ' +
              'The value of the E phase event will be used.')
        new_slice.args[arg_name] = arg_value

  def _ProcessMetadataEvent(self, event):
    if event['name'] == 'thread_name':
      thread = (self._GetOrCreateProcess(event['pid'])
          .GetOrCreateThread(event['tid']))
      thread.name = event['args']['name']
    if event['name'] == 'process_name':
      process = self._GetOrCreateProcess(event['pid'])
      process.name = event['args']['name']
    else:
      self._model.import_errors.append(
          'Unrecognized metadata name: ' + event['name'])

  def _ProcessInstantEvent(self, event):
    # Treat an Instant event as a duration 0 slice.
    # SliceTrack's redraw() knows how to handle this.
    thread = (self._GetOrCreateProcess(event['pid'])
      .GetOrCreateThread(event['tid']))
    thread.BeginSlice(event['cat'],
                      event['name'],
                      event['ts'] / 1000.0,
                      event.get('args'))
    thread.EndSlice(event['ts'] / 1000.0)

  def _ProcessSampleEvent(self, event):
    thread = (self._GetOrCreateProcess(event['pid'])
        .GetOrCreateThread(event['tid']))
    thread.AddSample(event['cat'],
                     event['name'],
                     event['ts'] / 1000.0,
                     args=event.get('args'))

  def ImportEvents(self):
    ''' Walks through the events_ list and outputs the structures discovered to
    model_.
    '''
    for event in self._events:
      phase = event.get('ph', None)
      if phase == 'B' or phase == 'E':
        self._ProcessDurationEvent(event)
      elif phase == 'S' or phase == 'F' or phase == 'T':
        self._ProcessAsyncEvent(event)
      # Note, I is historic. The instant event marker got changed, but we
      # want to support loading load trace files so we have both I and i.
      elif phase == 'I' or phase == 'i':
        self._ProcessInstantEvent(event)
      elif phase == 'P':
        self._ProcessSampleEvent(event)
      elif phase == 'C':
        self._ProcessCounterEvent(event)
      elif phase == 'M':
        self._ProcessMetadataEvent(event)
      elif phase == 'N' or phase == 'D' or phase == 'O':
        self._ProcessObjectEvent(event)
      elif phase == 's' or phase == 't' or phase == 'f':
        # NB: toss until there's proper support
        pass
      else:
        self._model.import_errors.append('Unrecognized event phase: ' +
            phase + '(' + event['name'] + ')')

    return self._model

  def FinalizeImport(self):
    '''Called by the Model after all other importers have imported their
    events.'''
    self._model.UpdateBounds()

    # We need to reupdate the bounds in case the minimum start time changes
    self._model.UpdateBounds()
    self._CreateAsyncSlices()
    self._CreateExplicitObjects()
    self._CreateImplicitObjects()

  def _CreateAsyncSlices(self):
    if len(self._all_async_events) == 0:
      return

    self._all_async_events.sort(
        cmp=lambda x, y: x['event']['ts'] - y['event']['ts'])

    async_event_states_by_name_then_id = {}

    all_async_events = self._all_async_events
    for async_event_state in all_async_events:
      event = async_event_state['event']
      name = event.get('name', None)
      if name is None:
        self._model.import_errors.append(
            'Async events (ph: S, T or F) require an name parameter.')
        continue

      event_id = event.get('id')
      if event_id is None:
        self._model.import_errors.append(
            'Async events (ph: S, T or F) require an id parameter.')
        continue

      # TODO(simonjam): Add a synchronous tick on the appropriate thread.

      if event['ph'] == 'S':
        if not name in async_event_states_by_name_then_id:
          async_event_states_by_name_then_id[name] = {}
        if event_id in async_event_states_by_name_then_id[name]:
          self._model.import_errors.append(
              'At ' + event['ts'] + ', a slice of the same id ' + event_id +
              ' was alrady open.')
          continue

        async_event_states_by_name_then_id[name][event_id] = []
        async_event_states_by_name_then_id[name][event_id].append(
            async_event_state)
      else:
        if name not in async_event_states_by_name_then_id:
          self._model.import_errors.append(
              'At ' + str(event['ts']) + ', no slice named ' + name +
              ' was open.')
          continue
        if event_id not in async_event_states_by_name_then_id[name]:
          self._model.import_errors.append(
              'At ' + str(event['ts']) + ', no slice named ' + name +
              ' with id=' + event_id + ' was open.')
          continue
        events = async_event_states_by_name_then_id[name][event_id]
        events.append(async_event_state)

        if event['ph'] == 'F':
          # Create a slice from start to end.
          async_slice = tracing_async_slice.AsyncSlice(
              events[0]['event']['cat'],
              name,
              events[0]['event']['ts'] / 1000.0)

          async_slice.duration = ((event['ts'] / 1000.0)
              - (events[0]['event']['ts'] / 1000.0))

          async_slice.start_thread = events[0]['thread']
          async_slice.end_thread = async_event_state['thread']
          async_slice.id = event_id
          async_slice.args = events[0]['event']['args']

          # Create sub_slices for each step.
          for j in xrange(1, len(events)):
            sub_name = name
            if events[j - 1]['event']['ph'] == 'T':
              sub_name = name + ':' + events[j - 1]['event']['args']['step']
            sub_slice = tracing_async_slice.AsyncSlice(
                events[0]['event']['cat'],
                sub_name,
                events[j - 1]['event']['ts'] / 1000.0)
            sub_slice.parent_slice = async_slice

            sub_slice.duration = ((events[j]['event']['ts'] / 1000.0)
                - (events[j - 1]['event']['ts'] / 1000.0))

            sub_slice.start_thread = events[j - 1]['thread']
            sub_slice.end_thread = events[j]['thread']
            sub_slice.id = event_id
            sub_slice.args = events[j - 1]['event']['args']

            async_slice.AddSubSlice(sub_slice)

          # The args for the finish event go in the last sub_slice.
          last_slice = async_slice.sub_slices[-1]
          for arg_name, arg_value in event['args'].iteritems():
            last_slice.args[arg_name] = arg_value

          # Add |async_slice| to the start-thread's async_slices.
          async_slice.start_thread.AddAsyncSlice(async_slice)
          del async_event_states_by_name_then_id[name][event_id]

  def _CreateExplicitObjects(self):
    # TODO(tengs): Implement object instance parsing
    pass

  def _CreateImplicitObjects(self):
    # TODO(tengs): Implement object instance parsing
    pass
