# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import profile_creator

class DummyProfileCreator(profile_creator.ProfileCreator):
  def CreateProfile(self):
    pass
