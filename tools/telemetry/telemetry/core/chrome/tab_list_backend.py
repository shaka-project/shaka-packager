# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import urllib2
import weakref

from telemetry.core import util
from telemetry.core import tab
from telemetry.core.chrome import inspector_backend

class TabListBackend(object):
  def __init__(self, browser_backend):
    self._browser_backend = browser_backend

    # Stores web socket debugger URLs in iteration order.
    self._tab_list = []
    # Maps debugger URLs to Tab objects.
    self._tab_dict = weakref.WeakValueDictionary()

  def Init(self):
    self._UpdateTabList()

  def New(self, timeout):
    assert self._browser_backend.supports_tab_control

    self._browser_backend.Request('new', timeout=timeout)
    return self[-1]

  def DoesDebuggerUrlExist(self, debugger_url):
    return self._FindTabInfo(debugger_url) is not None

  def CloseTab(self, debugger_url, timeout=None):
    assert self._browser_backend.supports_tab_control

    # TODO(dtu): crbug.com/160946, allow closing the last tab on some platforms.
    # For now, just create a new tab before closing the last tab.
    if len(self) <= 1:
      self.New(timeout)

    tab_id = debugger_url.split('/')[-1]
    try:
      response = self._browser_backend.Request('close/%s' % tab_id,
                                               timeout=timeout,
                                               throw_network_exception=True)
    except urllib2.HTTPError:
      raise Exception('Unable to close tab, tab id not found: %s' % tab_id)
    assert response == 'Target is closing'

    util.WaitFor(lambda: not self._FindTabInfo(debugger_url), timeout=5)

    if debugger_url in self._tab_dict:
      del self._tab_dict[debugger_url]
    self._UpdateTabList()

  def ActivateTab(self, debugger_url, timeout=None):
    assert self._browser_backend.supports_tab_control

    assert debugger_url in self._tab_dict
    tab_id = debugger_url.split('/')[-1]
    try:
      response = self._browser_backend.Request('activate/%s' % tab_id,
                                               timeout=timeout,
                                               throw_network_exception=True)
    except urllib2.HTTPError:
      raise Exception('Unable to activate tab, tab id not found: %s' % tab_id)
    assert response == 'Target activated'

  def GetTabUrl(self, debugger_url):
    tab_info = self._FindTabInfo(debugger_url)
    # TODO(hartmanng): crbug.com/166886 (uncomment the following assert and
    # remove the extra None check when _ListTabs is fixed):
    # assert tab_info is not None
    return tab_info['url'] if tab_info else None

  def __iter__(self):
    self._UpdateTabList()
    return self._tab_list.__iter__()

  def __len__(self):
    self._UpdateTabList()
    return len(self._tab_list)

  def Get(self, index, ret):
    """Returns self[index] if it exists, or ret if index is out of bounds.
    """
    self._UpdateTabList()
    if len(self._tab_list) <= index:
      return ret
    debugger_url = self._tab_list[index]
    # Lazily get/create a Tab object.
    tab_object = self._tab_dict.get(debugger_url)
    if not tab_object:
      backend = inspector_backend.InspectorBackend(
          self._browser_backend.browser,
          self._browser_backend,
          debugger_url)
      tab_object = tab.Tab(backend)
      self._tab_dict[debugger_url] = tab_object
    return tab_object

  def __getitem__(self, index):
    tab_object = self.Get(index, None)
    if tab_object is None:
      raise IndexError('list index out of range')
    return tab_object

  def _ListTabs(self, timeout=None):
    def _IsTab(context):
      if 'type' in context:
        return context['type'] == 'page'
      # TODO: For compatibility with Chrome before r177683.
      # This check is not completely correct, see crbug.com/190592.
      return not context['url'].startswith('chrome-extension://')
    data = self._browser_backend.Request('', timeout=timeout)
    all_contexts = json.loads(data)
    tabs = filter(_IsTab, all_contexts)
    return tabs

  def _UpdateTabList(self):
    def GetDebuggerUrl(tab_info):
      if 'webSocketDebuggerUrl' not in tab_info:
        return None
      return tab_info['webSocketDebuggerUrl']
    new_tab_list = map(GetDebuggerUrl, self._ListTabs())
    self._tab_list = [t for t in self._tab_list
                      if t in self._tab_dict or t in new_tab_list]
    self._tab_list += [t for t in new_tab_list
                       if t is not None and t not in self._tab_list]

  def _FindTabInfo(self, debugger_url):
    for tab_info in self._ListTabs():
      if tab_info.get('webSocketDebuggerUrl') == debugger_url:
        return tab_info
    return None
