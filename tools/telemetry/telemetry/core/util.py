# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import inspect
import os
import socket
import sys
import time

class TimeoutException(Exception):
  pass

def GetBaseDir():
  main_module = sys.modules['__main__']
  if hasattr(main_module, '__file__'):
    return os.path.dirname(os.path.abspath(main_module.__file__))
  else:
    return os.getcwd()

def GetTelemetryDir():
  return os.path.normpath(os.path.join(
      __file__, os.pardir, os.pardir, os.pardir))

def GetUnittestDataDir():
  return os.path.join(GetTelemetryDir(), 'unittest_data')

def GetChromiumSrcDir():
  return os.path.normpath(os.path.join(GetTelemetryDir(), os.pardir, os.pardir))

def WaitFor(condition,
            timeout, poll_interval=0.1,
            pass_time_left_to_func=False):
  assert isinstance(condition, type(lambda: None))  # is function
  start_time = time.time()
  while True:
    if pass_time_left_to_func:
      res = condition(max((start_time + timeout) - time.time(), 0.0))
    else:
      res = condition()
    if res:
      break
    if time.time() - start_time > timeout:
      if condition.__name__ == '<lambda>':
        try:
          condition_string = inspect.getsource(condition).strip()
        except IOError:
          condition_string = condition.__name__
      else:
        condition_string = condition.__name__
      raise TimeoutException('Timed out while waiting %ds for %s.' %
                             (timeout, condition_string))
    time.sleep(poll_interval)

def FindElementAndPerformAction(tab, text, callback_code):
  """JavaScript snippet for finding an element with a given text on a page."""
  code = """
      (function() {
        var callback_function = """ + callback_code + """;
        function _findElement(element, text) {
          if (element.innerHTML == text) {
            callback_function
            return element;
          }
          for (var i in element.childNodes) {
            var found = _findElement(element.childNodes[i], text);
            if (found)
              return found;
          }
          return null;
        }
        var _element = _findElement(document, \"""" + text + """\");
        return callback_function(_element);
      })();"""
  return tab.EvaluateJavaScript(code)

class PortPair(object):
  def __init__(self, local_port, remote_port):
    self.local_port = local_port
    self.remote_port = remote_port

def GetAvailableLocalPort():
  tmp = socket.socket()
  tmp.bind(('', 0))
  port = tmp.getsockname()[1]
  tmp.close()

  return port

def CloseConnections(tab):
  """Closes all TCP sockets held open by the browser."""
  try:
    tab.ExecuteJavaScript("""window.chrome && chrome.benchmarking &&
                             chrome.benchmarking.closeConnections()""")
  except Exception:
    pass

def GetBuildDirectories():
  """Yields all combination of Chromium build output directories."""
  build_dirs = ['build',
                'out',
                'sconsbuild',
                'xcodebuild']

  build_types = ['Debug', 'Debug_x64', 'Release', 'Release_x64']

  for build_dir in build_dirs:
    for build_type in build_types:
      yield build_dir, build_type
