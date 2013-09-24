# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class TraceResult(object):
  def __init__(self, impl):
    self._impl = impl

  def Serialize(self, f):
    """Serializes the trace result to a file-like object"""
    return self._impl.Serialize(f)

  def AsTimelineModel(self):
    """Parses the trace result into a timeline model for in-memory
    manipulation."""
    return self._impl.AsTimelineModel()
