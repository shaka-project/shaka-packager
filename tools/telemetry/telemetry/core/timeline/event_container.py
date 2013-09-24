# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class TimelineEventContainer(object):
  """Represents a container for events."""
  def __init__(self, name, parent):
    self.parent = parent
    self.name = name

  def IterChildContainers(self):
    raise NotImplementedError()

  def IterEventsInThisContainer(self):
    raise NotImplementedError()
