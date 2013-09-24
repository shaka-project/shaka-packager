# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json

from telemetry.core import web_contents
from telemetry.core.chrome import inspector_backend

class MiscWebContentsBackend(object):
  """Provides acccess to chrome://oobe/login page, which is neither an extension
  nor a tab."""
  def __init__(self, browser_backend):
    self._browser_backend = browser_backend

  def GetOobe(self):
    oobe_web_contents_info = self._FindWebContentsInfo()
    if oobe_web_contents_info:
      debugger_url = oobe_web_contents_info.get('webSocketDebuggerUrl')
      if debugger_url:
        inspector = self._CreateInspectorBackend(debugger_url)
        return web_contents.WebContents(inspector)
    return None

  def _CreateInspectorBackend(self, debugger_url):
    return inspector_backend.InspectorBackend(self._browser_backend.browser,
                                              self._browser_backend,
                                              debugger_url)

  def _ListWebContents(self, timeout=None):
    data = self._browser_backend.Request('', timeout=timeout)
    return json.loads(data)

  def _FindWebContentsInfo(self):
    for web_contents_info in self._ListWebContents():
      # Prior to crrev.com/203152, url was chrome://oobe/login.
      if (web_contents_info.get('url').startswith('chrome://oobe')):
        return web_contents_info
    return None
