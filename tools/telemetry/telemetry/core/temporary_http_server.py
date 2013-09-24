# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import socket
import subprocess
import sys
import urlparse

from telemetry.core import util

class TemporaryHTTPServer(object):
  def __init__(self, browser_backend, paths):
    self._server = None
    self._devnull = None
    self._paths = paths
    self._forwarder = None
    self._host_port = util.GetAvailableLocalPort()

    for path in self._paths:
      assert os.path.exists(path), path

    self._devnull = open(os.devnull, 'w')
    cmd = [sys.executable, '-m', 'memory_cache_http_server',
           str(self._host_port)]
    cmd.extend(self._paths)
    env = os.environ.copy()
    env['PYTHONPATH'] = os.path.abspath(os.path.dirname(__file__))
    self._server = subprocess.Popen(cmd, cwd=os.path.commonprefix(self._paths),
        env=env, stdout=self._devnull, stderr=self._devnull)

    self._forwarder = browser_backend.CreateForwarder(
        util.PortPair(self._host_port,
                      browser_backend.GetRemotePort(self._host_port)))

    def IsServerUp():
      return not socket.socket().connect_ex(('localhost', self._host_port))
    util.WaitFor(IsServerUp, 10)

  @property
  def paths(self):
    return self._paths

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  def __del__(self):
    self.Close()

  def Close(self):
    if self._forwarder:
      self._forwarder.Close()
      self._forwarder = None
    if self._server:
      self._server.kill()
      self._server = None
    if self._devnull:
      self._devnull.close()
      self._devnull = None

  @property
  def url(self):
    return self._forwarder.url

  def UrlOf(self, path):
    return urlparse.urljoin(self.url, path)
