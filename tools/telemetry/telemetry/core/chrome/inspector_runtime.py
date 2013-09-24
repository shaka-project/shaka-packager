# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import exceptions

class InspectorRuntime(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self._inspector_backend.RegisterDomain(
        'Runtime',
        self._OnNotification,
        self._OnClose)

  def _OnNotification(self, msg):
    pass

  def _OnClose(self):
    pass

  def Execute(self, expr, timeout=60):
    """Executes expr in javascript. Does not return the result.

    If the expression failed to evaluate, EvaluateException will be raised.
    """
    self.Evaluate(expr + '; 0;', timeout)

  def Evaluate(self, expr, timeout=60):
    """Evalutes expr in javascript and returns the JSONized result.

    Consider using Execute for cases where the result of the expression is not
    needed.

    If evaluation throws in javascript, a python EvaluateException will
    be raised.

    If the result of the evaluation cannot be JSONized, then an
    EvaluationException will be raised.
    """
    request = {
      'method': 'Runtime.evaluate',
      'params': {
        'expression': expr,
        'returnByValue': True
        }
      }
    res = self._inspector_backend.SyncRequest(request, timeout)
    if 'error' in res:
      raise exceptions.EvaluateException(res['error']['message'])

    if 'wasThrown' in res['result'] and res['result']['wasThrown']:
      # TODO(nduca): propagate stacks from javascript up to the python
      # exception.
      raise exceptions.EvaluateException(res['result']['result']['description'])
    if res['result']['result']['type'] == 'undefined':
      return None
    return res['result']['result']['value']
