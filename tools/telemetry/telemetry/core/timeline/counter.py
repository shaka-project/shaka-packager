# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import telemetry.core.timeline.event_container as event_container

# Doesn't inherit from TimelineEvent because its only a temporary wrapper of a
# counter sample into an event. During stable operation, the samples are stored
# a dense array of values rather than in the long-form done by an Event.
class CounterSample(object):
  def __init__(self, counter, sample_index):
    self._counter = counter
    self._sample_index = sample_index

  @property
  def name(self):
    return None

  @property
  def start(self):
    return self._counter.timestamps[self._sample_index]

  @start.setter
  def start(self, start):
    self._counter.timestamps[self._sample_index] = start

  @property
  def duration(self):
    return 0

  @property
  def end(self):
    return self.start


class Counter(event_container.TimelineEventContainer):
  """ Stores all the samples for a given counter.
  """
  def __init__(self, parent, category, name):
    super(Counter, self).__init__(name, parent)
    self.category = category
    self.full_name  = category + '.' + name
    self.samples = []
    self.timestamps = []
    self.series_names = []
    self.totals = []
    self.max_total = 0

  def IterChildContainers(self):
    return iter([])

  def IterEventsInThisContainer(self):
    for i in range(len(self.timestamps)):
      yield CounterSample(self, i)

  @property
  def num_series(self):
    return len(self.series_names)

  @property
  def num_samples(self):
    return len(self.timestamps)

  def FinalizeImport(self):
    if self.num_series * self.num_samples != len(self.samples):
      raise ValueError(
          'Length of samples must be a multiple of length of timestamps.')

    self.totals = []
    self.max_total = 0
    if not len(self.samples):
      return

    max_total = None
    for i in xrange(self.num_samples):
      total = 0
      for j in xrange(self.num_series):
        total += self.samples[i * self.num_series + j]
        self.totals.append(total)
      if max_total is None or total > max_total:
        max_total = total
    self.max_total = max_total
