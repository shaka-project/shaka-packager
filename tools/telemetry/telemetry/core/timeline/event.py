# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class TimelineEvent(object):
  """Represents a timeline event."""
  def __init__(self, category, name, start, duration, args=None):
    self.category = category
    self.name = name
    self.start = start
    self.duration = duration
    self.args = args

  @property
  def end(self):
    return self.start + self.duration

  def __repr__(self):
    if self.args:
      args_str = ', ' + repr(self.args)
    else:
      args_str = ''

    return "TimelineEvent(name='%s', start=%f, duration=%s%s)" % (
      self.name,
      self.start,
      self.duration,
      args_str)
