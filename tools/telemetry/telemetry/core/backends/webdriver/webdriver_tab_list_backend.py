# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.backends.webdriver import webdriver_tab_backend

class WebDriverTabListBackend(object):
  def __init__(self, browser_backend):
    self._browser_backend = browser_backend
    # Stores the window handles.
    self._tab_list = []

  def Init(self):
    self._UpdateTabList()

  def New(self, timeout=None):
    # Webdriver API doesn't support tab controlling.
    raise NotImplementedError()

  def __iter__(self):
    self._UpdateTabList()
    return self._tab_list.__iter__()

  def __len__(self):
    self._UpdateTabList()
    return len(self._tab_list)

  def __getitem__(self, index):
    self._UpdateTabList()
    if len(self._tab_list) <= index:
      raise IndexError('list index out of range')
    return self._tab_list[index]

  def _UpdateTabList(self):
    window_handles = self._browser_backend.driver.window_handles
    old_tab_list = self._tab_list
    self._tab_list = []
    for window_handle in window_handles:
      tab = None
      for old_tab in old_tab_list:
        if old_tab.window_handle == window_handle:
          tab = old_tab
          break
      else:
        tab = webdriver_tab_backend.WebDriverTabBackend(
            self._browser_backend, window_handle)
      self._tab_list.append(tab)
