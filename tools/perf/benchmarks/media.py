# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import media
from telemetry import test


class Media(test.Test):
  """Obtains media metrics for key user scenarios."""
  test = media.Media
  page_set = 'page_sets/tough_video_cases.json'
