# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import httplib
import json
import re
import socket
import sys
import urllib2

from telemetry.core import exceptions
from telemetry.core import user_agent
from telemetry.core import util
from telemetry.core import web_contents
from telemetry.core import wpr_modes
from telemetry.core import wpr_server
from telemetry.core.backends import browser_backend
from telemetry.core.chrome import extension_dict_backend
from telemetry.core.chrome import misc_web_contents_backend
from telemetry.core.chrome import tab_list_backend
from telemetry.core.chrome import tracing_backend
from telemetry.unittest import options_for_unittests

class ChromeBrowserBackend(browser_backend.BrowserBackend):
  """An abstract class for chrome browser backends. Provides basic functionality
  once a remote-debugger port has been established."""
  # It is OK to have abstract methods. pylint: disable=W0223

  def __init__(self, is_content_shell, supports_extensions, options):
    super(ChromeBrowserBackend, self).__init__(
        is_content_shell=is_content_shell,
        supports_extensions=supports_extensions,
        options=options,
        tab_list_backend=tab_list_backend.TabListBackend)
    self._port = None

    self._inspector_protocol_version = 0
    self._chrome_branch_number = 0
    self._tracing_backend = None

    self.webpagereplay_local_http_port = util.GetAvailableLocalPort()
    self.webpagereplay_local_https_port = util.GetAvailableLocalPort()
    self.webpagereplay_remote_http_port = self.webpagereplay_local_http_port
    self.webpagereplay_remote_https_port = self.webpagereplay_local_https_port

    if options.dont_override_profile and not options_for_unittests.AreSet():
      sys.stderr.write('Warning: Not overriding profile. This can cause '
                       'unexpected effects due to profile-specific settings, '
                       'such as about:flags settings, cookies, and '
                       'extensions.\n')
    self._misc_web_contents_backend = (
        misc_web_contents_backend.MiscWebContentsBackend(self))
    self._extension_dict_backend = None
    if supports_extensions:
      self._extension_dict_backend = (
          extension_dict_backend.ExtensionDictBackend(self))

  def AddReplayServerOptions(self, options):
    options.append('--no-dns_forwarding')

  @property
  def misc_web_contents_backend(self):
    """Access to chrome://oobe/login page which is neither a tab nor an
    extension."""
    return self._misc_web_contents_backend

  @property
  def extension_dict_backend(self):
    return self._extension_dict_backend

  def GetBrowserStartupArgs(self):
    args = []
    args.extend(self.options.extra_browser_args)
    args.append('--disable-background-networking')
    args.append('--metrics-recording-only')
    args.append('--no-first-run')
    if self.options.wpr_mode != wpr_modes.WPR_OFF:
      args.extend(wpr_server.GetChromeFlags(
          self.WEBPAGEREPLAY_HOST,
          self.webpagereplay_remote_http_port,
          self.webpagereplay_remote_https_port))
    args.extend(user_agent.GetChromeUserAgentArgumentFromType(
        self.options.browser_user_agent_type))

    extensions = [extension.local_path for extension in
                  self.options.extensions_to_load if not extension.is_component]
    extension_str = ','.join(extensions)
    if len(extensions) > 0:
      args.append('--load-extension=%s' % extension_str)

    component_extensions = [extension.local_path for extension in
                  self.options.extensions_to_load if extension.is_component]
    component_extension_str = ','.join(component_extensions)
    if len(component_extensions) > 0:
      args.append('--load-component-extension=%s' % component_extension_str)

    if self.options.no_proxy_server:
      args.append('--no-proxy-server')

    return args

  def _WaitForBrowserToComeUp(self, timeout=None):
    def IsBrowserUp():
      try:
        self.Request('', timeout=timeout)
      except (exceptions.BrowserGoneException,
              exceptions.BrowserConnectionGoneException):
        return False
      else:
        return True
    try:
      util.WaitFor(IsBrowserUp, timeout=30)
    except util.TimeoutException:
      raise exceptions.BrowserGoneException(self.GetStackTrace())

    def AllExtensionsLoaded():
      # Extension pages are loaded from an about:blank page,
      # so we need to check that the document URL is the extension
      # page in addition to the ready state.
      extension_ready_js = """
          document.URL.lastIndexOf('chrome-extension://%s/', 0) == 0 &&
          (document.readyState == 'complete' ||
           document.readyState == 'interactive')
      """
      for e in self.options.extensions_to_load:
        if not e.extension_id in self._extension_dict_backend:
          return False
        extension_object = self._extension_dict_backend[e.extension_id]
        res = extension_object.EvaluateJavaScript(
            extension_ready_js % e.extension_id)
        if not res:
          return False
      return True
    if self._supports_extensions:
      util.WaitFor(AllExtensionsLoaded, timeout=30)

  def _PostBrowserStartupInitialization(self):
    # Detect version information.
    data = self.Request('version')
    resp = json.loads(data)
    if 'Protocol-Version' in resp:
      self._inspector_protocol_version = resp['Protocol-Version']

      if 'Browser' in resp:
        branch_number_match = re.search('Chrome/\d+\.\d+\.(\d+)\.\d+',
                                        resp['Browser'])
      else:
        branch_number_match = re.search(
            'Chrome/\d+\.\d+\.(\d+)\.\d+ (Mobile )?Safari',
            resp['User-Agent'])

      if branch_number_match:
        self._chrome_branch_number = int(branch_number_match.group(1))
      else:
        # Content Shell returns '' for Browser, for now we have to
        # fall-back and assume branch 1025.
        self._chrome_branch_number = 1025
      return

    # Detection has failed: assume 18.0.1025.168 ~= Chrome Android.
    self._inspector_protocol_version = 1.0
    self._chrome_branch_number = 1025

  def Request(self, path, timeout=None, throw_network_exception=False):
    url = 'http://localhost:%i/json' % self._port
    if path:
      url += '/' + path
    try:
      req = urllib2.urlopen(url, timeout=timeout)
      return req.read()
    except (socket.error, httplib.BadStatusLine, urllib2.URLError) as e:
      if throw_network_exception:
        raise e
      if not self.IsBrowserRunning():
        raise exceptions.BrowserGoneException()
      raise exceptions.BrowserConnectionGoneException()

  @property
  def browser_directory(self):
    raise NotImplementedError()

  @property
  def profile_directory(self):
    raise NotImplementedError()

  @property
  def chrome_branch_number(self):
    return self._chrome_branch_number

  @property
  def supports_tab_control(self):
    return self._chrome_branch_number >= 1303

  @property
  def supports_tracing(self):
    return self.is_content_shell or self._chrome_branch_number >= 1385

  def StartTracing(self, custom_categories=None,
                   timeout=web_contents.DEFAULT_WEB_CONTENTS_TIMEOUT):
    """ custom_categories is an optional string containing a list of
    comma separated categories that will be traced instead of the
    default category set.  Example: use
    "webkit,cc,disabled-by-default-cc.debug" to trace only those three
    event categories.
    """
    if self._tracing_backend is None:
      self._tracing_backend = tracing_backend.TracingBackend(self._port)
    self._tracing_backend.BeginTracing(custom_categories, timeout)

  def StopTracing(self):
    self._tracing_backend.EndTracing()

  def GetTraceResultAndReset(self):
    return self._tracing_backend.GetTraceResultAndReset()

  def GetProcessName(self, cmd_line):
    """Returns a user-friendly name for the process of the given |cmd_line|."""
    if 'nacl_helper_bootstrap' in cmd_line:
      return 'nacl_helper_bootstrap'
    if ':sandboxed_process' in cmd_line:
      return 'renderer'
    m = re.match(r'.* --type=([^\s]*) .*', cmd_line)
    if not m:
      return 'browser'
    return m.group(1)

  def Close(self):
    if self._tracing_backend:
      self._tracing_backend.Close()
      self._tracing_backend = None
