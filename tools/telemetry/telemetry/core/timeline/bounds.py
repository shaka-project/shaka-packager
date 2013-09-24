# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Bounds(object):
  """Represents a min-max bounds."""
  def __init__(self):
    self.is_empty_ = True
    self.min_ = None
    self.max_ = None

  @property
  def is_empty(self):
    return self.is_empty_

  @property
  def min(self):
    if self.is_empty_:
      return None
    return self.min_

  @property
  def max(self):
    if self.is_empty_:
      return None
    return self.max_

  @property
  def bounds(self):
    if self.is_empty_:
      return None
    return self.max_ - self.min_

  @property
  def center(self):
    return (self.min_ + self.max_) * 0.5


  def Reset(self):
    self.is_empty_ = True
    self.min_ = None
    self.max_ = None

  def AddBounds(self, bounds):
    if bounds.isEmpty:
      return
    self.AddValue(bounds.min_)
    self.AddValue(bounds.max_)

  def AddValue(self, value):
    if self.is_empty_:
      self.max_ = value
      self.min_ = value
      self.is_empty_ = False
      return

    self.max_ = max(self.max_, value)
    self.min_ = min(self.min_, value)

  @staticmethod
  def CompareByMinTimes(a, b):
    if not a.is_empty and not b.is_empty:
      return a.min_ - b.min_

    if a.is_empty and not b.is_empty:
      return -1

    if not a.is_empty and b.is_empty:
      return 1

    return 0
