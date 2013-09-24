# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class ProfileCreator(object):
  """Base class for an object that constructs a Chrome profile."""

  def __init__(self, browser):
    self._browser = browser

  def CreateProfile(self):
    """Fill in the profile in question."""
    raise NotImplementedError
