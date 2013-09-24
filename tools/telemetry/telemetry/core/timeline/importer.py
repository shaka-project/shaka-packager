# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class TimelineImporter(object):
  """Interface for classes that can add events to
  a timeline model from raw event data."""
  def __init__(self, model, event_data, import_priority=0):
    self._model = model
    self._event_data = event_data
    self.import_priority = import_priority

  @staticmethod
  def CanImport(event_data):
    """Returns true if the importer can process the given event data."""
    raise NotImplementedError

  def ImportEvents(self):
    """Processes the event data and creates and adds
    new timeline events to the model"""
    raise NotImplementedError

  def FinalizeImport(self):
    """Called after all other importers for the model are run."""
    raise NotImplementedError
