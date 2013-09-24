# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

from telemetry.core import util

# Get chrome/test/functional scripts into our path.
# TODO(tonyg): Move webpagereplay.py to a common location.
sys.path.append(
    os.path.abspath(
        os.path.join(os.path.dirname(__file__),
                     '..', '..', '..', '..', 'chrome', 'test', 'functional')))
import webpagereplay  # pylint: disable=F0401

def GetChromeFlags(replay_host, http_port, https_port):
  return webpagereplay.GetChromeFlags(replay_host, http_port, https_port)

class _WebPageReplayServer(webpagereplay.ReplayServer): # pylint: disable=W0232
  def _AddDefaultReplayOptions(self):
    """Override. Because '--no-dns_forwarding' is added by default in parent
       while webdriver-based backends need dns forwarding."""
    self.replay_options += [
        '--port', str(self._http_port),
        '--ssl_port', str(self._https_port),
        '--use_closest_match',
        '--log_level', 'warning'
        ]

class ReplayServer(object):
  def __init__(self, browser_backend, path, is_record_mode, is_append_mode,
               make_javascript_deterministic, webpagereplay_host,
               webpagereplay_local_http_port, webpagereplay_local_https_port,
               webpagereplay_remote_http_port, webpagereplay_remote_https_port):
    self._browser_backend = browser_backend
    self._forwarder = None
    self._web_page_replay = None
    self._is_record_mode = is_record_mode
    self._is_append_mode = is_append_mode
    self._webpagereplay_host = webpagereplay_host
    self._webpagereplay_local_http_port = webpagereplay_local_http_port
    self._webpagereplay_local_https_port = webpagereplay_local_https_port
    self._webpagereplay_remote_http_port = webpagereplay_remote_http_port
    self._webpagereplay_remote_https_port = webpagereplay_remote_https_port

    self._forwarder = browser_backend.CreateForwarder(
        util.PortPair(self._webpagereplay_local_http_port,
                      self._webpagereplay_remote_http_port),
        util.PortPair(self._webpagereplay_local_https_port,
                      self._webpagereplay_remote_https_port))

    options = browser_backend.options.extra_wpr_args
    if self._is_record_mode:
      if self._is_append_mode:
        options.append('--append')
      else:
        options.append('--record')
    if not make_javascript_deterministic:
      options.append('--inject_scripts=')
    browser_backend.AddReplayServerOptions(options)
    self._web_page_replay = _WebPageReplayServer(
        path,
        self._webpagereplay_host,
        self._webpagereplay_local_http_port,
        self._webpagereplay_local_https_port,
        options)
    self._web_page_replay.StartServer()

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def Close(self):
    if self._forwarder:
      self._forwarder.Close()
      self._forwarder = None
    if self._web_page_replay:
      self._web_page_replay.StopServer()
      self._web_page_replay = None
