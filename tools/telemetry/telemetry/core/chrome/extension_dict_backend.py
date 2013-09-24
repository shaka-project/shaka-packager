# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import re
import weakref

from telemetry.core import extension_page
from telemetry.core.chrome import inspector_backend

class ExtensionNotFoundException(Exception):
  pass

class ExtensionDictBackend(object):
  def __init__(self, browser_backend):
    self._browser_backend = browser_backend
    # Maps extension ids to ExtensionPage objects.
    self._extension_dict = weakref.WeakValueDictionary()

  def __getitem__(self, extension_id):
    extension_object = self._extension_dict.get(extension_id)
    if not extension_object:
      extension_object = self._CreateExtensionObject(extension_id)
      assert extension_object
      self._extension_dict[extension_id] = extension_object
    return extension_object

  def __contains__(self, extension_id):
    return extension_id in self._GetExtensionIds()

  @staticmethod
  def _ExtractExtensionId(url):
    m = re.match(r"(chrome-extension://)([^/]+)", url)
    assert m
    return m.group(2)

  @staticmethod
  def _GetExtensionId(extension_info):
    if 'url' not in extension_info:
      return None
    return ExtensionDictBackend._ExtractExtensionId(extension_info['url'])

  def _CreateExtensionObject(self, extension_id):
    extension_info = self._FindExtensionInfo(extension_id)
    if not extension_info or not 'webSocketDebuggerUrl' in extension_info:
      raise ExtensionNotFoundException()
    return extension_page.ExtensionPage(
        self._CreateInspectorBackendForDebuggerUrl(
            extension_info['webSocketDebuggerUrl']))

  def _CreateInspectorBackendForDebuggerUrl(self, debugger_url):
    return inspector_backend.InspectorBackend(self._browser_backend.browser,
                                              self._browser_backend,
                                              debugger_url)

  def _FindExtensionInfo(self, extension_id):
    for extension_info in self._GetExtensionInfoList():
      if self._GetExtensionId(extension_info) == extension_id:
        return extension_info
    return None

  def _GetExtensionInfoList(self, timeout=None):
    data = self._browser_backend.Request('', timeout=timeout)
    return self._FilterExtensions(json.loads(data))

  def _FilterExtensions(self, all_pages):
    return [page_info for page_info in all_pages
            if page_info['url'].startswith('chrome-extension://')]

  def _GetExtensionIds(self):
    return map(self._GetExtensionId, self._GetExtensionInfoList())
