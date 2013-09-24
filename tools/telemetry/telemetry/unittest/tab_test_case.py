# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core import browser_finder
from telemetry.unittest import options_for_unittests

class TabTestCase(unittest.TestCase):
  def __init__(self, *args):
    self._extra_browser_args = []
    super(TabTestCase, self).__init__(*args)

  def setUp(self):
    self._browser = None
    self._tab = None
    options = options_for_unittests.GetCopy()

    self.CustomizeBrowserOptions(options)

    if self._extra_browser_args:
      for arg in self._extra_browser_args:
        options.extra_browser_args.append(arg)

    browser_to_create = browser_finder.FindBrowser(options)
    if not browser_to_create:
      raise Exception('No browser found, cannot continue test.')
    try:
      self._browser = browser_to_create.Create()
      self._browser.Start()
      self._tab = self._browser.tabs[0]
    except:
      self.tearDown()
      raise

  def tearDown(self):
    if self._tab:
      self._tab.Disconnect()
    if self._browser:
      self._browser.Close()

  def CustomizeBrowserOptions(self, options):
    """Override to add test-specific options to the BrowserOptions object"""
    pass
