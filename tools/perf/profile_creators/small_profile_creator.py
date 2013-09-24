# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import profile_creator
from telemetry.page import page_set

class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.
  """

  def CreateProfile(self):
    top_25 = os.path.join(os.path.dirname(__file__),
                          '..', 'page_sets', 'top_25.json')
    pages_to_load = page_set.PageSet.FromFile(top_25)
    tab = self._browser.tabs[0]
    for page in pages_to_load:
      tab.Navigate(page.url)
      tab.WaitForDocumentReadyStateToBeComplete()
    tab.Disconnect()
