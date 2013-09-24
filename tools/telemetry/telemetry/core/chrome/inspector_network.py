# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class InspectorNetwork(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend

  def ClearCache(self, timeout=60):
    """Clears the browser's disk and memory cache."""
    res = self._inspector_backend.SyncRequest({
        'method': 'Network.canClearBrowserCache'
        }, timeout)
    assert res['result'], 'Cache clearing is not supported by this browser'
    self._inspector_backend.SyncRequest({
        'method': 'Network.clearBrowserCache'
        }, timeout)
