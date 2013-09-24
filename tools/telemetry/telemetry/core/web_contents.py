# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEFAULT_WEB_CONTENTS_TIMEOUT = 60

# TODO(achuith, dtu, nduca): Add unit tests specifically for WebContents,
# independent of Tab.
class WebContents(object):
  """Represents web contents in the browser"""
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend

  def __del__(self):
    self.Disconnect()

  def Disconnect(self):
    self._inspector_backend.Disconnect()

  def Close(self):
    """Closes this page.

    Not all browsers or browser versions support this method.
    Be sure to check browser.supports_tab_control."""
    self._inspector_backend.Close()

  def WaitForDocumentReadyStateToBeComplete(self,
      timeout=DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._inspector_backend.WaitForDocumentReadyStateToBeComplete(timeout)

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self,
      timeout=DEFAULT_WEB_CONTENTS_TIMEOUT):
    self._inspector_backend.WaitForDocumentReadyStateToBeInteractiveOrBetter(
        timeout)

  def ExecuteJavaScript(self, expr, timeout=DEFAULT_WEB_CONTENTS_TIMEOUT):
    """Executes expr in JavaScript. Does not return the result.

    If the expression failed to evaluate, EvaluateException will be raised.
    """
    self._inspector_backend.ExecuteJavaScript(expr, timeout)

  def EvaluateJavaScript(self, expr, timeout=DEFAULT_WEB_CONTENTS_TIMEOUT):
    """Evalutes expr in JavaScript and returns the JSONized result.

    Consider using ExecuteJavaScript for cases where the result of the
    expression is not needed.

    If evaluation throws in JavaScript, a Python EvaluateException will
    be raised.

    If the result of the evaluation cannot be JSONized, then an
    EvaluationException will be raised.
    """
    return self._inspector_backend.EvaluateJavaScript(expr, timeout)

  @property
  def message_output_stream(self):
    return self._inspector_backend.message_output_stream

  @message_output_stream.setter
  def message_output_stream(self, stream):
    self._inspector_backend.message_output_stream = stream

  @property
  def timeline_model(self):
    return self._inspector_backend.timeline_model

  def StartTimelineRecording(self):
    self._inspector_backend.StartTimelineRecording()

  def StopTimelineRecording(self):
    self._inspector_backend.StopTimelineRecording()
