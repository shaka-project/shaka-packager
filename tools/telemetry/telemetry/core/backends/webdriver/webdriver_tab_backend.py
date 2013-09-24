# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

# TODO(chrisgao): Make png_bitmap sharable for both chrome and webdriver.
from telemetry.core.chrome import png_bitmap

class WebDriverTabBackend(object):
  def __init__(self, browser_backend, window_handle):
    self._browser_backend = browser_backend
    self._window_handle = window_handle

  def Disconnect(self):
    pass

  @property
  def browser(self):
    return self._browser_backend.browser

  @property
  def window_handle(self):
    return self._window_handle

  @property
  def url(self):
    self._browser_backend.driver.switch_to_window(self._window_handle)
    return self._browser_backend.driver.current_url

  def Activate(self):
    # Webdriver doesn't support tab control.
    raise NotImplementedError()

  def Close(self):
    self._browser_backend.driver.switch_to_window(self._window_handle)
    self._browser_backend.driver.close()

  def WaitForDocumentReadyStateToBeComplete(self, timeout=None):
    # TODO(chrisgao): Double check of document state.
    pass

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self, timeout=None):
    # TODO(chrisgao): Double check of document state.
    pass

  @property
  def screenshot_supported(self):
    return True

  def Screenshot(self, timeout=None):  # pylint: disable=W0613
    if timeout:
      logging.warning('timeout is not supported')
    self._browser_backend.driver.switch_to_window(self._window_handle)
    snap = self._browser_backend.driver.get_screenshot_as_base64()
    if snap:
      return png_bitmap.PngBitmap(snap)
    return None

  @property
  def message_output_stream(self):
    # Webdriver has no API for grabbing console messages.
    raise NotImplementedError()

  @message_output_stream.setter
  def message_output_stream(self, stream):
    raise NotImplementedError()

  def GetDOMStats(self, timeout=None):
    # Webdriver has no API for DOM status.
    raise NotImplementedError()

  def PerformActionAndWaitForNavigate(self, action_function, _):
    # TODO(chrisgao): Double check of navigation.
    action_function()

  def Navigate(self, url, script_to_evaluate_on_commit=None, timeout=None):
    if script_to_evaluate_on_commit:
      raise NotImplementedError('script_to_evaluate_on_commit is NOT supported')
    self._browser_backend.driver.switch_to_window(self._window_handle)
    if timeout:
      self._browser_backend.driver.set_page_load_timeout(timeout * 1000)
    self._browser_backend.driver.get(url)

  def GetCookieByName(self, name, timeout=None):
    if timeout:
      logging.warning('timeout is not supported')
    self._browser_backend.driver.switch_to_window(self._window_handle)
    cookie = self._browser_backend.driver.get_cookie(name)
    if cookie:
      return cookie['value']
    return None

  def ExecuteJavaScript(self, expr, timeout=None):
    self._browser_backend.driver.switch_to_window(self._window_handle)
    if timeout:
      logging.warning('timeout is not supported')
    self._browser_backend.driver.execute_script(expr)

  def EvaluateJavaScript(self, expr, timeout=None):
    self._browser_backend.driver.switch_to_window(self._window_handle)
    if timeout:
      logging.warning('timeout is not supported')
    return self._browser_backend.driver.execute_script(
        'return eval(\'%s\')' % expr.replace('\'', '\\\'').replace('\n', ' '))

  @property
  def timeline_model(self):
    # IE/Firefox has no timeline.
    raise NotImplementedError()

  def StartTimelineRecording(self):
    raise NotImplementedError()

  def StopTimelineRecording(self):
    raise NotImplementedError()

  def ClearCache(self):
    # Can't find a way to clear cache of a tab in IE/Firefox.
    raise NotImplementedError()
