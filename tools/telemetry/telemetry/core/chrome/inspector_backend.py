# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import socket
import sys

from telemetry.core import util
from telemetry.core import exceptions
from telemetry.core.chrome import inspector_console
from telemetry.core.chrome import inspector_memory
from telemetry.core.chrome import inspector_network
from telemetry.core.chrome import inspector_page
from telemetry.core.chrome import inspector_runtime
from telemetry.core.chrome import inspector_timeline
from telemetry.core.chrome import png_bitmap
from telemetry.core.chrome import websocket

class InspectorException(Exception):
  pass

class InspectorBackend(object):
  def __init__(self, browser, browser_backend, debugger_url):
    assert debugger_url
    self._browser = browser
    self._browser_backend = browser_backend
    self._debugger_url = debugger_url
    self._socket = None
    self._domain_handlers = {}
    self._cur_socket_timeout = 0
    self._next_request_id = 0

    self._console = inspector_console.InspectorConsole(self)
    self._memory = inspector_memory.InspectorMemory(self)
    self._page = inspector_page.InspectorPage(self)
    self._runtime = inspector_runtime.InspectorRuntime(self)
    self._timeline = inspector_timeline.InspectorTimeline(self)
    self._network = inspector_network.InspectorNetwork(self)

  def __del__(self):
    self.Disconnect()

  def _Connect(self):
    if self._socket:
      return
    try:
      self._socket = websocket.create_connection(self._debugger_url)
    except (websocket.WebSocketException):
      if self._browser_backend.IsBrowserRunning():
        raise exceptions.TabCrashException(sys.exc_info()[1])
      else:
        raise exceptions.BrowserGoneException()

    self._cur_socket_timeout = 0
    self._next_request_id = 0

  def Disconnect(self):
    for _, handlers in self._domain_handlers.items():
      _, will_close_handler = handlers
      will_close_handler()
    self._domain_handlers = {}

    if self._socket:
      self._socket.close()
      self._socket = None

  # General public methods.

  @property
  def browser(self):
    return self._browser

  @property
  def url(self):
    self.Disconnect()
    return self._browser_backend.tab_list_backend.GetTabUrl(self._debugger_url)

  def Activate(self):
    self._Connect()
    self._browser_backend.tab_list_backend.ActivateTab(self._debugger_url)

  def Close(self):
    self.Disconnect()
    self._browser_backend.tab_list_backend.CloseTab(self._debugger_url)

  # Public methods implemented in JavaScript.

  def WaitForDocumentReadyStateToBeComplete(self, timeout):
    util.WaitFor(
        lambda: self._runtime.Evaluate('document.readyState') == 'complete',
        timeout)

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(
      self, timeout):
    def IsReadyStateInteractiveOrBetter():
      rs = self._runtime.Evaluate('document.readyState')
      return rs == 'complete' or rs == 'interactive'
    util.WaitFor(IsReadyStateInteractiveOrBetter, timeout)

  @property
  def screenshot_supported(self):
    if self._runtime.Evaluate(
        'window.chrome.gpuBenchmarking === undefined'):
      return False

    if self._runtime.Evaluate(
        'window.chrome.gpuBenchmarking.beginWindowSnapshotPNG === undefined'):
      return False

    return self._browser_backend.chrome_branch_number >= 1391

  def Screenshot(self, timeout):
    if self._runtime.Evaluate(
        'window.chrome.gpuBenchmarking === undefined'):
      raise Exception("Browser was not started with --enable-gpu-benchmarking")

    if self._runtime.Evaluate(
        'window.chrome.gpuBenchmarking.beginWindowSnapshotPNG === undefined'):
      raise Exception("Browser does not support window snapshot API.")

    self._runtime.Evaluate("""
        if(!window.__telemetry) {
          window.__telemetry = {}
        }
        window.__telemetry.snapshotComplete = false;
        window.__telemetry.snapshotData = null;
        window.chrome.gpuBenchmarking.beginWindowSnapshotPNG(
          function(snapshot) {
            window.__telemetry.snapshotData = snapshot;
            window.__telemetry.snapshotComplete = true;
          }
        );
    """)

    def IsSnapshotComplete():
      return self._runtime.Evaluate('window.__telemetry.snapshotComplete')

    util.WaitFor(IsSnapshotComplete, timeout)

    snap = self._runtime.Evaluate("""
      (function() {
        var data = window.__telemetry.snapshotData;
        delete window.__telemetry.snapshotComplete;
        delete window.__telemetry.snapshotData;
        return data;
      })()
    """)
    if snap:
      return png_bitmap.PngBitmap(snap['data'])
    return None

  # Console public methods.

  @property
  def message_output_stream(self):  # pylint: disable=E0202
    return self._console.message_output_stream

  @message_output_stream.setter
  def message_output_stream(self, stream):  # pylint: disable=E0202
    self._console.message_output_stream = stream

  # Memory public methods.

  def GetDOMStats(self, timeout):
    dom_counters = self._memory.GetDOMCounters(timeout)
    return {
      'document_count': dom_counters['documents'],
      'node_count': dom_counters['nodes'],
      'event_listener_count': dom_counters['jsEventListeners']
    }

  # Page public methods.

  def PerformActionAndWaitForNavigate(self, action_function, timeout):
    self._page.PerformActionAndWaitForNavigate(action_function, timeout)

  def Navigate(self, url, script_to_evaluate_on_commit, timeout):
    self._page.Navigate(url, script_to_evaluate_on_commit, timeout)

  def GetCookieByName(self, name, timeout):
    return self._page.GetCookieByName(name, timeout)

  # Runtime public methods.

  def ExecuteJavaScript(self, expr, timeout):
    self._runtime.Execute(expr, timeout)

  def EvaluateJavaScript(self, expr, timeout):
    return self._runtime.Evaluate(expr, timeout)

  # Timeline public methods.

  @property
  def timeline_model(self):
    return self._timeline.timeline_model

  def StartTimelineRecording(self):
    self._timeline.Start()

  def StopTimelineRecording(self):
    self._timeline.Stop()

  # Network public methods.

  def ClearCache(self):
    self._network.ClearCache()

  # Methods used internally by other backends.

  def DispatchNotifications(self, timeout=10):
    self._Connect()
    self._SetTimeout(timeout)

    try:
      data = self._socket.recv()
    except (socket.error, websocket.WebSocketException):
      if self._browser_backend.tab_list_backend.DoesDebuggerUrlExist(
          self._debugger_url):
        return
      raise exceptions.TabCrashException(sys.exc_info()[1])

    res = json.loads(data)
    logging.debug('got [%s]', data)
    if 'method' in res:
      self._HandleNotification(res)

  def _HandleNotification(self, res):
    if (res['method'] == 'Inspector.detached' and
        res.get('params', {}).get('reason','') == 'replaced_with_devtools'):
      self._WaitForInspectorToGoAwayAndReconnect()
      return
    if res['method'] == 'Inspector.targetCrashed':
      raise exceptions.TabCrashException()

    mname = res['method']
    dot_pos = mname.find('.')
    domain_name = mname[:dot_pos]
    if domain_name in self._domain_handlers:
      try:
        self._domain_handlers[domain_name][0](res)
      except Exception:
        import traceback
        traceback.print_exc()
    else:
      logging.debug('Unhandled inspector message: %s', res)

  def SendAndIgnoreResponse(self, req):
    self._Connect()
    req['id'] = self._next_request_id
    self._next_request_id += 1
    data = json.dumps(req)
    self._socket.send(data)
    logging.debug('sent [%s]', data)

  def _SetTimeout(self, timeout):
    if self._cur_socket_timeout != timeout:
      self._socket.settimeout(timeout)
      self._cur_socket_timeout = timeout

  def _WaitForInspectorToGoAwayAndReconnect(self):
    sys.stderr.write('The connection to Chrome was lost to the Inspector UI.\n')
    sys.stderr.write('Telemetry is waiting for the inspector to be closed...\n')
    self._socket.close()
    self._socket = None
    def IsBack():
      return self._browser_backend.tab_list_backend.DoesDebuggerUrlExist(
        self._debugger_url)
    util.WaitFor(IsBack, 512, 0.5)
    sys.stderr.write('\n')
    sys.stderr.write('Inspector\'s UI closed. Telemetry will now resume.\n')
    self._Connect()

  def SyncRequest(self, req, timeout=10):
    self._Connect()
    # TODO(nduca): Listen to the timeout argument
    # pylint: disable=W0613
    self._SetTimeout(timeout)
    self.SendAndIgnoreResponse(req)

    while True:
      try:
        data = self._socket.recv()
      except (socket.error, websocket.WebSocketException):
        if self._browser_backend.tab_list_backend.DoesDebuggerUrlExist(
            self._debugger_url):
          raise util.TimeoutException(
            'Timed out waiting for reply. This is unusual.')
        raise exceptions.TabCrashException(sys.exc_info()[1])

      res = json.loads(data)
      logging.debug('got [%s]', data)
      if 'method' in res:
        self._HandleNotification(res)
        continue

      if res['id'] != req['id']:
        logging.debug('Dropped reply: %s', json.dumps(res))
        continue
      return res

  def RegisterDomain(self,
      domain_name, notification_handler, will_close_handler):
    """Registers a given domain for handling notification methods.

    For example, given inspector_backend:
       def OnConsoleNotification(msg):
          if msg['method'] == 'Console.messageAdded':
             print msg['params']['message']
          return
       def OnConsoleClose(self):
          pass
       inspector_backend.RegisterDomain('Console',
                                        OnConsoleNotification, OnConsoleClose)
       """
    assert domain_name not in self._domain_handlers
    self._domain_handlers[domain_name] = (notification_handler,
                                          will_close_handler)

  def UnregisterDomain(self, domain_name):
    """Unregisters a previously registered domain."""
    assert domain_name in self._domain_handlers
    self._domain_handlers.pop(domain_name)

  def CollectGarbage(self):
    self._page.CollectGarbage()
