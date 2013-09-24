# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import telemetry.core.timeline.event as event

class AsyncSlice(event.TimelineEvent):
  ''' A AsyncSlice represents an interval of time during which an
  asynchronous operation is in progress. An AsyncSlice consumes no CPU time
  itself and so is only associated with Threads at its start and end point.
  '''
  def __init__(self, category, name, timestamp, args=None):
    super(AsyncSlice, self).__init__(
        category, name, timestamp, duration=0, args=args)
    self.parent_slice = None
    self.start_thread = None
    self.end_thread = None
    self.sub_slices = []
    self.id = None

  def AddSubSlice(self, sub_slice):
    assert sub_slice.parent_slice == self
    self.sub_slices.append(sub_slice)


  def IterEventsInThisContainerRecrusively(self):
    for sub_slice in self.sub_slices:
      yield sub_slice
