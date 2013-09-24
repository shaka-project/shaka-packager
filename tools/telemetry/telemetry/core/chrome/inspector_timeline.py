# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core.timeline import model

class TabBackendException(Exception):
  pass

class InspectorTimeline(object):
  """Implementation of dev tools timeline."""
  class Recorder(object):
    """Utility class to Start / Stop recording timeline."""
    def __init__(self, tab):
      self._tab = tab

    def __enter__(self):
      self._tab.StartTimelineRecording()

    def __exit__(self, *args):
      self._tab.StopTimelineRecording()

  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self._is_recording = False
    self._timeline_model = None
    self._raw_events = None

  @property
  def timeline_model(self):
    return self._timeline_model

  def Start(self):
    if self._is_recording:
      return
    self._is_recording = True
    self._timeline_model = None
    self._raw_events = []
    self._inspector_backend.RegisterDomain('Timeline',
       self._OnNotification, self._OnClose)
    req = {'method': 'Timeline.start'}
    self._SendSyncRequest(req)

  def Stop(self):
    if not self._is_recording:
      raise TabBackendException('Stop() called but not started')
    self._is_recording = False
    self._timeline_model = model.TimelineModel(event_data=self._raw_events,
                                               shift_world_to_zero=False)
    req = {'method': 'Timeline.stop'}
    self._SendSyncRequest(req)
    self._inspector_backend.UnregisterDomain('Timeline')

  def _SendSyncRequest(self, req, timeout=60):
    res = self._inspector_backend.SyncRequest(req, timeout)
    if 'error' in res:
      raise TabBackendException(res['error']['message'])
    return res['result']

  def _OnNotification(self, msg):
    if not self._is_recording:
      return
    if 'method' in msg and msg['method'] == 'Timeline.eventRecorded':
      self._OnEventRecorded(msg)

  def _OnEventRecorded(self, msg):
    record = msg.get('params', {}).get('record')
    if record:
      self._raw_events.append(record)

  def _OnClose(self):
    pass
