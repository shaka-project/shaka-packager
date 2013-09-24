# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

class InspectorMemoryException(Exception):
  pass

class InspectorMemory(object):
  """Communicates with the remote inspector's Memory domain."""

  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self._inspector_backend.RegisterDomain(
        'Memory',
        self._OnNotification,
        self._OnClose)

  def _OnNotification(self, msg):
    pass

  def _OnClose(self):
    pass

  def GetDOMCounters(self, timeout):
    """Retrieves DOM element counts.

    Args:
      timeout: The number of seconds to wait for the inspector backend to
          service the request before timing out.

    Returns:
      A dictionary containing the counts associated with "nodes", "documents",
      and "jsEventListeners".
    """
    res = self._inspector_backend.SyncRequest({
      'method': 'Memory.getDOMCounters'
    }, timeout)
    if ('result' not in res or
        'nodes' not in res['result'] or
        'documents' not in res['result'] or
        'jsEventListeners' not in res['result']):
      raise InspectorMemoryException(
          'Inspector returned unexpected result for Memory.getDOMCounters:\n' +
          json.dumps(res, indent=2))
    return {
        'nodes': res['result']['nodes'],
        'documents': res['result']['documents'],
        'jsEventListeners': res['result']['jsEventListeners']
    }
